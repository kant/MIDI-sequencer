/*
* faust_ble_midi V1, based on:
* - faust_mqtt_tcp6_nb_v5
* - esp_nimble_client_V2
*
* MQTT and WIFI functionality to be removed later
CURRENTLY MIDI COMES IN OVER MQTT
 
D (5868) MQTT_CLIENT: deliver_publish: msg_topic_len=16
D (5878) MQTT_CLIENT: Get data len= 21, topic len=16, total_data: 21 offset: 0
D (5888) MQTT_CLIENT: mqtt_message_receive: first byte: 0x30
D (5888) MQTT_CLIENT: mqtt_message_receive: read "remaining length" byte: 0x27
D (5898) MQTT_CLIENT: mqtt_message_receive: total message length: 41 (already read: 2)
D (5908) MQTT_CLIENT: mqtt_message_receive: read_len=39
D (5908) MQTT_CLIENT: mqtt_message_receive: transport_read():41 41
D (5918) MQTT_CLIENT: msg_type=3, msg_id=0
D (5918) MQTT_CLIENT: deliver_publish, message_length_read=41, message_length=41
D (5928) MQTT_CLIENT: deliver_publish: msg_topic_len=16
D (5938) MQTT_CLIENT: Get data len= 21, topic len=16, total_data: 21 offset: 0
*/


/* MQTT (over TCP) Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

//#define FCKX_SEQUENCER_API
#define MIDICTRL 1  //to enable compilation of midi functionality

//#define SYSEX_START_N 0x7b  //added by FCKX
//#define SYSEX_END 0x05

/*
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
//#include "protocol_examples_common.h"

*/

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h" //for using software timers, NOT required for the nbDelay (?)

#include "freertos/event_groups.h"

#include "esp_spi_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#include "WM8978.h"
#include "DspFaust.h"
#include "secrets.h"

//#include "fckx_sequencer.h" //is this required???????
#include "midi.h"         //FCKX
#include "jdksmidi/msg.h" //FCKX
#include "jdksmidi/track.h" //FCKX
#include "queue.h"



#define MAX_RTTL_LENGTH 1024

#define DSP_MACHINE "sawtooth_synth"




extern "C" {           //FCKX
    void app_main(void);
}


extern "C" {           //FCKX
    static esp_mqtt_client * mqtt_app_start(void);
}

extern "C" {           //FCKX
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event);
}

/*

extern "C" {
    static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data);
}

extern "C" {
    void wifi_init_sta(void);
}

*/


/*
//software timer parameters. See example in: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html#timer-api
#define NUM_TIMERS 1

// An array to hold handles to the created timers.
TimerHandle_t xTimers[ NUM_TIMERS ];

// An array to hold a count of the number of times each timer expires.
int32_t lExpireCounters[ NUM_TIMERS ] = { 0 };

//timer callback to be defined just before main (OR should it be just before the sequencer procedure that uses it?

*/


//definition of some global variables
//later, encapsulte these in classes

#define TEMPO 120
#define TIMESIG_NUM 4
#define TIMESIG_DENOM 4

DspFaust* DSP;
bool expired;
bool metronome_keyOn_expired;
bool metronome_keyOff_expired;
bool beat_expired;
int beatCount;
int measureCount;
int loopCount;
int loopLength;
unsigned long loopStart;
int myCounter;
float tempo_scale;
//bool metronomeOn = true;
int timesig_num; 
int timesig_denom;
TimerHandle_t beatTimer;

float tempo = TEMPO;

int loopDuration;
int beatDuration;
int measureDuration;


//buffer for .MsgToText()
//char messageText[64];

uintptr_t metronomeVoiceAddress;


/**********************************************************
//CODE imported from esp_nimble_client_V2
***********************************************************/
/**
 * A BLE client example that is rich in capabilities.
 * There is a lot new capabilities implemented.
 * author unknown
 * updated by chegewara
 * updated for NimBLE by H2zero
 */
 
/** NimBLE differences highlighted in comment blocks **/

/*******original********
#include "BLEDevice.h"
***********************/
#include "NimBLEDevice.h"

//extern "C"{void app_main(void);}
 
// The remote service we wish to connect to.
static BLEUUID serviceUUID("4fafc201-1fb5-459e-8fcc-c5c9c331914b");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("beb5483e-36e1-4688-b7f5-ea07361b26a8");

static bool doConnect = false;
static bool connected = false;
static bool doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

static void notifyCallback(
                              BLERemoteCharacteristic* pBLERemoteCharacteristic,
                              uint8_t* pData,
                              size_t length,
                              bool isNotify) {
    static const char *TAG = "ble_notify_midi";
 /*   printf("Notify callback for characteristic %s of data length %d data: 0x%x 0x%x 0x%x 0x%x 0x%x\n",

    //   printf("Notify callback for characteristic %s of data length %d data: 0x%x\n",
        //  printf("Notify callback for characteristic %s of data length %d data: %u\n", //FCKX
   // printf("Notify callback for characteristic %s of data length %d data: %s\n",
           pBLERemoteCharacteristic->getUUID().toString().c_str(),
           length,
           //(char*)pData);
         //pData.toString().c_str()); //FCKX
          // pData); //FCKX
         // pData[0]);  
         pData[0], pData[1],pData[2],pData[3],pData[4]); 

*/         
         //see MIDI BLE specification for actual data structures (rp52public.pdf)
//play midi immediately for testing
//aDSP->propagateMidi(count, time, type, channel, data1, data2);

/*  //for 5 bytes messages
    DSP->propagateMidi(3, 0, pData[2], 0, pData[3], pData[3]); 
    //ESP_LOGI(TAG,"NUMERICAL VALUE:%lu ", mididata);
    ESP_LOGE(TAG,"HEADER: %u (0x%X)", pData[0], pData[0]);
    ESP_LOGE(TAG,"TIMESTAMP: %u (0x%X)", pData[1], pData[1]);
    ESP_LOGE(TAG,"STATUS: %u (0x%X)", pData[2], pData[2]);
    ESP_LOGE(TAG,"DATA1: %u (0x%X)", pData[3], pData[3]);
    ESP_LOGE(TAG,"DATA2: %u (0x%X)", pData[4], pData[4]) ; //integers are 32 bits!!!!             
*/
    //for 3 bytes messages
    DSP->propagateMidi(3, 0, pData[0]& 0xf0, 0, pData[1], pData[2]);
   //  DSP->propagateMidi(3, 0, pData[0], 0, pData[1], pData[1]);  
    //ESP_LOGE(TAG,"HEADER: %u (0x%X)", pData[0], pData[0]);
    //ESP_LOGE(TAG,"TIMESTAMP: %u (0x%X)", pData[1], pData[1]);    
    ESP_LOGE(TAG,"BYTE0: %u (0x%X)", pData[0], pData[0]);
    ESP_LOGE(TAG,"BYTE1: %u (0x%X)", pData[1], pData[1]);
    ESP_LOGE(TAG,"BYTE2: %u (0x%X)", pData[2], pData[2]) ; //integers are 32 bits!!!!
    ESP_LOGE(TAG,"STATUS: %u (0x%X)",pData[0] & 0xf0, pData[0] & 0xf0);
    ESP_LOGE(TAG,"CHANNEL: %u (0x%X)",pData[0] & 0x0f, pData[0] & 0x0f);
    ESP_LOGE(TAG,"PITCH: %u (0x%X)", pData[1], pData[1]);
    ESP_LOGE(TAG,"VELOCITY: %u (0x%X)", pData[2], pData[2]) ; //integers are 32 bits!!!!     
}

/**  None of these are required as they will be handled by the library with defaults. **
 **                       Remove as you see fit for your needs                        */  
class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
    printf("onDisconnect");
  }
/***************** New - Security handled here ********************
****** Note: these are the same return values as defaults ********/
  uint32_t onPassKeyRequest(){
    printf("Client PassKeyRequest\n");
    return 123456; 
  }
  bool onConfirmPIN(uint32_t pass_key){
    printf("The passkey YES/NO number: %d\n", pass_key);
    return true; 
  }

  void onAuthenticationComplete(ble_gap_conn_desc desc){
    printf("Starting BLE work!\n");
  }
/*******************************************************************/
};

bool connectToServer() {
    printf("Forming a connection to %s\n", myDevice->getAddress().toString().c_str());

    BLEClient*  pClient  = BLEDevice::createClient();
    printf(" - Created client\n");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(myDevice);  // if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    printf(" - Connected to server\n");

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      printf("Failed to find our service UUID: %s\n", serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    printf(" - Found our service\n");


    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      printf("Failed to find our characteristic UUID: %s\n", charUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    printf(" - Found our characteristic\n");

    // Read the value of the characteristic.
    if(pRemoteCharacteristic->canRead()) {
      std::string value = pRemoteCharacteristic->readValue();
      printf("The characteristic value was: %s\n", value.c_str());
      }
    /** registerForNotify() has been deprecated and replaced with subscribe() / unsubscribe().
     *  Subscribe parameter defaults are: notifications=true, notifyCallback=nullptr, response=false.
     *  Unsubscribe parameter defaults are: response=false. 
     */
    if(pRemoteCharacteristic->canNotify()) {
        //pRemoteCharacteristic->registerForNotify(notifyCallback);
        pRemoteCharacteristic->subscribe(true, notifyCallback);
    }

    connected = true;
    return true;
}

/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
   
/*** Only a reference to the advertised device is passed now
  void onResult(BLEAdvertisedDevice advertisedDevice) { **/     
  void onResult(BLEAdvertisedDevice* advertisedDevice) {
    printf("BLE Advertised Device found: %s\n", advertisedDevice->toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
/********************************************************************************
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
********************************************************************************/
    if (advertisedDevice->haveServiceUUID() && advertisedDevice->isAdvertisingService(serviceUUID)) {

      BLEDevice::getScan()->stop();
/*******************************************************************
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
*******************************************************************/
      myDevice = advertisedDevice; /** Just save the reference now, no need to copy the object */
      doConnect = true;
      doScan = true;

    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks


// This is the Arduino main loop function.
void connectTask (void * parameter){
    for(;;) {
      // If the flag "doConnect" is true then we have scanned for and found the desired
      // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
      // connected we set the connected flag to be true.
      if (doConnect == true) {
        if (connectToServer()) {
          printf("We are now connected to the BLE Server.\n");
        } else {
          printf("We have failed to connect to the server; there is nothin more we will do.\n");
        }
        doConnect = false;
      }

      // If we are connected to a peer BLE Server, update the characteristic each time we are reached
      // with the current time since boot.
      if (connected) {
        char buf[256];
        snprintf(buf, 256, "Time since boot: %lu", (unsigned long)(esp_timer_get_time() / 1000000ULL));
        printf("Setting new characteristic value to %s\n", buf);
        
        // Set the characteristic's value to be the array of bytes that is actually a string.
        /*** Note: write value now returns true if successful, false otherwise - try again or disconnect ***/
        pRemoteCharacteristic->writeValue((uint8_t*)buf, strlen(buf), false);
      }else if(doScan){
        BLEDevice::getScan()->start(0);  // this is just eample to start scan after disconnect, most likely there is better way to do it in arduino
      }
      
      vTaskDelay(1000/portTICK_PERIOD_MS); // Delay a second between loops.
    }
    
    vTaskDelete(NULL);
} // End of loop


/**********************************************************
//end of CODE imported from esp_nimble_client_V2
***********************************************************/




















//class MIDITimedBigMessage;

/*

//dummy message class
class MIDITimedBigMessage //: public MIDIBigMessage
{
public:
   int value;
    //
    // Constructors
    //

    MIDITimedBigMessage();

};


MIDITimedBigMessage::MIDITimedBigMessage(){
  // a "creator" with not so useful content...
 value = 3;  
};    
// * mybigmsg = new MIDITimedBigMessage();
MIDITimedBigMessage mybigmsg = MIDITimedBigMessage();
*/

/*

class MIDIQueue
{
public:
    MIDIQueue ( int num_msgs );
    virtual ~MIDIQueue();

    void Clear();

    bool CanPut() const;

    bool CanGet() const;

    bool IsFull() const
    {
        return !CanPut();
    }


    void Put ( const MIDITimedBigMessage &msg )
    {
        buf[next_in] = msg;
        next_in = ( next_in + 1 ) % bufsize;
    }

    MIDITimedBigMessage Get() const
    {
        return MIDITimedBigMessage ( buf[next_out] );
    }

    void Next()
    {
        next_out = ( next_out + 1 ) % bufsize;
    }

    const MIDITimedBigMessage *Peek() const
    {
        return &buf[next_out];
    }

protected:
    MIDITimedBigMessage *buf;
    int bufsize;
    volatile int next_in;
    volatile int next_out;
};




MIDIQueue::MIDIQueue ( int num_msgs )
    :
    buf ( new MIDITimedBigMessage[ num_msgs ] ),
    bufsize ( num_msgs ),
    next_in ( 0 ),
    next_out ( 0 )
{
}

MIDIQueue::~MIDIQueue()
{
    //jdks_safe_delete_array( buf ); //MUST STILL BE DEFINED
}


void MIDIQueue::Clear()
{
    next_in = 0;
    next_out = 0;
}

bool MIDIQueue::CanPut() const
{
    return next_out != ( ( next_in + 1 ) % bufsize );
}

bool MIDIQueue::CanGet() const
{
    return next_in != next_out;
}

*/

jdksmidi::MIDIQueue myQueue(100); //create queue of 100 (dummy) msgs...
//jdksmidi::MIDITrack myTrack(128); //create track of 2 chunks

int msg_id;

//audio codec parameters WM8978 
WM8978 wm8978;

//audio codec default parameters
int hpVol_L = 20;
int hpVol_R = 20;
int micGain = 30;
int lineinGain = 0;    //check wm8978 docs for min/max and type (float vs int)

bool micen    = false;    //microphone enable
bool lineinen = false;    //line in enable
bool auxen    = false;    //aux (in?) enable

bool dacen = true;        //DAC enable
bool bpen = false;        //bypass mic, line in , aux  CHECK

//implement functions to:
//          - set the GUI widgets to these initial values
//          - let the GUI status follow changes in these parameters (as an option.....)

//DspFaust* DSP;
bool incoming_updates = false;

//initial values in app for DSP parameters (may not be compliant with DspFaust.cpp defaults)
//NOTE: these are specific for a sound engine (WaveSynth_FX.dsp in the current version)
//Try to circumvent using these parameters. Use getParamValue, getVoiceParamValue, setParamValue, setVoiceParamValue and getParamsCount to
//have a more generic way to handle Dsp parameters

//implement functions to:
//          - set the GUI widgets to these initial values
//          - let the GUI status follow changes in these parameters (as an option.....)

float lfoFreq = 0.1;     //
float lfoDepth = 0;
float gain = 0.5;
int gate = 0;
float synthA = 0.01;
float synthD = 0.6;
float synthS = 0.2;
float synthR = 0.9;

float bend = 0;
float synthFreq = 440;
float waveTravel = 0;

int songlength = 0;
char * songbuffer = "Bond:d=4,o=5,b=80:32p,16c#6,32d#6,32d#6,16d#6,8d#6,16c#6,16c#6,16c#6,16c#6,32e6,32e6,16e6,8e6,16d#6,16d#6,16d#6,16c#6,32d#6,32d#6,16d#6,8d#6,16c#6,16c#6,16c#6,16c#6,32e6,32e6,16e6,8e6,16d#6,16d6,16c#6,16c#7,c.7,16g#6,16f#6,g#.6";
;

bool play_flag = true;
int poly = 0; //ofset for

unsigned short int play_incoming_mode = 0x80;
unsigned short int record_mode = 0x40;
unsigned short int metronome_mode = 0x20; 
unsigned short int loop_mode = 0x10
;  
unsigned short int seq_mode = play_incoming_mode + record_mode + metronome_mode;//+ metronome_mode;

//Prevent to use the following parameters. Use the generic Faust API functions instead!
//parameter base ID's (WaveSynth FX), taken from API README.md
//add 1 in case of polyphony
int synthABaseId = 0;
int synthDBaseId = 1;
int synthRBaseId = 2;
int synthSBaseId = 3;
int bendBaseId = 4;
int synthFreqBaseId = 5; //for DSP_MACHINE poly
int gainBaseId = 6; //for DSP_MACHINE poly
int gateBaseId = 7; //for DSP_MACHINE poly
int lfoDepthBaseId = 8;
int lfoFreqBaseId = 9;
int waveTravelBaseId = 10;

static const char *TAG = "MQTT_FAUST";



//retrieve the Dsp parameters


//retrieve the parameters of a particular voice

//retrieve the parameters of all (active) voices

//Callback for executing events for a list of message pointers
/*
void vEventTimerCallback( TimerHandle_t pxTimer ){
  int32_t lArrayIndex;
  const int32_t xMaxExpiryCountBeforeStopping = 10;

      // Optionally do something if the pxTimer parameter is NULL.
      configASSERT( pxTimer );
 
      // Which timer expired?
      lArrayIndex = ( int32_t ) pvTimerGetTimerID( pxTimer );
 
      // Increment the number of times that pxTimer has expired.
      lExpireCounters[ lArrayIndex ] += 1;
 
      // If the timer has expired 10 times then stop it from running.
      if( lExpireCounters[ lArrayIndex ] == xMaxExpiryCountBeforeStopping )
      {
          // Do not use a block time if calling a timer API function from a
          // timer callback function, as doing so could cause a deadlock!
          xTimerStop( pxTimer, 0 );
      }
  };

*/

unsigned long generate_Timestamp( unsigned long now_Ts) {
    
    return now_Ts-loopStart;
    
};

jdksmidi::MIDITimedBigMessage generate_MIDITimedBigMessage( int mididata, unsigned long timestamp){
    
       //convert incoming mididata to jdksmidi MIDITimedBigMessage
    //create a function that accepts mididata and current time and returns a MIDITimedBigMessage
    //this function will depend on another function that generates the message timestamp, based on actual time and on playing mode (e.g. correcting for looping)     
    static const char *TAG = "EXECUTE_SINGLE_COMMAND";
    int data2 = mididata & 0x000000ff;
    int data1 = (mididata & 0x0000ff00)>>8;
    int status = (mididata & 0x00ff0000)>>16;
    //ESP_LOGI(TAG,"NUMERICAL VALUE:%u ", mididata);
    //ESP_LOGI(TAG,"STATUS: %u (0x%X)", status, status);
    //ESP_LOGI(TAG,"DATA1: %u (0x%X)", data1, data1);
    //ESP_LOGI(TAG,"DATA2: %u (0x%X)", data2, data2) ; //integers are 32 bits!!!!             
    //ESP_LOGI(TAG,"...to be implemented...(store msg and) call mqtt_midi"); 
    int type = status & 0xf0;
    int channel = status & 0x0f;

    //ESP_LOGW(TAG,"seq_mode %d", seq_mode);
    //ESP_LOGW(TAG,"have a look at mode selection method!");
    

            //create a MIDITimedBigMessage

            jdksmidi::MIDITimedBigMessage inMessage = jdksmidi::MIDITimedBigMessage();

            //inMessage.Clear();  //this may be incluede in the msg constructor
      /*
            //need to store status
            inMessage.SetByte1(data1);
            inMessage.SetByte2(data2);
      */
        if (type == 0x90) {
            //  if (type == NOTE_ON) {
          int note = data1;
          int vel = data2;      
          inMessage.SetNoteOn(channel, note, vel); } 
        else {
          if (type == 0x80) {
            //    if (type == NOTE_OFF) {
          int note = data1;
          int vel = data2;      
          inMessage.SetNoteOff(channel, note, vel);  
          }  
          }
      //unsigned long eventTimestamp = xTaskGetTickCount()-loopStart; 

      inMessage.SetTime(timestamp );   //actual time in ticks id replaced by loop time or whatever time measure is active 
    return inMessage;
};

void propagate_Midi_Event(DspFaust * aDSP, int mididata) {    
    ESP_LOGW(TAG,"PLAY MIDI EVENT");                                  //wrap this into a routine using a MIDITimedBigMessage as input (not necessary for incoming
    int data2 = mididata & 0x000000ff;
    int data1 = (mididata & 0x0000ff00)>>8;
    int status = (mididata & 0x00ff0000)>>16;
    ESP_LOGI(TAG,"NUMERICAL VALUE:%u ", mididata);
    ESP_LOGI(TAG,"STATUS: %u (0x%X)", status, status);
    ESP_LOGI(TAG,"DATA1: %u (0x%X)", data1, data1);
    ESP_LOGI(TAG,"DATA2: %u (0x%X)", data2, data2) ; //integers are 32 bits!!!!             
    ESP_LOGI(TAG,"...to be implemented...(store msg and) call mqtt_midi"); 
    int type = status & 0xf0;
    int channel = status & 0x0f;
    int count = 3;
    int time = 0;
    aDSP->propagateMidi(count, time, type, channel, data1, data2);    
};

#define NUM_TIMERS 10

// An array to hold handles to the created timers.
TimerHandle_t MIDItimerArr[ NUM_TIMERS ];
const jdksmidi::MIDITimedBigMessage * msgPtrArr[NUM_TIMERS]; 
int eventsCount;


void   execute_MIDITimedBigMessage_immediate_temp(jdksmidi::MIDITimedBigMessage  inMessage) {
    static const char *TAG = "execute_MIDITimedBigMessage_immediate_temp";
    //ESP_LOGW(TAG,"execute_MIDITimedBigMessage_immediate_temp"); 
    //retrieve data from inMessage 
    int retrieved_status = inMessage.GetStatus();            
    int retrieved_data1 = inMessage.GetByte1();
    int retrieved_data2 = inMessage.GetByte2();           
    ESP_LOGI(TAG,"RETRIEVED_QUEUE STATUS:  %u (0x%X) DATA1: %u (0x%X)  DATA2: %u (0x%X)  timestamp: %lu", retrieved_status, retrieved_status, retrieved_data1, retrieved_data1, retrieved_data2, retrieved_data2, inMessage.GetTime());      
};


void   execute_MIDITimedBigMessage_immediate(DspFaust * aDSP, const jdksmidi::MIDITimedBigMessage * inMessage) {
    static const char *TAG = "EMTBMI";
    //ESP_LOGW(TAG,"execute_MIDITimedBigMessage_immediate_temp"); 
    //retrieve data from inMessage 
    int retrieved_status = inMessage->GetStatus();            
    int retrieved_data1 = inMessage->GetByte1();
    int retrieved_data2 = inMessage->GetByte2(); 
    int type = retrieved_status & 0xf0;
    int channel = retrieved_status & 0x0f;
    int count = 3;
    int time = 0;   
    aDSP->propagateMidi(count, time, type, channel, retrieved_data1, retrieved_data2);        
    ESP_LOGI(TAG,"STATUS:  %u (0x%X) DATA1: %u (0x%X)  DATA2: %u (0x%X)  timestamp: %lu", retrieved_status, retrieved_status, retrieved_data1, retrieved_data1, retrieved_data2, retrieved_data2, inMessage->GetTime()); 
};


void vMIDITimedBigmessageTimerCallback( TimerHandle_t pxTimer ){ 
 static const char *TAG = "MIDI_EVENT_CB";
 ESP_LOGI(TAG,"MIDI EVENT CALLBACK"); 
    int32_t eventIndex;
    // Optionally do something if the pxTimer parameter is NULL.
    configASSERT( pxTimer );

    // Which timer expired?
    eventIndex = ( int32_t ) pvTimerGetTimerID( pxTimer );
    ESP_LOGI(TAG,"eventIndex:  %d", eventIndex); 
    //execute the MIDIMsg 

    execute_MIDITimedBigMessage_immediate(DSP, msgPtrArr[eventIndex]); 
    
    //remove the timer for this event
    //also remove the entries from the MIDItimerArr and msgPtrArr
    //<code here>
    xTimerDelete(pxTimer,0); 
 };



void   execute_MIDITimedBigMessage(DspFaust * aDSP, const jdksmidi::MIDITimedBigMessage * inMessage) {
    static const char *TAG = "EMTBM timed";
    //ESP_LOGW(TAG,"execute_MIDITimedBigMessage_immediate_temp"); 
    //retrieve data from inMessage 
    int retrieved_status = inMessage->GetStatus();            
    int retrieved_data1 = inMessage->GetByte1();
    int retrieved_data2 = inMessage->GetByte2(); 
    int type = retrieved_status & 0xf0;
    int channel = retrieved_status & 0x0f;
    int count = 3;
    int time = 0;   
    aDSP->propagateMidi(count, time, type, channel, retrieved_data1, retrieved_data2);        
            ESP_LOGI(TAG,"MIDImsg:  %u (0x%X) DATA1: %u (0x%X)  DATA2: %u (0x%X)  timestamp: %lu", retrieved_status, retrieved_status, retrieved_data1, retrieved_data1, retrieved_data2, retrieved_data2, inMessage->GetTime()); 
};


const jdksmidi::MIDITimedBigMessage * retrieved_Message_Ptr ;

void handle_Queue(DspFaust * aDSP){
      static const char *TAG = "HANDLE_QUEUE"; 
       ESP_LOGI(TAG,"HANDLE_QUEUE");
         //***************************************************************   
            
            //retrieve msg from queue using Get
            jdksmidi::MIDITimedBigMessage retrieved_Message = jdksmidi::MIDITimedBigMessage();

            //the next code is for TESTING retrieval of messages from the queue
            //more convenient handlers for the queue content is done with the track class !            
            
            myQueue.Next();
            while (!(myQueue.CanGet())) { 
                     ESP_LOGW(TAG,"SKIP");
                     myQueue.Next();
            };
            
            ESP_LOGI(TAG,"READY SKIPPING NOT GETTABLES");
            eventsCount = 0;
            while (myQueue.CanGet()) {
                
             //   retrieved_Message = myQueue.Get();
                //retrieved_Message_ptr = myQueue.Peek();
             /*   
                // if (retrieved_Message.GetTime() != 0) {retrieved_Message = myQueue.Get();  myQueue.Next();};
                retrieved_status = retrieved_Message.GetStatus();  
                if (retrieved_status != 0x00){                
                retrieved_data1 = retrieved_Message.GetByte1();
                retrieved_data2 = retrieved_Message.GetByte2();
              
                ESP_LOGI(TAG,"RETRIEVED_QUEUE STATUS:  %u (0x%X) DATA1: %u (0x%X)  DATA2: %u (0x%X)  timestamp: %lu", retrieved_status, retrieved_status, retrieved_data1, retrieved_data1, retrieved_data2, retrieved_data2, retrieved_Message.GetTime()); 
             */   
                
                //start timers to play the events
                //the delay in ticks depends on the loopTime stored in the message,  on the actual loopTime and on the length of the loop in ticks
                //how to link a payload to the timer to transfer the message properties.....?
                //it is probably useful to link a single message pointer to the timer
            
//            msg_ptr_Arr[eventsCount] = retrieved_Message.Peek();
//                eventsCount = eventsCount + 1;

                //USE pvTimer id to hold an index to an array of message pointers, the message pointers to be collected from the message buffer with Peek
                //create timer with eventsCount as timer ID            
                //you can have a look at the timer example in the ESP-IDF freeRTOS docs: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/freertos.html
                //scan for: TimerHandle_t xTimers[ NUM_TIMERS ];
                
                
                //make sure to destroy the timer after use. OR use a cyclic scheme as with the message buffer. The number of timers does not have to be very long
                //something relating to the max number of events in a measure?   

                //create a routine that takes a pointer at a MIDITimedBigMessage as input and handles the event.
                //to be called in the timer callback.
                  retrieved_Message_Ptr = myQueue.Peek(); //a single pointer....  you need an array of it
            //  ESP_LOGV(TAG,"PEEK");
               
if  ( retrieved_Message_Ptr->GetStatus() !=0  ){  

    msgPtrArr[eventsCount] = retrieved_Message_Ptr;
ESP_LOGI(TAG,"EVENT FOUND");    
    //execute_MIDITimedBigMessage_immediate(aDSP, retrieved_Message_Ptr); 
 // ESP_LOGD(TAG,"PATH FINDER");             
                  //ESP_LOGE(TAG,"play event immediately "); 
                 //execute_MIDITimedBigMessage(aDSP, retrieved_Message_Ptr); 

        

        //calculate current position in loop (in ticks)         
         //xTaskGetTickCount()-loopStart;
         
//event should fire at:         
//retrieved_Message_Ptr->GetTime();

//so have to wait for:  
//retrieved_Message_Ptr->GetTime()-xTaskGetTickCount()+loopStart;
//if the number is negative, add the length of another loop
//length of loop   in ticks:   loopLength(measures)*timesig_num(beats per neasure) / tempo(bpm)*60 (s/m)*1000 (ms/s) /portTICK_PERIOD_MS (MS/tick)

ESP_LOGI(TAG,"loopDuration %d",loopDuration);
ESP_LOGI(TAG,"measureDuration %d",measureDuration);
ESP_LOGI(TAG,"beatDuration %d",beatDuration);

ESP_LOGI(TAG,"create timer, msgTimestamp: %lu current time %u loopStart %lu", retrieved_Message_Ptr->GetTime(), xTaskGetTickCount(), loopStart);

int delay = (retrieved_Message_Ptr->GetTime()-xTaskGetTickCount()+loopStart+loopDuration)%loopDuration;
ESP_LOGI(TAG,"create timer, length: %d", delay );

         MIDItimerArr[ eventsCount] = xTimerCreate(    "MIDITimer",       // Just a text name, not used by the kernel.
                                       ( delay ),   // The timer period in ticks. BASE IT ON GetTime of MIDIMsg!!!!
                                       pdFALSE,        // The timers will NOT auto-reload themselves when they expire.
                                       ( void * ) eventsCount,  // Assign each timer a unique id equal to its array index.
                                       vMIDITimedBigmessageTimerCallback // Each timer calls the same callback when it expires.
                                   );

       if( MIDItimerArr[ eventsCount ] == NULL )
       {
           // The timer was not created.
       }
       else
       {
           // Start the timer.  No block time is specified, and even if one was
           // it would be ignored because the scheduler has not yet been
           // started.
           if( xTimerStart( MIDItimerArr[ eventsCount ], 0 ) != pdPASS )
           {
               // The timer could not be set into the Active state.
           }
       }
  

 eventsCount = eventsCount + 1;

}; //if status....
        
// ESP_LOGE(TAG,"pos1 events in queue %d", eventsCount);
        myQueue.Next();
                };
               
            ESP_LOGI(TAG,"pos2 events in queue  %d", eventsCount);     
                
                //create vEventTimerCallback as in the docs (see above)
                
                //};
              
            //retrieve from queue using Peek    
   //   retrieved_Message_Ptr = myQueue.Peek(); //a single pointer....  you need an array of it
                        //const jdksmidi::MIDITimedBigMessage * retrieved_Message_ptr ; 
                        
     //test immediate playing with a pointer as input                   
   //  execute_MIDITimedBigMessage_immediate_temp(inMessage);      
     //test immediate playing with a pointer as input
 //   execute_MIDITimedBigMessage_immediate(retrieved_Message_Ptr); 
     // execute_MIDITimedBigMessage_immediate(jdksmidi::MIDITimedBigMessage * inMessage);  //to be implemented, overrides the timestamp in the message
     //test timed playing with a pointer as input
     //execute_MIDITimedBigMessage(jdksmidi::MIDITimedBigMessage * inMessage);  //to be implemented, uses the timestamp in the message
     //test using an array of timed events
 //    const jdksmidi::MIDITimedBigMessage * msg_ptr_Arr[20];        
     //have a look at delayUntil.....???
            
            //ESP_LOGW(TAG,"inMessage %s", inMessage.MsgToText());
            //SetTime  //timestamp is actual midi time 
            //if quantisizer_on:  round to nearest beat time
            //actual miditime my be looping
            //Set note parameters
            //insert in queue
          //  ESP_LOGE(TAG,"RECORD MIDI EVENT (end of code)");
    
}; //handle_Queue











void execute_single_midi_command(DspFaust * aDSP, int mididata){  //uses propagateMidi
static const char *TAG = "execute_single_midi_command";
      //unsigned long eventTimestamp = generate_Timestamp(xTaskGetTickCount());
    unsigned long event_Timestamp =  generate_Timestamp( xTaskGetTickCount() ) ;
    jdksmidi::MIDITimedBigMessage inMessage = generate_MIDITimedBigMessage( mididata, event_Timestamp);
    
    if (!(seq_mode & play_incoming_mode) == 0) {    
        //and play
     propagate_Midi_Event(aDSP, mididata);
     

    };
    
    
    if (!(seq_mode & record_mode) == 0) {
        //record
        ESP_LOGI(TAG,"RECORD MIDI EVENT (start of code)");
        

            if (myQueue.CanPut()){
            myQueue.Put(inMessage); //store inMessage in queue and move next_in index up by one position
                                    //messages are in the queue in order of the moment that they are added. Especially for e.g. looping applications
                                    //the messages are NOT in order of the time that the events need to be fired
                                    
                                    //add an option to insert a message in the right position. This is at the cost of effort at insertion, but prevents the need for sorting later
                                    //this may be a similar effort, and use similar or the same basic handling....
                                    
                                    //for firing of events, timers based on the actual moment of activation can be used
                                    //to prevent long timer / output queues the main message queue must be sorted now and then. THis is also done in the jdksmidi lib
                                    //by the track subclass 
            
            } else { ESP_LOGE(TAG,"cannot Put msg to queue");} 
            //display length of queue or get some other indication of it's contents....
            //ESP_LOGE(TAG,"myQueue.next_in %d", myQueue.next_in);
            
          ESP_LOGI(TAG,"RECORD MIDI EVENT (end of code)");  

        };
        
 };




/*
* update DSP parameters, based on the values set in the input storage
* input storage is updated asynchronously in MQTT callbacks (API , API2)
* update_controls is called at strategically suitable moments in e.g. playing sequences
*/

void update_all_controls (DspFaust * aDSP){
 static const char *TAG = "update_all_controls"; 
  if (incoming_updates) {
  
  wm8978.hpVolSet(hpVol_R, hpVol_L);
  ESP_LOGI(TAG, "poly: %d", poly); 
  //if (poly == 1){
    //found out that the BaseId is sufficient, poly = 0
    poly = 1;
    
    //later, use a struct, better: re-use an existing struct
    //and or use MACROS and a loop
 /*   
        aDSP->setParamValue(synthFreqBaseId+poly, synthFreq);
    ESP_LOGI(TAG, "synthFreqBaseId %d, set gain %6.2f", synthFreqBaseId, synthFreq);  //FCKX  
         aDSP->setParamValue(gateBaseId+poly, gate);
  //  ESP_LOGI(TAG, "gateBaseId %d, set gate %6.2f", gateBaseId, gate);  //FCKX    
    
           aDSP->setParamValue(waveTravelBaseId+poly, waveTravel);
    ESP_LOGI(TAG, "waveTravelBaseId %d, set waveTravel %6.2f", waveTravelBaseId, waveTravel);  //FCKX    
    
  */  
    aDSP->setParamValue(bendBaseId+poly, bend);
    ESP_LOGI(TAG, "bendBaseId %d, set bend %6.2f", bendBaseId, bend);  //FCKX   
    aDSP->setParamValue(gainBaseId+poly, gain);
    ESP_LOGI(TAG, "gainBaseId %d, set gain %6.2f", gainBaseId, gain);  //FCKX
    
    
    aDSP->setParamValue(lfoFreqBaseId+poly, lfoFreq);
    ESP_LOGI(TAG, "lfoFreqBaseId %d, set lfoFreq %6.2f", lfoFreqBaseId, lfoFreq);  //FCKX
    aDSP->setParamValue(lfoDepthBaseId+poly, lfoDepth);
    ESP_LOGI(TAG, "lfoDepthBaseId %d, set lfoDepth %6.2f", lfoDepthBaseId, lfoDepth);  //FCKX  
    aDSP->setParamValue(synthABaseId+poly, synthA);
    ESP_LOGI(TAG, "synthABaseId %d, set synthA %6.2f", synthABaseId, synthA);  //FCKX
    aDSP->setParamValue(synthDBaseId+poly, synthD);
    ESP_LOGI(TAG, "synthDBaseId %d, set synthD %6.2f", synthDBaseId, synthD);  //FCKX
    aDSP->setParamValue(synthSBaseId+poly, synthS);
    ESP_LOGI(TAG, "synthSBaseId %d, set synthS %6.2f", synthSBaseId, synthS);  //FCKX
    aDSP->setParamValue(synthRBaseId+poly, synthR);
    ESP_LOGI(TAG, "synthRBaseId %d, set synthR %6.2f", synthRBaseId, synthR);  //FCKX
/* 
 } else { //poly == 0
    DSP->setParamValue(lfoFreqBaseId+poly, lfoFreq);
    ESP_LOGI(TAG, "lfoFreqBaseId %d, set lfoFreq %6.2f", lfoFreqBaseId, lfoFreq);  //FCKX
    aDSP->setParamValue(lfoDepthBaseId+poly, lfoDepth); 
    aDSP->setParamValue(synthABaseId+poly, synthA);
    aDSP->setParamValue(synthDBaseId+poly, synthD);
    aDSP->setParamValue(synthSBaseId+poly, synthS);
    aDSP->setParamValue(synthRBaseId+poly, synthR);    
      
  }
*/    
  }
  incoming_updates = false;

}


void update_controls_all(DspFaust * aDSP){  //maybe do this in a cyclic freertos timer task
  static const char *TAG = "update_controls"; 
  if (incoming_updates) {
  
  wm8978.hpVolSet(hpVol_R, hpVol_L);
  ESP_LOGI(TAG, "poly: %d", poly); 
  //if (poly == 1){
    //found out that the BaseId is sufficient, poly = 0
    poly = 0;
    
    //later, use a struct, better: re-use an existing struct
    //and or use MACROS and a loop
        aDSP->setParamValue(synthFreqBaseId+poly, synthFreq);
    ESP_LOGI(TAG, "synthFreqBaseId %d, set gain %6.2f", synthFreqBaseId, synthFreq);  //FCKX  
         aDSP->setParamValue(gateBaseId+poly, gate);
  //  ESP_LOGI(TAG, "gateBaseId %d, set gate %6.2f", gateBaseId, gate);  //FCKX    
    
           aDSP->setParamValue(waveTravelBaseId+poly, waveTravel);
    ESP_LOGI(TAG, "waveTravelBaseId %d, set waveTravel %6.2f", waveTravelBaseId, waveTravel);  //FCKX    
    
    
    aDSP->setParamValue(bendBaseId+poly, bend);
    ESP_LOGI(TAG, "bendBaseId %d, set bend %6.2f", bendBaseId, bend);  //FCKX   
    aDSP->setParamValue(gainBaseId+poly, gain);
    ESP_LOGI(TAG, "gainBaseId %d, set gain %6.2f", gainBaseId, gain);  //FCKX
    
    
    aDSP->setParamValue(lfoFreqBaseId+poly, lfoFreq);
    ESP_LOGI(TAG, "lfoFreqBaseId %d, set lfoFreq %6.2f", lfoFreqBaseId, lfoFreq);  //FCKX
    aDSP->setParamValue(lfoDepthBaseId+poly, lfoDepth);
    ESP_LOGI(TAG, "lfoDepthBaseId %d, set lfoDepth %6.2f", lfoDepthBaseId, lfoDepth);  //FCKX  
    aDSP->setParamValue(synthABaseId+poly, synthA);
    ESP_LOGI(TAG, "synthABaseId %d, set synthA %6.2f", synthABaseId, synthA);  //FCKX
    aDSP->setParamValue(synthDBaseId+poly, synthD);
    ESP_LOGI(TAG, "synthDBaseId %d, set synthD %6.2f", synthDBaseId, synthD);  //FCKX
    aDSP->setParamValue(synthSBaseId+poly, synthS);
    ESP_LOGI(TAG, "synthSBaseId %d, set synthS %6.2f", synthSBaseId, synthS);  //FCKX
    aDSP->setParamValue(synthRBaseId+poly, synthR);
    ESP_LOGI(TAG, "synthRBaseId %d, set synthR %6.2f", synthRBaseId, synthR);  //FCKX
/* 
 } else { //poly == 0
    DSP->setParamValue(lfoFreqBaseId+poly, lfoFreq);
    ESP_LOGI(TAG, "lfoFreqBaseId %d, set lfoFreq %6.2f", lfoFreqBaseId, lfoFreq);  //FCKX
    aDSP->setParamValue(lfoDepthBaseId+poly, lfoDepth); 
    aDSP->setParamValue(synthABaseId+poly, synthA);
    aDSP->setParamValue(synthDBaseId+poly, synthD);
    aDSP->setParamValue(synthSBaseId+poly, synthS);
    aDSP->setParamValue(synthRBaseId+poly, synthR);    
      
  }
*/    
  }
  incoming_updates = false;

}



void update_controls(uintptr_t voiceAddress, DspFaust * aDSP){  //maybe do this in a cyclic freertos timer task
  static const char *TAG = "update_controls"; 
  if (incoming_updates) {
  
  wm8978.hpVolSet(hpVol_R, hpVol_L);
  ESP_LOGI(TAG, "poly: %d", poly); 
  //if (poly == 1){
    //found out that the BaseId is sufficient, poly = 0
    poly = 0;
    
    //later, use a struct, better: re-use an existing struct
    //and or use MACROS and a loop
        aDSP->setVoiceParamValue(synthFreqBaseId+poly,voiceAddress, synthFreq);
    ESP_LOGI(TAG, "synthFreqBaseId %d, set gain %6.2f", synthFreqBaseId, synthFreq);  //FCKX  
         aDSP->setVoiceParamValue(gateBaseId+poly,voiceAddress, gate);
  //  ESP_LOGI(TAG, "gateBaseId %d, set gate %6.2f", gateBaseId, gate);  //FCKX    
    
           aDSP->setVoiceParamValue(waveTravelBaseId+poly,voiceAddress, waveTravel);
    ESP_LOGI(TAG, "waveTravelBaseId %d, set waveTravel %6.2f", waveTravelBaseId, waveTravel);  //FCKX    
    
    
    aDSP->setVoiceParamValue(bendBaseId+poly,voiceAddress, bend);
    ESP_LOGI(TAG, "bendBaseId %d, set bend %6.2f", bendBaseId, bend);  //FCKX   
    aDSP->setVoiceParamValue(gainBaseId+poly,voiceAddress, gain);
    ESP_LOGI(TAG, "gainBaseId %d, set gain %6.2f", gainBaseId, gain);  //FCKX
    
    
    aDSP->setVoiceParamValue(lfoFreqBaseId+poly,voiceAddress, lfoFreq);
    ESP_LOGI(TAG, "lfoFreqBaseId %d, set lfoFreq %6.2f", lfoFreqBaseId, lfoFreq);  //FCKX
    aDSP->setVoiceParamValue(lfoDepthBaseId+poly,voiceAddress, lfoDepth);
    ESP_LOGI(TAG, "lfoDepthBaseId %d, set lfoDepth %6.2f", lfoDepthBaseId, lfoDepth);  //FCKX  
    aDSP->setVoiceParamValue(synthABaseId+poly,voiceAddress, synthA);
    ESP_LOGI(TAG, "synthABaseId %d, set synthA %6.2f", synthABaseId, synthA);  //FCKX
    aDSP->setVoiceParamValue(synthDBaseId+poly,voiceAddress, synthD);
    ESP_LOGI(TAG, "synthDBaseId %d, set synthD %6.2f", synthDBaseId, synthD);  //FCKX
    aDSP->setVoiceParamValue(synthSBaseId+poly,voiceAddress, synthS);
    ESP_LOGI(TAG, "synthSBaseId %d, set synthS %6.2f", synthSBaseId, synthS);  //FCKX
    aDSP->setVoiceParamValue(synthRBaseId+poly,voiceAddress, synthR);
    ESP_LOGI(TAG, "synthRBaseId %d, set synthR %6.2f", synthRBaseId, synthR);  //FCKX
/* 
 } else { //poly == 0
    DSP->setParamValue(lfoFreqBaseId+poly, lfoFreq);
    ESP_LOGI(TAG, "lfoFreqBaseId %d, set lfoFreq %6.2f", lfoFreqBaseId, lfoFreq);  //FCKX
    aDSP->setParamValue(lfoDepthBaseId+poly, lfoDepth); 
    aDSP->setParamValue(synthABaseId+poly, synthA);
    aDSP->setParamValue(synthDBaseId+poly, synthD);
    aDSP->setParamValue(synthSBaseId+poly, synthS);
    aDSP->setParamValue(synthRBaseId+poly, synthR);    
      
  }
*/    
  }
  incoming_updates = false;

}



void nbDelay(int delayTicks) {        
        TickType_t startTick = xTaskGetTickCount();
        while( (xTaskGetTickCount()-startTick) < pdMS_TO_TICKS(delayTicks)){
          //…
        } 
    };

/* note playing routines using the Faust API commands
*
*/

/* definitions for use in rtttl sequencers 
*/
#define OCTAVE_OFFSET 0

float freqs[] = { 0,
65.40639133,69.29565774,73.41619198,77.78174593,82.40688923,87.30705786,92.49860568,97.998859,103.8261744,110,116.5409404,123.4708253,
130.8127827,138.5913155,146.832384,155.5634919,164.8137785,174.6141157,184.9972114,195.997718,207.6523488,220,233.0818808,246.9416506,
261.6255653,277.182631,293.6647679,311.1269837,329.6275569,349.2282314,369.9944227,391.995436,415.3046976,440,466.1637615,493.8833013,
523.2511306,554.365262,587.3295358,622.2539674,659.2551138,698.4564629,739.9888454,783.990872,830.6093952,880,932.327523,987.7666025
};

/*
int notes[] = { 0,
NOTE_C4, NOTE_CS4, NOTE_D4, NOTE_DS4, NOTE_E4, NOTE_F4, NOTE_FS4, NOTE_G4, NOTE_GS4, NOTE_A4, NOTE_AS4, NOTE_B4,
NOTE_C5, NOTE_CS5, NOTE_D5, NOTE_DS5, NOTE_E5, NOTE_F5, NOTE_FS5, NOTE_G5, NOTE_GS5, NOTE_A5, NOTE_AS5, NOTE_B5,
NOTE_C6, NOTE_CS6, NOTE_D6, NOTE_DS6, NOTE_E6, NOTE_F6, NOTE_FS6, NOTE_G6, NOTE_GS6, NOTE_A6, NOTE_AS6, NOTE_B6,
NOTE_C7, NOTE_CS7, NOTE_D7, NOTE_DS7, NOTE_E7, NOTE_F7, NOTE_FS7, NOTE_G7, NOTE_GS7, NOTE_A7, NOTE_AS7, NOTE_B7
};
*/

//char *song = "The Simpsons:d=4,o=5,b=160:c.6,e6,f#6,8a6,g.6,e6,c6,8a,8f#,8f#,8f#,2g,8p,8p,8f#,8f#,8f#,8g,a#.,8c6,8c6,8c6,c6";
//char *song = "Indiana:d=4,o=5,b=250:e,8p,8f,8g,8p,1c6,8p.,d,8p,8e,1f,p.,g,8p,8a,8b,8p,1f6,p,a,8p,8b,2c6,2d6,2e6,e,8p,8f,8g,8p,1c6,p,d6,8p,8e6,1f.6,g,8p,8g,e.6,8p,d6,8p,8g,e.6,8p,d6,8p,8g,f.6,8p,e6,8p,8d6,2c6";
//char *song = "TakeOnMe:d=4,o=4,b=160:8f#5,8f#5,8f#5,8d5,8p,8b,8p,8e5,8p,8e5,8p,8e5,8g#5,8g#5,8a5,8b5,8a5,8a5,8a5,8e5,8p,8d5,8p,8f#5,8p,8f#5,8p,8f#5,8e5,8e5,8f#5,8e5,8f#5,8f#5,8f#5,8d5,8p,8b,8p,8e5,8p,8e5,8p,8e5,8g#5,8g#5,8a5,8b5,8a5,8a5,8a5,8e5,8p,8d5,8p,8f#5,8p,8f#5,8p,8f#5,8e5,8e5";
//char *song = "Entertainer:d=4,o=5,b=140:8d,8d#,8e,c6,8e,c6,8e,2c.6,8c6,8d6,8d#6,8e6,8c6,8d6,e6,8b,d6,2c6,p,8d,8d#,8e,c6,8e,c6,8e,2c.6,8p,8a,8g,8f#,8a,8c6,e6,8d6,8c6,8a,2d6";
//char *song = "Muppets:d=4,o=5,b=250:c6,c6,a,b,8a,b,g,p,c6,c6,a,8b,8a,8p,g.,p,e,e,g,f,8e,f,8c6,8c,8d,e,8e,8e,8p,8e,g,2p,c6,c6,a,b,8a,b,g,p,c6,c6,a,8b,a,g.,p,e,e,g,f,8e,f,8c6,8c,8d,e,8e,d,8d,c";
//char *song = "Xfiles:d=4,o=5,b=125:e,b,a,b,d6,2b.,1p,e,b,a,b,e6,2b.,1p,g6,f#6,e6,d6,e6,2b.,1p,g6,f#6,e6,d6,f#6,2b.,1p,e,b,a,b,d6,2b.,1p,e,b,a,b,e6,2b.,1p,e6,2b.";
//char *song = "Looney:d=4,o=5,b=140:32p,c6,8f6,8e6,8d6,8c6,a.,8c6,8f6,8e6,8d6,8d#6,e.6,8e6,8e6,8c6,8d6,8c6,8e6,8c6,8d6,8a,8c6,8g,8a#,8a,8f";
//char *song = "20thCenFox:d=16,o=5,b=140:b,8p,b,b,2b,p,c6,32p,b,32p,c6,32p,b,32p,c6,32p,b,8p,b,b,b,32p,b,32p,b,32p,b,32p,b,32p,b,32p,b,32p,g#,32p,a,32p,b,8p,b,b,2b,4p,8e,8g#,8b,1c#6,8f#,8a,8c#6,1e6,8a,8c#6,8e6,1e6,8b,8g#,8a,2b";
//char *song = "Bond:d=4,o=5,b=80:32p,16c#6,32d#6,32d#6,16d#6,8d#6,16c#6,16c#6,16c#6,16c#6,32e6,32e6,16e6,8e6,16d#6,16d#6,16d#6,16c#6,32d#6,32d#6,16d#6,8d#6,16c#6,16c#6,16c#6,16c#6,32e6,32e6,16e6,8e6,16d#6,16d6,16c#6,16c#7,c.7,16g#6,16f#6,g#.6";
//char *song = "MASH:d=8,o=5,b=140:4a,4g,f#,g,p,f#,p,g,p,f#,p,2e.,p,f#,e,4f#,e,f#,p,e,p,4d.,p,f#,4e,d,e,p,d,p,e,p,d,p,2c#.,p,d,c#,4d,c#,d,p,e,p,4f#,p,a,p,4b,a,b,p,a,p,b,p,2a.,4p,a,b,a,4b,a,b,p,2a.,a,4f#,a,b,p,d6,p,4e.6,d6,b,p,a,p,2b";
//char *song = "StarWars:d=4,o=5,b=45:32p,32f#,32f#,32f#,8b.,8f#.6,32e6,32d#6,32c#6,8b.6,16f#.6,32e6,32d#6,32c#6,8b.6,16f#.6,32e6,32d#6,32e6,8c#.6,32f#,32f#,32f#,8b.,8f#.6,32e6,32d#6,32c#6,8b.6,16f#.6,32e6,32d#6,32c#6,8b.6,16f#.6,32e6,32d#6,32e6,8c#6";
//char *song = "TopGun:d=4,o=4,b=31:32p,16c#,16g#,16g#,32f#,32f,32f#,32f,16d#,16d#,32c#,32d#,16f,32d#,32f,16f#,32f,32c#,16f,d#,16c#,16g#,16g#,32f#,32f,32f#,32f,16d#,16d#,32c#,32d#,16f,32d#,32f,16f#,32f,32c#,g#";
//char *song = "A-Team:d=8,o=5,b=125:4d#6,a#,2d#6,16p,g#,4a#,4d#.,p,16g,16a#,d#6,a#,f6,2d#6,16p,c#.6,16c6,16a#,g#.,2a#";
//char *song = "Flinstones:d=4,o=5,b=40:32p,16f6,16a#,16a#6,32g6,16f6,16a#.,16f6,32d#6,32d6,32d6,32d#6,32f6,16a#,16c6,d6,16f6,16a#.,16a#6,32g6,16f6,16a#.,32f6,32f6,32d#6,32d6,32d6,32d#6,32f6,16a#,16c6,a#,16a6,16d.6,16a#6,32a6,32a6,32g6,32f#6,32a6,8g6,16g6,16c.6,32a6,32a6,32g6,32g6,32f6,32e6,32g6,8f6,16f6,16a#.,16a#6,32g6,16f6,16a#.,16f6,32d#6,32d6,32d6,32d#6,32f6,16a#,16c.6,32d6,32d#6,32f6,16a#,16c.6,32d6,32d#6,32f6,16a#6,16c7,8a#.6";
//char *song = "Jeopardy:d=4,o=6,b=125:c,f,c,f5,c,f,2c,c,f,c,f,a.,8g,8f,8e,8d,8c#,c,f,c,f5,c,f,2c,f.,8d,c,a#5,a5,g5,f5,p,d#,g#,d#,g#5,d#,g#,2d#,d#,g#,d#,g#,c.7,8a#,8g#,8g,8f,8e,d#,g#,d#,g#5,d#,g#,2d#,g#.,8f,d#,c#,c,p,a#5,p,g#.5,d#,g#";
//char *song = "Gadget:d=16,o=5,b=50:32d#,32f,32f#,32g#,a#,f#,a,f,g#,f#,32d#,32f,32f#,32g#,a#,d#6,4d6,32d#,32f,32f#,32g#,a#,f#,a,f,g#,f#,8d#";
//char *song = "Smurfs:d=32,o=5,b=200:4c#6,16p,4f#6,p,16c#6,p,8d#6,p,8b,p,4g#,16p,4c#6,p,16a#,p,8f#,p,8a#,p,4g#,4p,g#,p,a#,p,b,p,c6,p,4c#6,16p,4f#6,p,16c#6,p,8d#6,p,8b,p,4g#,16p,4c#6,p,16a#,p,8b,p,8f,p,4f#";
//char *song = "MahnaMahna:d=16,o=6,b=125:c#,c.,b5,8a#.5,8f.,4g#,a#,g.,4d#,8p,c#,c.,b5,8a#.5,8f.,g#.,8a#.,4g,8p,c#,c.,b5,8a#.5,8f.,4g#,f,g.,8d#.,f,g.,8d#.,f,8g,8d#.,f,8g,d#,8c,a#5,8d#.,8d#.,4d#,8d#.";
//char *song = "LeisureSuit:d=16,o=6,b=56:f.5,f#.5,g.5,g#5,32a#5,f5,g#.5,a#.5,32f5,g#5,32a#5,g#5,8c#.,a#5,32c#,a5,a#.5,c#.,32a5,a#5,32c#,d#,8e,c#.,f.,f.,f.,f.,f,32e,d#,8d,a#.5,e,32f,e,32f,c#,d#.,c#";
//char *song = "MissionImp:d=16,o=6,b=95:32d,32d#,32d,32d#,32d,32d#,32d,32d#,32d,32d,32d#,32e,32f,32f#,32g,g,8p,g,8p,a#,p,c7,p,g,8p,g,8p,f,p,f#,p,g,8p,g,8p,a#,p,c7,p,g,8p,g,8p,f,p,f#,p,a#,g,2d,32p,a#,g,2c#,32p,a#,g,2c,a#5,8c,2p,32p,a#5,g5,2f#,32p,a#5,g5,2f,32p,a#5,g5,2e,d#,8d";
char *song = "GoodBad:d=4,o=5,b=56:32p,32a#,32d#6,32a#,32d#6,8a#.,16f#.,16g#.,d#,32a#,32d#6,32a#,32d#6,8a#.,16f#.,16g#.,c#6,32a#,32d#6,32a#,32d#6,8a#.,16f#.,32f.,32d#.,c#,32a#,32d#6,32a#,32d#6,8a#.,16g#.,d#";

#define isdigit(n) (n >= '0' && n <= '9')

/* 
* keyOn / keyOff players
*/

void play_keys(DspFaust * aDSP){  //uses keyOn / keyOff
       // start continuous background voice for testing polyphony
       static const char *TAG = "PLAY_KEYS";
       int res;
       uintptr_t voiceAddress;
       ESP_LOGI(TAG, "starting play_keys");
       
       aDSP->keyOn(50, 126);
       vTaskDelay(3000 / portTICK_PERIOD_MS);
       
        int vel1 = 126;
        for (int pitch = 51; pitch < 69; pitch++){
   
        //printf("counter ii %d \n",ii);    
        nbDelay(100);
        //vTaskDelay(100 / portTICK_PERIOD_MS);
        
           
        ESP_LOGI(TAG, "keyOn pitch %d velocity % d", pitch,vel1);      
        voiceAddress = aDSP->keyOn(pitch,vel1);
        //update_controls(voiceAddress,aDSP);
        ESP_LOGI(TAG, "after keyOn");  
        //cannot use update_controls as used here for this kind of voice ?? 
        //update_controls(voiceAddress,aDSP); 
         nbDelay(1000);
        //vTaskDelay(1000 / portTICK_PERIOD_MS);
        //aDSP->setVoiceParamValue(5,voiceAddress,110);
        //update_controls(voiceAddress,aDSP);
        //aDSP->setVoiceParamValue("/WaveSynth_FX/freq",voiceAddress,110);
         nbDelay(1000);
        //vTaskDelay(1000 / portTICK_PERIOD_MS);
        ESP_LOGI(TAG, "keyOn pitch %d velocity % d", pitch,vel1);  
        res = aDSP->keyOff(pitch);
        } 
         ESP_LOGI(TAG, "end of sequence");        
          nbDelay(3000);
         //vTaskDelay(3000 / portTICK_PERIOD_MS);
         
        
        //release continuous background voice
        aDSP->keyOff(50);
        
}


void play_keys_nb(DspFaust * aDSP){  //uses keyOn / keyOff
    // start continuous background voice for testing polyphony
    static const char *TAG = "PLAY_KEYS";
    int res;
    uintptr_t voiceAddress;
    ESP_LOGW(TAG, "starting play_keys_nb ");

    nbDelay(3000); 
    int vel1 = 126;
    
    voiceAddress = aDSP->keyOn(38,vel1);
    nbDelay(5000); 
    for (int pitch = 48; pitch < 69; pitch++){

    //printf("counter ii %d \n",ii);  

    nbDelay(100);        
       //C major 0,2,6
       //D minor 0, -2, 2
       //A minor 0, 2, 4
    ESP_LOGI(TAG, "keyOn pitch %d velocity % d", pitch,vel1);      
    voiceAddress = aDSP->keyOn(pitch+0,vel1);
    voiceAddress = aDSP->keyOn(pitch -2,vel1);
    voiceAddress = aDSP->keyOn(pitch +2,vel1);
   voiceAddress = aDSP->keyOn(pitch+9,vel1);
    //update_controls(voiceAddress,aDSP);
    ESP_LOGI(TAG, "after keyOn");  

        

    ESP_LOGI(TAG, "keyOn pitch %d velocity % d", pitch,vel1); 

    nbDelay(100);         

     res = aDSP->keyOff(pitch+0);
     res = aDSP->keyOff(pitch-2);
      res = aDSP->keyOff(pitch+2);
      res = aDSP->keyOff(pitch+9);
    } 
     ESP_LOGW(TAG, "end of sequence");     
    nbDelay(5000); 
   
    //release continuous background voice
     res = aDSP->keyOff(38);
 /* 
 aDSP->keyOff(50);
    */
    }


void play_keys2(DspFaust * aDSP){  //uses keyOn / keyOff
       // start continuous background voice
       
      
       aDSP->keyOn(50, 126);
       nbDelay(3000);
       //vTaskDelay(3000 / portTICK_PERIOD_MS);
/*       
       //release continuous background voice
       //aDSP->keyOff(50); 
       */
       
        for (int ii = 52; ii < 71; ii++){
        printf("counter ii %d \n",ii);    
        nbDelay(100);
        //vTaskDelay(100 / portTICK_PERIOD_MS);    
        uintptr_t voiceAddress = aDSP->keyOn(ii,126);
        //cannot use update_controls as used here for this kind of voice 
        //update_controls(voiceAddress,aDSP); 
        nbDelay(1000);
        //vTaskDelay(1000 / portTICK_PERIOD_MS);
        //aDSP->setVoiceParamValue(5,voiceAddress,110);
        aDSP->setVoiceParamValue("/WaveSynth_FX/freq",voiceAddress,110);
        nbDelay(1000);
        //vTaskDelay(1000 / portTICK_PERIOD_MS);
        int res = aDSP->keyOff(ii);
        
        }  
         vTaskDelay(3000 / portTICK_PERIOD_MS);
        
        //release continuous background voice
        aDSP->keyOff(50);
        
}


/* 
*propagateMidi players
*/


void play_midi(DspFaust * aDSP){  //uses propagateMidi
       // start continuous background voice for testing polyphony
       static const char *TAG = "PLAY_MIDI";
       int res;
       uintptr_t voiceAddress;
       ESP_LOGI(TAG, "starting play_keys");
       /*
       aDSP->keyOn(50, 126);
       vTaskDelay(3000 / portTICK_PERIOD_MS);
       */
        int vel1 = 126;
        
        int count;
        double time;
        int type;
        int channel;
        int data1;       
        int data2;
        
        
        for (int pitch = 48; pitch < 69; pitch++){
   
        //printf("counter ii %d \n",ii);    
        //vTaskDelay(100 / portTICK_PERIOD_MS);
        nbDelay(100);
        
        count = 3;
        time = 0;
        type = 9*16; //status
        channel = 0;
        data1 = 4*16+pitch-64;//pitch       
        data2 = 127;//4*16;//attack
        
        ESP_LOGI(TAG, "keyOn pitch %d velocity % d", pitch,vel1);      
        //voiceAddress = aDSP->keyOn(pitch,vel1);
        aDSP->propagateMidi(count, time, type, channel, data1, data2);
        
        //update_controls(voiceAddress,aDSP);
        ESP_LOGI(TAG, "after keyOn");  
        //cannot use update_controls as used here for this kind of voice ?? 
        //update_controls(voiceAddress,aDSP); 
        //vTaskDelay(1000 / portTICK_PERIOD_MS);
        nbDelay(1000);
        //aDSP->setVoiceParamValue(5,voiceAddress,110);
        //update_controls(voiceAddress,aDSP);
        //aDSP->setVoiceParamValue("/WaveSynth_FX/freq",voiceAddress,110);
        //vTaskDelay(1000 / portTICK_PERIOD_MS);
        nbDelay(1000);
        ESP_LOGI(TAG, "keyOn pitch %d velocity % d", pitch,vel1);  
        //res = aDSP->keyOff(pitch);
        count = 3;
        time = 0;
        type = 8*16; //staus
        channel = 0;
        data1 = 4*16+pitch-64; //pitch      
        data2 = 0*16; //attack

        aDSP->propagateMidi(count, time, type, channel, data1, data2);
        } 
         ESP_LOGI(TAG, "end of sequence");        
         //vTaskDelay(3000 / portTICK_PERIOD_MS);
         nbDelay(3000);
        /*
        //release continuous background voice
        aDSP->keyOff(50);
        */
}


/* 
* setVoiceParamValue path players
*/

void play_setVoiceParam_path(DspFaust * aDSP) 
{ //uses setVoiceParamValue(path
       static const char *TAG = "PLAY_setVoiceParam_path";
       ESP_LOGI(TAG, "starting play_setVoiceParam_path");
/*
    uintptr_t bg_voiceAddress = aDSP->newVoice(); //create background voice    
    aDSP->setVoiceParamValue("/WaveSynth_FX/gain",bg_voiceAddress,1);  
    aDSP->setVoiceParamValue("/WaveSynth_FX/freq",bg_voiceAddress,110.0);
    // aDSP->setVoiceParamValue("/WaveSynth_FX/freq",freqs[(scale-4) * 12 + note]);
    aDSP->setVoiceParamValue("/WaveSynth_FX/gate",bg_voiceAddress,1.0);
    vTaskDelay(500 / portTICK_PERIOD_MS);  
*/

    /*
    uintptr_t bg_voiceAddress = aDSP->newVoice(); //create main voice    
    aDSP->setVoiceParamValue("/WaveSynth_FX/gain",bg_voiceAddress,1);
    update_controls(bg_voiceAddress,aDSP);
    */
    
    uintptr_t voiceAddress = aDSP->newVoice(); //create main voice
    aDSP->setVoiceParamValue("/WaveSynth_FX/gain",voiceAddress,1);
    //update_controls(voiceAddress,aDSP);
        
    //aDSP->setVoiceParamValue("/WaveSynth_FX/gain",voiceAddress,1); 
    
        for (int ii = 50; ii < 60; ii++){
           update_controls(voiceAddress,aDSP);  
           ESP_LOGI(TAG, "going to set frequency 2"); 
           
           aDSP->setVoiceParamValue("/WaveSynth_FX/freq",voiceAddress,220.0);
           ESP_LOGI(TAG, "going to set gate ON"); 
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,1.0);
           nbDelay(500);
           //vTaskDelay(500 / portTICK_PERIOD_MS); 
           ESP_LOGI(TAG, "going to set gate OFF");            
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,0);
           nbDelay(5000);
           //vTaskDelay(5000 / portTICK_PERIOD_MS);


           
           
           //update_controls(voiceAddress,aDSP);  
           // aDSP->setVoiceParamValue("/WaveSynth_FX/freq",freqs[(scale-4) * 12 + note]);

           //aDSP->setVoiceParamValue("/WaveSynth_FX/freq",voiceAddress,440.0);
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,1.0);
           nbDelay(100);
           //vTaskDelay(100 / portTICK_PERIOD_MS);          
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,0);
           nbDelay(100);
           //vTaskDelay(100 / portTICK_PERIOD_MS);  
           
           //adding subsequent short notes does not work!
           //the strange thing is that when the third note is added, also the two first ones do not fire .....
          
           //aDSP->setVoiceParamValue("/WaveSynth_FX/freq",voiceAddress,440.0);
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,1.0);
           nbDelay(100);
           //vTaskDelay(100 / portTICK_PERIOD_MS);           
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,0);
           nbDelay(100);
           //vTaskDelay(100 / portTICK_PERIOD_MS);  
           
           
           
           
           aDSP->setVoiceParamValue("/WaveSynth_FX/freq",voiceAddress,440.0);
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,1);
           nbDelay(100);
           //vTaskDelay(100 / portTICK_PERIOD_MS);            
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,0);
           nbDelay(100);
           //vTaskDelay(100 / portTICK_PERIOD_MS);  
                      
                      
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,1);
           nbDelay(100);
           //vTaskDelay(100 / portTICK_PERIOD_MS);           
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,0);
           nbDelay(100);
           //vTaskDelay(100 / portTICK_PERIOD_MS);  
           
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,1);
           nbDelay(100);
           //vTaskDelay(100 / portTICK_PERIOD_MS);            
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,0);
           nbDelay(100);
           //vTaskDelay(100 / portTICK_PERIOD_MS);  
           
           
           nbDelay(100);
           //vTaskDelay(100 / portTICK_PERIOD_MS);  
           
           /*
           vTaskDelay(500 / portTICK_PERIOD_MS);          
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",bg_voiceAddress,0);
           vTaskDelay(500 / portTICK_PERIOD_MS); 
           */
           
        }
        ESP_LOGI(TAG, "end of sequence");
           aDSP->deleteVoice(voiceAddress); //delete main voice
  //        aDSP->deleteVoice(bg_voiceAddress); //delete bg voice     
  //         aDSP->deleteVoice(bg_voiceAddress); //delete background voice
};

void play_setVoiceParam_path_nb(DspFaust * aDSP) 
{
    
//must use flexible root of path
//and use only existing UI controls    
bool testpoly = true;

    //uses setVoiceParamValue(path
       static const char *TAG = "PLAY_setVoiceParam_path";
       ESP_LOGI(TAG, "starting play_setVoiceParam_path");
/*
    uintptr_t bg_voiceAddress = aDSP->newVoice(); //create background voice    
    aDSP->setVoiceParamValue("/WaveSynth_FX/gain",bg_voiceAddress,1);  
    aDSP->setVoiceParamValue("/WaveSynth_FX/freq",bg_voiceAddress,110.0);
    aDSP->setVoiceParamValue("/WaveSynth_FX/gate",bg_voiceAddress,1.0);
    vTaskDelay(500 / portTICK_PERIOD_MS);  
*/
uintptr_t bg_voiceAddress;
   if (testpoly) {
    bg_voiceAddress = aDSP->newVoice(); //create main voice    
    aDSP->setVoiceParamValue("/WaveSynth_FX/gain",bg_voiceAddress,1);
    //update_controls(bg_voiceAddress,aDSP);

           aDSP->setVoiceParamValue("/WaveSynth_FX/freq",bg_voiceAddress,110.0);
           ESP_LOGI(TAG, "going to set gate ON for BACKGROUND voice"); 
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",bg_voiceAddress,1.0);
           //vTaskDelay(500 / portTICK_PERIOD_MS);     
 
           nbDelay(1000); 
   }
    
             ESP_LOGI(TAG, "going to call newVoice for main voice"); 
    uintptr_t voiceAddress = aDSP->newVoice(); //create main voice
       ESP_LOGI(TAG, "going to set gain for main voice"); 
    aDSP->setVoiceParamValue("/WaveSynth_FX/gain",voiceAddress,1);
    //update_controls(voiceAddress,aDSP);
        
    aDSP->setVoiceParamValue("/WaveSynth_FX/gain",voiceAddress,1); 
     ESP_LOGI(TAG, "entering play loop"); 
        for (int ii = 50; ii < 52; ii++){
               ESP_LOGI(TAG, "going to update controls"); 
           update_controls(voiceAddress,aDSP);  
           ESP_LOGI(TAG, "going to set frequency 2"); 
           
           aDSP->setVoiceParamValue("/WaveSynth_FX/freq",voiceAddress,220.0);
           ESP_LOGI(TAG, "going to set gate ON for MAIN voice"); 
         aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,1.0);
           //vTaskDelay(500 / portTICK_PERIOD_MS); 
           nbDelay(500); 
           ESP_LOGI(TAG, "going to set gate OFF for MAIN voice");            
          aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,0);
           //vTaskDelay(5000 / portTICK_PERIOD_MS);
           nbDelay(1000); 

           
           
           //update_controls(voiceAddress,aDSP);  
           // aDSP->setVoiceParamValue("/WaveSynth_FX/freq",freqs[(scale-4) * 12 + note]);

           //aDSP->setVoiceParamValue("/WaveSynth_FX/freq",voiceAddress,440.0);
   //        aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,1.0);
           //vTaskDelay(100 / portTICK_PERIOD_MS);          
           nbDelay(100); 
    //       aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,0);
           //vTaskDelay(100 / portTICK_PERIOD_MS);
           nbDelay(100); 
           //adding subsequent short notes does not work!
           //the strange thing is that when the third note is added, also the two first ones do not fire .....
          
           aDSP->setVoiceParamValue("/WaveSynth_FX/freq",voiceAddress,440.0);
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,1.0);
           //vTaskDelay(100 / portTICK_PERIOD_MS); 
 nbDelay(100);            
      aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,0);
           //vTaskDelay(100 / portTICK_PERIOD_MS);
            nbDelay(100);           
           
           
           aDSP->setVoiceParamValue("/WaveSynth_FX/freq",voiceAddress,440.0);
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,1);
           //vTaskDelay(100 / portTICK_PERIOD_MS);          
            nbDelay(100); 
          aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,0);
         
            nbDelay(100);            
                      
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,1);
           //vTaskDelay(100 / portTICK_PERIOD_MS);          
            nbDelay(100); 
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,0);
           //vTaskDelay(100 / portTICK_PERIOD_MS);
            nbDelay(100); 
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,1);
           //vTaskDelay(100 / portTICK_PERIOD_MS);          
            nbDelay(100); 
          aDSP->setVoiceParamValue("/WaveSynth_FX/gate",voiceAddress,0);
      
            nbDelay(100); 
           
           
           //vTaskDelay(100 / portTICK_PERIOD_MS);
            nbDelay(100); 
 if (testpoly){          
           //vTaskDelay(500 / portTICK_PERIOD_MS);   
           ESP_LOGI(TAG, "going to set gate OFF for BACKGROUND voice");            
           aDSP->setVoiceParamValue("/WaveSynth_FX/gate",bg_voiceAddress,0);
           //vTaskDelay(500 / portTICK_PERIOD_MS); 
 nbDelay(100); }
           
          
        }
        ESP_LOGI(TAG, "end of sequence");
        //gently delete the Voice
           aDSP->setVoiceParamValue("/WaveSynth_FX/gain",voiceAddress,0);
           aDSP->deleteVoice(voiceAddress); //delete main voice
  if (testpoly){
 aDSP->setVoiceParamValue("/WaveSynth_FX/gain",bg_voiceAddress,0);     
  aDSP->deleteVoice(bg_voiceAddress); } //delete background voice
};

 

/* 
* setVoiceParamValue ID players
*/

void play_setVoiceParam_Id(DspFaust * aDSP) 
{
ESP_LOGI(TAG, "play_poly_without_midi_Id");

  /*
    //start background voice
    uintptr_t background_voiceAddress = aDSP->newVoice(); //create main voice
    aDSP->setVoiceParamValue(freqId,background_voiceAddress,110.0);
    aDSP->setVoiceParamValue(gainId,background_voiceAddress,0.02);
    ESP_LOGI(TAG, "setVoiceParamValue(freqId,voiceAddress,110) set freq");  //FCKX
    aDSP->setVoiceParamValue(gateId,background_voiceAddress,1);
    ESP_LOGI(TAG, "setVoiceParamValue(gateId,background_voiceAddress,1) GATE ON");  //FCKX
  */
     
    //loop with subsequential triggers on the same voice  
    uintptr_t voiceAddress = aDSP->newVoice(); //create main voice

    for (int ii = 0; ii < 50; ii++){
        
        update_controls(voiceAddress,aDSP);        
        aDSP->setVoiceParamValue(synthFreqBaseId+poly,voiceAddress,440.0);
        ESP_LOGI(TAG, "synthFreqBaseId %d, set freq %6.2f", synthFreqBaseId, 440.0);  //FCKX
        aDSP->setVoiceParamValue(gateBaseId+poly,voiceAddress,1);
        ESP_LOGI(TAG, "gateBaseId %d, set gate %d", gateBaseId, 1);  //FCKX
        nbDelay(500);
        //vTaskDelay(500 / portTICK_PERIOD_MS); 
        ESP_LOGI(TAG, "delay");  //FCKX
        aDSP->setVoiceParamValue(gateBaseId+poly,voiceAddress,0);
        ESP_LOGI(TAG, "gateBaseId %d, set gate %d", gateBaseId, 0);  //FCKX
        nbDelay(500);
        //vTaskDelay(500 / portTICK_PERIOD_MS); 
        ESP_LOGI(TAG, "delay");  //FCKX

        update_controls(voiceAddress,aDSP);        
        aDSP->setVoiceParamValue(synthFreqBaseId+poly,voiceAddress,220.0);
        ESP_LOGI(TAG, "setVoiceParamValue(synthFreqBaseId+poly,voiceAddress,440.0)");  //FCKX
        aDSP->setVoiceParamValue(gateBaseId+poly,voiceAddress,1);
        ESP_LOGI(TAG, "gateBaseId %d, set gate %d", gateBaseId, 1);  //FCKX
        nbDelay(500);
        //vTaskDelay(500 / portTICK_PERIOD_MS); 
        ESP_LOGI(TAG, "delay");  //FCKX
        aDSP->setVoiceParamValue(gateBaseId+poly,voiceAddress,0);
        ESP_LOGI(TAG, "gateBaseId %d, set gate %d", gateBaseId, 0);  //FCKX
        nbDelay(500);
        //vTaskDelay(500 / portTICK_PERIOD_MS); 
        ESP_LOGI(TAG, "delay");  //FCKX
        
        }

          //release background voice
         // aDSP->setVoiceParamValue(gateId,background_voiceAddress,0);
         // ESP_LOGI(TAG, "setVoiceParamValue(gateId,background_voiceAddress,1) GATE OFF");  //FCKX
          
          //clean up voices
           
          aDSP->deleteVoice(voiceAddress);            //delete main voice 
           
          
       //   aDSP->deleteVoice(background_voiceAddress); //delete background voice  
           
};


/* 
* not yet categorized players
*/

/*
* ALTENATIVE API command handler
* called in MQTT events with topics starting with /faust/api/
* it is the intention to reserve this handler for API commands of the faust2api generated DspFaust
*
* Populate the handler using getParamsCount and getParamAddress(id) 
*/
/*
Implement the same on the server side for the wm8978 API
OR:  implement a getParamsCount and getParamAddress(id) based on the received JSONUI.  Put this very useful jsonui code in a separate lib 
*/


//static void call_auto_faust_api(esp_mqtt_event_handle_t event){

static bool call_auto_faust_api(esp_mqtt_event_handle_t event) {  //return true if a command was handled
    static const char *TAG = "AUTO_API";
    int paramsCount = DSP->getParamsCount(); 
    bool paramHandled = false;
    if (paramsCount > 0){
        int id = 0;

        while ((id < paramsCount) &&  (!paramHandled) ){
         ESP_LOGD(TAG,"COMMAND:%.*s\r ", event->topic_len, event->topic); 
         ESP_LOGD(TAG,"CHECKED ADDRESS: %s", DSP->getParamAddress(id) );
          if (strncmp(event->topic, DSP->getParamAddress(id),strlen(DSP->getParamAddress(id))) == 0) {    
             ESP_LOGD(TAG,"HIT!");
             ESP_LOGD(TAG,"DATA:%.*s\r ", event->data_len, event->data);
             //how to make storage generic?  use array with indexid and values address names
             //MORE CHALLENGING: HOW TO MAKE (type dependent) CONVERSION GENERIC?  IS IT NECESSARY TO CONVERT???
             //OR DOES setParamValue accept one specific type??
             //so:  type must be fit to setParamValue already in the controller widget!   NO CONVERSIONS
             //setParamValue ALWAYS ???? accepts float values 
             //so, let widgets send float values!!!!            
           //  someStorage = atof(event->data);     //local storage (to be used as flag for the GUI)  SO someStorage must always be float????
            // DSP->setParamValue(id,someStorage);  //send to DSP
            if (id > 0) {
                //if metronomeVoice exists
                //save the parameter for the metronomeVoice
                //float savedVoiceParam = DSP->getVoiceParamValue(voiceid
                //DSP->getVoiceParamValue(voiceAddress
                
                DSP->setParamValue(id,atof(event->data));
                //if metronomeVoice exists                
                //restore the parameter for the metronomeVoice
                //DSP->setVoiceParamValue(voiceid
                ////DSP->setVoiceParamValue(voiceAddreess , savedVoiceParam
                } 
            else {
                DSP->allNotesOff(false);
                };
                paramHandled = true;
             //try to prevent usage of additional local storage parameters!
             //instead set and get the parameter values with DSP->set and DSP->get functions. These already exist in DspFaust!             
          }          
         /*
         if (strncmp(event->topic, "/faust/api/DspFaust",strlen("/faust/api/DspFaust")) == 0) {
         printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic); }
         */
        //    getParamAddress(id)
        id = id + 1;
        }
        
        
    } else { //paramsCount <= 0
        ESP_LOGD(TAG,"paramsCount <=0");   
    };
    return paramHandled;
};

/*
* API command handler
* called in MQTT events with topics starting with /faust/api/
* it is the intention to reserve this handler for API commands of the faust2api generated DspFaust 
*/

static void call_faust_api(esp_mqtt_event_handle_t event){
    //printf("HANDLING FAUST API CALL=%s\n"," /faust/api");
   
    static const char *TAG = "MQTT_FAUST_API";
    incoming_updates = true;
    if (strncmp(event->topic, "/faust/api/DspFaust",strlen("/faust/api/DspFaust")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else
    
    if (strncmp(event->topic, "/faust/api/DspFaust",strlen("/faust/api/DspFaust")) == 0) {
            ESP_LOGI(TAG,"COMMAND:%.*s\r ", event->topic_len, event->topic);
            ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data); 
            ESP_LOGI(TAG,"...to be implemented..."); 
    } else    
    if (strncmp(event->topic, "/faust/api/start",strlen("/faust/api/start")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/stop",strlen("/faust/api/stop")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/isRunning",strlen("/faust/api/isRunning")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/keyOn",strlen("/faust/api/keyOn")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/keyOff",strlen("/faust/api/keyOff")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/newVoice",strlen("/faust/api/newVoice")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/deleteVoice",strlen("/faust/api/deleteVoice")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/allNotesOff",strlen("/faust/api/allNotesOff")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/propagateMidi",strlen("/faust/api/propagateMidi")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/getJSONUI",strlen("/faust/api/getJSONUI")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/getJSONMeta",strlen("/faust/api/getJSONMeta")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/buildUserInterface",strlen("/faust/api/buildUserInterface")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/getParamsCount",strlen("/faust/api/getParamsCount")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/setParamValue",strlen("/faust/api/SetParamValue")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/getParamValue",strlen("/faust/api/getParamValue")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/setVoiceParamValue",strlen("/faust/api/setVoiceParamValue")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else     
    if (strncmp(event->topic, "/faust/api/getVoiceParamValue",strlen("/faust/api/getVoiceParamValue")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/getParamAddress",strlen("/faust/api/getParamAddress")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/getVoiceParamAddress",strlen("/faust/api/getVoiceParamAddress")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else      
    if (strncmp(event->topic, "/faust/api/DspFaust",strlen("/faust/api/DspFaust")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/DspFaust",strlen("/faust/api/DspFaust")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/getParamMin",strlen("/faust/api/getParamMin")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/getParamMax",strlen("/faust/api/getParamMax")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/getParamInit",strlen("/faust/api/getParamInit")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/getMetaData",strlen("/faust/api/getMetaData")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/propagateAcc",strlen("/faust/api/propagateAcc")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/setAccConverter",strlen("/faust/api/setAccCoverter")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/propagateGyr",strlen("/faust/api/propagateGyr")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/setGyrConverter",strlen("/faust/api/setGyrCoverter")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/getCPULoad",strlen("/faust/api/getCPULoad")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else    
    if (strncmp(event->topic, "/faust/api/configureOSC",strlen("/faust/api/configureOSC")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else 
    if (strncmp(event->topic, "/faust/api/isOSCOn",strlen("/faust/api/isOSCOn")) == 0) {
             printf("HANDLING FAUST API CALL: COMMAND= %s\n",event->topic);
    } else

/*        
    //the following parts fit with the commands for UI elements that have an "address" string defined by the DspFaust API
    //these can be handled in a more generic way, using the functions of in the DspFaust API. See: call_auto_faust_api
    //special attention for the path ending with /gate. This is the only one using a conversion to int i.s.o. float.
    if (strncmp(event->topic, "/Polyphonic/Voices/WaveSynth_FX/A",strlen("/Polyphonic/Voices/WaveSynth_FX/A")) == 0) {
             ESP_LOGI(TAG,"COMMAND:%.*s\r ", event->topic_len, event->topic);
             ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data); 
             synthA = atof(event->data);
             //aDSP->setVoiceParamValue(synthABaseId+poly,voiceAddress, synthA);
             DSP->setParamValue(synthABaseId+1,synthA);
             //aDSP->setParamValue(synthABaseId+1,synthA);
             //aDSP->setParamValue(synthABaseId-1,synthA);
             //
                          
             //ESP_LOGI(TAG,"...to be implemented<<<"); 
    } else
    if (strncmp(event->topic, "/Polyphonic/Voices/WaveSynth_FX/D",strlen("/Polyphonic/Voices/WaveSynth_FX/D")) == 0) {
             ESP_LOGI(TAG,"COMMAND:%.*s\r ", event->topic_len, event->topic);
             ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data);
             synthD = atof(event->data); 
             DSP->setParamValue(synthDBaseId+1,synthD);             
             //ESP_LOGI(TAG,"...to be implemented<<<"); 
    } else     
    if (strncmp(event->topic, "/Polyphonic/Voices/WaveSynth_FX/R",strlen("/Polyphonic/Voices/WaveSynth_FX/R")) == 0) {
             ESP_LOGI(TAG,"COMMAND:%.*s\r ", event->topic_len, event->topic);
             ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data);
             synthR = atof(event->data);
             DSP->setParamValue(synthRBaseId+1,synthR);               
             //ESP_LOGI(TAG,"...to be implemented<<<"); 
    } else     
    if (strncmp(event->topic, "/Polyphonic/Voices/WaveSynth_FX/S",strlen("/Polyphonic/Voices/WaveSynth_FX/S")) == 0) {
             ESP_LOGI(TAG,"COMMAND:%.*s\r ", event->topic_len, event->topic);
             ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data); 
             synthS = atof(event->data);
             DSP->setParamValue(synthSBaseId+1,synthS);               
             //ESP_LOGI(TAG,"...to be implemented<<<"); 
    } else     
    if (strncmp(event->topic, "/Polyphonic/Voices/WaveSynth_FX/bend",strlen("/Polyphonic/Voices/WaveSynth_FX/bend")) == 0) {
             ESP_LOGI(TAG,"COMMAND:%.*s\r ", event->topic_len, event->topic);
             ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data); 
             bend = atof(event->data); 
             DSP->setParamValue(bendBaseId+1,bend);  
    } else  
    if (strncmp(event->topic, "/Polyphonic/Voices/WaveSynth_FX/freq",strlen("/Polyphonic/Voices/WaveSynth_FX/freq")) == 0) {
             ESP_LOGI(TAG,"COMMAND:%.*s\r ", event->topic_len, event->topic);
             ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data);
             synthFreq = atof(event->data);              
             ESP_LOGI(TAG,"...to be implemented<<<"); 
             
    } else
    if (strncmp(event->topic, "/Polyphonic/Voices/WaveSynth_FX/gain",strlen("/Polyphonic/Voices/WaveSynth_FX/gain")) == 0) {
             ESP_LOGI(TAG,"COMMAND:%.*s\r ", event->topic_len, event->topic);
             ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data);
             gain = atof(event->data);
             DSP->setParamValue(gainBaseId+1,gain);    //does not work???? 
             ESP_LOGI(TAG,"...to be implemented<<<");             
    } else  
    if (strncmp(event->topic, "/Polyphonic/Voices/WaveSynth_FX/gate",strlen("/Polyphonic/Voices/WaveSynth_FX/gate")) == 0) {
             ESP_LOGI(TAG,"COMMAND:%.*s\r ", event->topic_len, event->topic);
             ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data); 
             gate= atoi(event->data); //must convert from int  !! ??
             ESP_LOGI(TAG,"...to be implemented<<<"); 
    } else  
    if (strncmp(event->topic, "/Polyphonic/Voices/WaveSynth_FX/lfoFreq",strlen("/Polyphonic/Voices/WaveSynth_FX/lfoFreq")) == 0) {
             ESP_LOGI(TAG,"COMMAND:%.*s\r ", event->topic_len, event->topic);
             ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data);
             lfoFreq = atof(event->data); 
             DSP->setParamValue(lfoFreqBaseId+1,lfoFreq);              
             //ESP_LOGI(TAG,"...to be implemented<<<"); 
    } else  
    if (strncmp(event->topic, "/Polyphonic/Voices/WaveSynth_FX/lfoDepth",strlen("/Polyphonic/Voices/WaveSynth_FX/lfoDepth")) == 0) {
             ESP_LOGI(TAG,"COMMAND:%.*s\r ", event->topic_len, event->topic);
             ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data);
             lfoDepth = atof(event->data);   
             DSP->setParamValue(lfoDepthBaseId+1,lfoDepth);              
             //ESP_LOGI(TAG,"...to be implemented<<<"); 
    } else 
    if (strncmp(event->topic, "/Polyphonic/Voices/WaveSynth_FX/waveTravel",strlen("/Polyphonic/Voices/WaveSynth_FX/waveTravel")) == 0) {
             ESP_LOGI(TAG,"COMMAND:%.*s\r ", event->topic_len, event->topic);
             ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data);
             waveTravel = atof(event->data);
             DSP->setParamValue(waveTravelBaseId+1,waveTravel);
             //ESP_LOGI(TAG,"...to be implemented<<<"); 
    } else  
*/        
    {
        ESP_LOGI(TAG,"HANDLING FAUST API CALL: INVALID COMMAND= %s\n",event->topic); 
        //printf("HANDLING FAUST API CALL: INVALID COMMAND= %s\n",event->topic);
        incoming_updates = false;
          }            
    }

int bool2int( bool mybool){
    int myint;
    if (mybool) {
        myint=1;
    } else {myint =0;};
    return myint;
    
};

/*
* API2 command handler
* called in MQTT events with topics starting with /faust/api2/
* it is the intention to reserve this handler for API2 commands defined for this particular application  
*/
static void call_faust_api2(esp_mqtt_event_handle_t event){
    static const char *TAG = "MQTT_FAUST_API2";
    incoming_updates = true;
    ESP_LOGI(TAG,"HANDLING Faust API2 CALL:%.*s\r ", event->topic_len, event->topic);
    ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data); 
    //printf("HANDLING FAUST API2 CALL=%s\n"," /faust/api2");

    if (strncmp(event->topic, "/faust/api2/gate",strlen("/faust/api2/gate")) == 0) {
            ESP_LOGI(TAG,"...to be implemented..."); 
    } else    
    if (strncmp(event->topic, "/wm8978/hpVolL",strlen("/wm8978/hpVolL")) == 0)        {
             hpVol_L = atoi(event->data); 
             wm8978.hpVolSet(hpVol_R, hpVol_L);                
    } else  
    if (strncmp(event->topic, "/wm8978/hpVolR",strlen("/wm8978/hpVolR")) == 0)        {
             hpVol_R = atoi(event->data);             
             wm8978.hpVolSet(hpVol_R, hpVol_L);
    } else
    if (strncmp(event->topic, "/wm8978/micGain",strlen("/wm8978/micGain")) == 0)        {
             micGain = atoi(event->data); 
             wm8978.micGain(micGain);

    } else  
        //FCKXFCKX booleans.....
    if (strncmp(event->topic, "/wm8978/micen",strlen("/wm8978/micen")) == 0)        {
             micen = atoi(event->data);
             
             ESP_LOGI(TAG,"micen: %d",  bool2int(micen));             
             wm8978.inputCfg(micen ? 1:0, lineinen ? 1:0, auxen ? 1:0 );
    } else
    if (strncmp(event->topic, "/wm8978/lineinen",strlen("/wm8978/lineinen")) == 0)        {
             lineinen = atoi(event->data);  
             ESP_LOGI(TAG,"lineinen: %d ",  lineinen ? 1:0);              
             wm8978.inputCfg(micen ? 1:0, lineinen ? 1:0, auxen ? 1:0 );
    } else 
    if (strncmp(event->topic, "/wm8978/auxen",strlen("/wm8978/auxen")) == 0)        {
             auxen = atoi(event->data); 
             ESP_LOGI(TAG,"auxen: %d",  auxen ? 1:0);              
             wm8978.inputCfg(micen ? 1:0, lineinen ? 1:0, auxen ? 1:0 );
    } else 
    if (strncmp(event->topic, "/wm8978/dacen",strlen("/wm8978/dacen")) == 0)        {
             dacen = atoi(event->data); 
             ESP_LOGI(TAG,"dacen: %d",  dacen ? 1:0);           
             wm8978.outputCfg(dacen ? 1:0, bpen ? 1:0);
    } else 
    if (strncmp(event->topic, "/wm8978/bpen",strlen("/wm8978/bpen")) == 0)        {
             bpen = atoi(event->data);  
             ESP_LOGI(TAG,"bpen: %d",  bpen ? 1:0);             
             wm8978.outputCfg(dacen ? 1:0, bpen ? 1:0);
    } else 
 
        

    //wm8978.auxGain(0);        
/*        
    if (strncmp(event->topic, "/faust/api2/DSP/synthA",strlen("/faust/api2/DSP/synthA")) == 0)        {
             synthA = atof(event->data);             
    } else    
    if (strncmp(event->topic, "/faust/api2/DSP/synthD",strlen("/faust/api2/DSP/synthD")) == 0)       {
             synthD = atof(event->data);             
    } else            
    if (strncmp(event->topic, "/faust/api2/DSP/synthS",strlen("/faust/api2/DSP/synthS")) == 0)        {
             synthS = atof(event->data);             
    } else 
    if (strncmp(event->topic, "/faust/api2/DSP/synthR",strlen("/faust/api2/DSP/synthR")) == 0)        {
             synthR = atof(event->data);             
    } else         
    if (strncmp(event->topic, "/faust/api2/DSP/lfoFreq",strlen("/faust/api2/DSP/lfoFreq")) == 0)        {
             lfoFreq = atof(event->data);             
    } else    
    if (strncmp(event->topic, "/faust/api2/DSP/lfoDepth",strlen("/faust/api2/DSP/lfoDepth")) == 0)        {
             lfoDepth = atof(event->data);             
    } else 
*/        
    if (strncmp(event->topic, "/faust/api2/DSP/poly",strlen("/faust/api2/DSP/poly")) == 0)        {
             poly = atoi(event->data);             
    } else 
    if (strncmp(event->topic, "/faust/api2/DSP/play",strlen("/faust/api2/DSP/play")) == 0)
        {
            play_flag = strncmp(event->data, "false",strlen("false"));
            if (play_flag) { ESP_LOGI(TAG,"play_flag: true");}
            else { ESP_LOGI(TAG,"play_flag: false");} ;          
             //play_flag = atob(event_data); //try to implement playing a song in a separate task             
    } else  
    if (strncmp(event->topic, "/faust/api2/midi/single",strlen("/faust/api2/midi/single")) == 0)        {
             //receive a single midi message 3 bytes, coded by Nodered as a 24 bit integer
             //ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data);
             //int mididata = atoi(event->data);
             execute_single_midi_command(DSP, atoi(event->data));
            
             ESP_LOGI(TAG, "after single command");                          
             
             
             //here , implement playing immediately 
    } else 
        if (strncmp(event->topic, "/faust/api2/rtttl/load",strlen("/faust/api2/rtttl/load")) == 0)        {
             //load a rtttl song
             ESP_LOGW(TAG,"VALUE:%.*s\r ", event->data_len, event->data);
             ESP_LOGE(TAG,"...to be implemented but here it comes...");
             //strcpy(songbuffer,event->data);
            // play_poly_rtttl(event->data, DSP);
             //songbuffer = event->data; songlength = event->data_len;
             //store song in songbuffer             
             //ESP_LOGI(TAG,"...to be implemented..."); 
    } else  
        if (strncmp(event->topic, "/faust/api2/rtttl/play",strlen("/faust/api2/rtttl/play")) == 0)        {
             //play a rtttl song previously loaded into songbuffer
             ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data);
             //start a task that uses play rttl with the contents of songbuffer             
             ESP_LOGI(TAG,"...to be1 implemented 1..."); 
    } else  


        
    if (strncmp(event->topic, "/faust/api2/midi/seq",strlen("/faust/api2/midi/seq")) == 0)        {
             ESP_LOGI(TAG,"...to be implemented...(store msg and) call mqtt_midi"); 
             //poly = atoi(event->data);
             //here, implement storing the (timestamped) midi data (= recording) in a buffer, for later playback (             
             //notes: recording = receive midi data and ADD timestamp based on moment of receipt
             //       this can be data received from an instrument
             //       another function is receiving and storing pre-rorded (=timestamped) data
             //       this can be data from a midi-file streamed by another device
    } else   
    {
             ESP_LOGI(TAG,"COMMAND:%.*s\r ", event->topic_len, event->topic);
             ESP_LOGI(TAG,"VALUE:%.*s\r ", event->data_len, event->data); 
             ESP_LOGI(TAG,">>>INVALID COMMAND<<<"); 
             incoming_updates = false;
          }            
    }

/*
* MQTT callback based on the code in ESP-IDF example xxxxxxxxxx
*/
static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event){  
    //ESP_LOGI(TAG, "FCKX: HANDLER_CB CALLED");  //FCKX
    esp_mqtt_client_handle_t client = event->client;
    //int msg_id;
    int result;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            
            msg_id = esp_mqtt_client_publish(client, "/faust/test/qos1", "test qos1", 0, 1, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/faust/test/qos0", 0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/faust/test/qos1", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_unsubscribe(client, "/faust/test/qos1");
            ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);

            msg_id = esp_mqtt_client_subscribe(client, "/Polyphonic/Voices/#",0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
 
            msg_id = esp_mqtt_client_subscribe(client, "/wm8978/#",0);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id); 
            
            /*
            msg_id = esp_mqtt_client_subscribe(client, "/faust", 0);
            //ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_publish(client, "/faust", "MQTT OK", 0, 1, 0);
            //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            */
            
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGE(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            msg_id = esp_mqtt_client_publish(client, "/faust/test/qos0", "MQTT EVENT SUBSCRIBED", 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            //ESP_LOGI(TAG, "TOPIC=%.*s\r", event->topic_len, event->topic);
            //ESP_LOGI(TAG,"DATA=%.*s\r", event->data_len, event->data);
            //if topic starts with /faust/api or /faust/api2, call a command dispatcher/handler
            if (strncmp(event->topic, "/faust/api2",strlen("/faust/api2")) == 0) {
                call_faust_api2(event);    
                }
            else if (strncmp(event->topic, "/faust/api",strlen("/faust/api")) == 0) {
                call_faust_api(event);
                }                
            else if (strncmp(event->topic, "/Polyphonic/",strlen("/Polyphonic/")) == 0) { //this measn: string starts with ....
                if (!call_auto_faust_api(event)) { //go for checking additional commands only if no DspFaust address handled
                call_faust_api(event);   }; 
                } 
            else if (strncmp(event->topic, "/wm8978/",strlen("/wm8978/")) == 0) {
                call_faust_api2(event);    
                }                  
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGE(TAG, "MQTT_EVENT_ERROR");
            ESP_LOGE(TAG, "Switching allNotesOff to prevent future crash");
            DSP->allNotesOff();
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

/*
* MQTT event handler based on the code in ESP-IDF example xxxxxxxxxx
* throws a compilation error! May be obsolete when registering is done i a different way
*/
/*
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {

    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}
*/

/* MQTT event handler based on the code in ESP-IDF example xxxxxxxxxx
* repair by FCKX of the original code, see above
*/
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, esp_mqtt_event_handle_t event_data) {
    
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%d", base, event_id);
    mqtt_event_handler_cb(event_data);
}

/*
../main/main.cpp: In function 'void mqtt_event_handler(void*, esp_event_base_t, int32_t, void*)':
../main/main.cpp:94:27: error: invalid conversion from 'void*' to 'esp_mqtt_event_handle_t' {aka 'esp_mqtt_event_t*'} [-fpermissive]
     mqtt_event_handler_cb(event_data);
                           ^~~~~~~~~~
*/

/*
//https://github.com/espressif/esp-idf/issues/5248

@mntolia This is an issue with C++ build, has been fixed in espressif/esp-mqtt@8a1e1a5, but apparently that hasn't made it yet to arduino. As a workaround you can cast the variables to the expected types or even avoid using an event loop altogether.
It is still possible to configure mqtt_event_handler_cb() as a plain callback in client config:

    mqtt_config.event_handle = mqtt_event_handler_cb;
And remove both definition of the mqtt_event_handler() and registration of the handler esp_mqtt_client_register_event()

*/

/* MQTT client based on the code in ESP-IDF example ...\esp-idf\examples\protocols\mqtt\tcp
*  some adaptations were necessary
*/
static esp_mqtt_client * mqtt_app_start(void){
//static void mqtt_app_start(void){
   
    //FCKX
    esp_mqtt_client_config_t mqtt_cfg = {0}; // adapted by FCKX, see above
    mqtt_cfg.uri = SECRET_ESP_MQTT_BROKER_URI;
    mqtt_cfg.username = SECRET_ESP_MQTT_BROKER_USERNAME;
    mqtt_cfg.password = SECRET_ESP_MQTT_BROKER_PASSWORD;
    mqtt_cfg.client_id = SECRET_ESP_MQTT_CLIENT_ID;
    
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt_cfg.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt_cfg.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    //mqtt_cfg.event_handle = mqtt_event_handler_cb;
    //esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
    esp_mqtt_client_register_event(client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID, (esp_event_handler_t)mqtt_event_handler, client);  //FCKX, added 2x type casting 
    esp_mqtt_client_start(client);
    ESP_LOGI(TAG, "MQTT CLIENT STARTED");  //FCKX
    
    return client;
}


/* Use of freeRTOS software timers for non-blocking code in sequencing routines (experimental)
*
* Taken from freeRTOS part in ESP-IDF documentation

// Define a callback function that will be used by multiple timer instances.
// The callback function does nothing but count the number of times the
// associated timer expires, and stop the timer once the timer has expired
// 10 times.
*/

/*
void vTimerCallback( TimerHandle_t pxTimer )
{
ESP_LOGI(TAG, "TIMER_CALLBACK");    
int32_t lArrayIndex;
const int32_t xMaxExpiryCountBeforeStopping = 10;
     
    // Optionally do something if the pxTimer parameter is NULL.
    configASSERT( pxTimer );
    // Which timer expired?
    lArrayIndex = ( int32_t ) pvTimerGetTimerID( pxTimer );
    // Increment the number of times that pxTimer has expired.
    lExpireCounters[ lArrayIndex ] += 1;
    ESP_LOGI(TAG, "TIMER %d EXPIRED %d times", lArrayIndex, lExpireCounters[ lArrayIndex ] ); 
    // If the timer has expired 10 times then stop it from running.
    if( lExpireCounters[ lArrayIndex ] == xMaxExpiryCountBeforeStopping )
    {
        // Do not use a block time if calling a timer API function from a
        // timer callback function, as doing so could cause a deadlock!
         xTimerStop( pxTimer, 0 );
         ESP_LOGI(TAG, "TIMER %d stopped because nr of expirations reached %d", lArrayIndex , xMaxExpiryCountBeforeStopping); 
    }
 }

*/


 
void myDelayTimerCallback( TimerHandle_t pxTimer ){
    ESP_LOGI(TAG, "delayTimer_CALLBACK: expired!");    
    xTimerStop( pxTimer, 0 );
    expired = true;  
    xTimerDelete(pxTimer, 0);  //delete the timer immediately to prevent stack overflow
                               //this is useful for these timers that are created randomly  
 }

void non_blocking_Delay( int myTicks){
//non_blocking delay based on freertos timer
 TimerHandle_t delayTimer;

//create timer
        ESP_LOGI(TAG, "Creating delay timer");
        expired = false;         
        delayTimer = xTimerCreate(    "Timer",                // Just a text name, not used by the kernel.
                                         myTicks  ,           // The timer period in ticks.
                                         pdFALSE,             // The timer will not auto-reload itself when it expires.
                                         ( void * ) 1,        // Assign each timer a unique id equal to its array index.
                                         myDelayTimerCallback // Each timer calls the same callback when it expires.
                                     );

         if( delayTimer == NULL )
         {
             // The timer was not created.
             ESP_LOGI(TAG, "Timer was not created!");
         }
         else
         {
             // Start the timer.  No block time is specified, and even if one was
             // it would be ignored because the scheduler has not yet been
             // started.
             ESP_LOGI(TAG, "delayTimer to be started!");
             if( xTimerStart( delayTimer, 0 ) != pdPASS )
             {
                 // The timer could not be set into the Active state.
                 ESP_LOGI(TAG, "delayTimer could not be set into the Active state");
             }
         }
//stay in non-blocking delay loop until expired
while(!expired) {
    vTaskDelay(1);
}
} //

void metronome_keyOff_CB( TimerHandle_t pxTimer ){ 
    static const char *TAG = "METRONOME_KEYOFF_CB";   
    ESP_LOGI(TAG, "metronomeOffTimer_CALLBACK: expired!");

    int32_t timerId = ( int32_t ) pvTimerGetTimerID( pxTimer );

    //keyOff
    DSP->setVoiceParamValue("/WaveSynth_FX/gain",metronomeVoiceAddress,0);           
    DSP->setVoiceParamValue("/WaveSynth_FX/gate",metronomeVoiceAddress,0); 
    DSP->deleteVoice(metronomeVoiceAddress);

    //metronome_keyOff_expired = true;    
 }


TimerHandle_t keyOffTimer;


void create_metronome_keyOff_timer(int metronomeMS){
  static const char *TAG = "CREATE_METRONOME_KEYOFF";   

//create timer
//later: create it only once, but start it when desired

     
        metronome_keyOff_expired = false;         
        keyOffTimer = xTimerCreate(    "metronome_key_OffTimer",            // Just a text name, not used by the kernel.
                                         (metronomeMS+10)/portTICK_PERIOD_MS  ,           // The timer period in ticks.
                                         pdFALSE,             // The timer will not auto-reload itself when it expires.
                                         ( void * ) 2,        // Assign each timer a unique id equal to its array index.
                                         metronome_keyOff_CB  // Each timer calls the same callback when it expires.
                                     );

         if( keyOffTimer == NULL )
         {
             // The timer was not created.
             ESP_LOGI(TAG, "OffTimer was not created!");
         }
         else
         {
             // Start the timer.  No block time is specified, and even if one was
             // it would be ignored because the scheduler has not yet been
             // started.
             ESP_LOGI(TAG, "metronomeOFFTimer to be started!");
             if( xTimerStart( keyOffTimer, 0 ) != pdPASS )
             {
                 // The timer could not be set into the Active state.
                 ESP_LOGI(TAG, "metronomeTimer could not be set into the Active state");
             }
         }        
          
         
//stay in non-blocking delay loop until expired  NOT APPLICABLE TO METRONOME !!!!
/*
while(!expired) {
    vTaskDelay(1);
}    
*/      
    
}; //start_metronome_keyOff

void start_metronome_keyOff(int metronomeMS){
static const char *TAG = "START_METRONOME_KEYOFF";   
//non_blocking delay based on freertos timer


         if( keyOffTimer == NULL )
         {
             // The timer was not created.
             ESP_LOGI(TAG, "metronome keyOff timer does not exist, create it");
             create_metronome_keyOff_timer(metronomeMS);
         }
         else
         {
             // Start the timer.  No block time is specified, and even if one was
             // it would be ignored because the scheduler has not yet been
             // started.
             ESP_LOGI(TAG, "metronome keyOff Timer to be started!");
             if( xTimerStart( keyOffTimer, 0 ) != pdPASS )
             {
                 // The timer could not be set into the Active state.
                 ESP_LOGE(TAG, "metronomeTimer could not be set into the Active state");
             }
         }        
          
         
//stay in non-blocking delay loop until expired  NOT APPLICABLE TO METRONOME !!!!
/*
while(!expired) {
    vTaskDelay(1);
}    
*/      
    
}; //start_metronome_keyOff

void play_metronome(bool measureTick) {
    float mF;
    if (measureTick){mF = 200;} else {mF = 100;} ;
    //keyOn for metronome;
    metronomeVoiceAddress = DSP->newVoice(); //create main voice};
    //find a way to give the metronome voice it's own envelope
    /*
    DSP->setVoiceParamValue("/WaveSynth_FX/A",metronomeVoiceAddress,0.01);
    DSP->setVoiceParamValue("/WaveSynth_FX/D",metronomeVoiceAddress,2);
    DSP->setVoiceParamValue("/WaveSynth_FX/R",metronomeVoiceAddress,2);
    DSP->setVoiceParamValue("/WaveSynth_FX/S",metronomeVoiceAddress,0.2);
    */
    
    //try to make this code generic. Don't use a literal path string    
    DSP->setVoiceParamValue("/WaveSynth_FX/freq",metronomeVoiceAddress,mF);
    DSP->setVoiceParamValue("/WaveSynth_FX/gain",metronomeVoiceAddress,0.2);           
    DSP->setVoiceParamValue("/WaveSynth_FX/gate",metronomeVoiceAddress,1);   
    
    //start keyOff timer for metronome
    start_metronome_keyOff(50);
    
};

void handle_beat(void){
    static const char *TAG = "HANDLE_BEAT";   
    beatCount = beatCount+1; 
    ESP_LOGI(TAG, "timesig_num %d ", timesig_num);

    int beatInMeasureCount =  beatCount % timesig_num;
    if (beatInMeasureCount == 0) {measureCount = measureCount + 1;};
    int measureInLoopCount = measureCount % loopLength;
    if (measureInLoopCount == 0) {
        loopCount = loopCount + 1;
        loopStart = xTaskGetTickCount(); //record start time of loop
    };
    
    //metronome_mode = 0x00; //switches off metronome
    //the metronome plays, even if the corresponding bit in seq_mode is not set !!!
   // ESP_LOGW(TAG,"seq_mode %d", seq_mode);
   // ESP_LOGW(TAG,"have a look at mode selection method!");
   // ESP_LOGW(TAG,"seq_mode && metronome_mode %d", seq_mode && metronome_mode);
  //  ESP_LOGW(TAG,"seq_mode & metronome_mode %d", seq_mode & metronome_mode);

    // & represents bitwise AND,   && is logical AND   
  handle_Queue(DSP);
    if (!(seq_mode & metronome_mode) == 0) {
        play_metronome(beatInMeasureCount == 0);  //play metronome with a frequency depending on end of measure
 //handle_Queue(DSP);
        };
    if ((measureInLoopCount == 0) && (beatInMeasureCount == 0)){ //on every new loop, handle the queue of recorded events, but only in first beat of the loop
        //handle_Queue(DSP);
        } 

    ESP_LOGI(TAG, "beatCount %d  beatInMeasureCount %d  measureCount %d ", beatCount, beatInMeasureCount, measureCount);
    ESP_LOGI(TAG, "xTaskGetTickCount() %d ", xTaskGetTickCount());
    int loopTime = xTaskGetTickCount()-loopStart;
    ESP_LOGI(TAG, "xTaskGetTickCount()-loopStart %d ", loopTime);
    ESP_LOGI(TAG, "portTICK_PERIOD_MS %d ", portTICK_PERIOD_MS);    
};

void beat_CB( TimerHandle_t pxTimer ){
    
    int32_t timerId = ( int32_t ) pvTimerGetTimerID( pxTimer );
    static const char *TAG = "BEAT_CB";   
    ESP_LOGI(TAG, "BEAT_CALLBACK: expired! timerId: %d", timerId);

    if (timerId == 1) {
        ESP_LOGI(TAG, "timerId OK in beat_CB");
        handle_beat();
        
        } 
    else {  
        ESP_LOGE(TAG, "Unknown timerId in beat_CB");
        }

    //xTimerStop( pxTimer, 0 );
   
 }

void stop_beat(int ticksToWait){
        //after waiting for tickToWait ticks, stop the beat timer
        xTimerStop(beatTimer, ticksToWait); 

        //must also stop metronome timer gently and send keyOff        
    
};

void start_beat(float tempo,int timesig_num_in, int timesig_denom_in){
static const char *TAG = "START_BEAT";   
//non_blocking delay based on freertos timer

beatCount = 0;
measureCount = 0;
timesig_num = timesig_num_in;  //these variables must be initialized somewhere else later
timesig_denom = timesig_denom_in;
tempo_scale = 1;
if (beatTimer == NULL) {
    //create timer
    ESP_LOGI(TAG, "Creating beat timer");
    beat_expired = false;      
    beatTimer = xTimerCreate(        "beatTimer",            // Just a text name, not used by the kernel.
                                     60*1000/tempo/tempo_scale/portTICK_PERIOD_MS  ,           // The timer period in ticks.
                                     pdTRUE,             // The timer auto-reload itself when it expires.
                                     ( void * ) 1,        // Assign each timer a unique id equal to its array index.
                                     beat_CB  // Each timer calls the same callback when it expires.
                                  );
    }

 if(beatTimer == NULL )
 {
     // The timer was not created.
     ESP_LOGI(TAG, "beatTimer was not created!");
 }
 else
 {
     // Start the timer.  No block time is specified, and even if one was
     // it would be ignored because the scheduler has not yet been
     // started.
     loopStart = xTaskGetTickCount();
     ESP_LOGI(TAG, "beatTimer to be started!");
     if( xTimerStart( beatTimer, 0 ) != pdPASS ) {
         // The timer could not be set into the Active state.
         ESP_LOGI(TAG, "beatTimer could not be set into the Active state");
     }
 }
         
         
//stay in non-blocking delay loop until expired  NOT APPLICABLE TO METRONOME !!!!
/*
while(!expired) {
    vTaskDelay(1);
}    
*/      
    
}; //start_beat

void metronome_CB( TimerHandle_t pxTimer ){    
static const char *TAG = "METRONOME_OFF_CB";   
ESP_LOGI(TAG, "metronomeOffTimer_CALLBACK: expired!");
int32_t timerId = ( int32_t ) pvTimerGetTimerID( pxTimer );
if (timerId == 1) { //keyOn
    metronomeVoiceAddress = DSP->newVoice(); //create main voice};
    DSP->setVoiceParamValue("/WaveSynth_FX/A",metronomeVoiceAddress,0.01);
    DSP->setVoiceParamValue("/WaveSynth_FX/D",metronomeVoiceAddress,2);
    DSP->setVoiceParamValue("/WaveSynth_FX/R",metronomeVoiceAddress,2);
    DSP->setVoiceParamValue("/WaveSynth_FX/S",metronomeVoiceAddress,0.2);

    DSP->setVoiceParamValue("/WaveSynth_FX/freq",metronomeVoiceAddress,50);
    DSP->setVoiceParamValue("/WaveSynth_FX/gain",metronomeVoiceAddress,1);           
    DSP->setVoiceParamValue("/WaveSynth_FX/gate",metronomeVoiceAddress,1); 
    } 
else { if (timerId == 2) { //keyOff
    DSP->setVoiceParamValue("/WaveSynth_FX/gain",metronomeVoiceAddress,0);           
    DSP->setVoiceParamValue("/WaveSynth_FX/gate",metronomeVoiceAddress,0); 
    DSP->deleteVoice(metronomeVoiceAddress);
} }

//xTimerStop( pxTimer, 0 );
//metronome_keyOff_expired = true;    

 }

void start_metronome(int metronomeMS){
    static const char *TAG = "METRONOME_STARTER";   
    TimerHandle_t keyOnTimer;
    TimerHandle_t keyOffTimer;
    //create timers
    ESP_LOGI(TAG, "Creating metronome timer");
    metronome_keyOn_expired = false;         
    keyOnTimer = xTimerCreate(    "metronomeONTimer",            // Just a text name, not used by the kernel.
                                     metronomeMS/portTICK_PERIOD_MS  ,           // The timer period in ticks.
                                     pdTRUE,             // The timer will not auto-reload itself when it expires.
                                     ( void * ) 1,        // Assign each timer a unique id equal to its array index.
                                     metronome_CB  // Each timer calls the same callback when it expires.
                                 );

    if( keyOnTimer == NULL ) {
         // The timer was not created.
         ESP_LOGI(TAG, "OnTimer was not created!");
        }
    else {
         // Start the timer.  No block time is specified, and even if one was
         // it would be ignored because the scheduler has not yet been
         // started.
         ESP_LOGI(TAG, "metronomeOnTimer to be started!");
         if( xTimerStart( keyOnTimer, 0 ) != pdPASS )
         {
             // The timer could not be set into the Active state.
             ESP_LOGI(TAG, "metronomeTimer could not be set into the Active state");
         }
     }
         
        //create timer 
        metronome_keyOff_expired = false;         
        keyOffTimer = xTimerCreate(    "metronomeOFFTimer",            // Just a text name, not used by the kernel.
                                         (metronomeMS+10)/portTICK_PERIOD_MS  ,           // The timer period in ticks.
                                         pdTRUE,             // The timer will not auto-reload itself when it expires.
                                         ( void * ) 2,        // Assign each timer a unique id equal to its array index.
                                         metronome_CB  // Each timer calls the same callback when it expires.
                                     );

         if( keyOffTimer == NULL )
         {
             // The timer was not created.
             ESP_LOGI(TAG, "OffTimer was not created!");
         }
         else
         {
             // Start the timer.  No block time is specified, and even if one was
             // it would be ignored because the scheduler has not yet been
             // started.
             ESP_LOGI(TAG, "metronomeOFFTimer to be started!");
             if( xTimerStart( keyOffTimer, 0 ) != pdPASS )
             {
                 // The timer could not be set into the Active state.
                 ESP_LOGI(TAG, "metronomeTimer could not be set into the Active state");
             }
         }        
                 
        //stay in non-blocking delay loop until expired  NOT APPLICABLE TO METRONOME !!!!
        /*
        while(!expired) {
            vTaskDelay(1);
        }    
        */      
    
}; //start_metronome

void app_main(void){
    
    static const char *TAG = "APP_MAIN"; 


    esp_log_level_set("*", ESP_LOG_ERROR);
    esp_log_level_set("APP_MAIN", ESP_LOG_ERROR);
    esp_log_level_set("NimBLE", ESP_LOG_ERROR);
    esp_log_level_set("HANDLE_QUEUE", ESP_LOG_VERBOSE);
    esp_log_level_set("DSPFAUST", ESP_LOG_ERROR);
    //esp_log_level_set("DSPFAUST", ESP_LOG_VERBOSE);
    esp_log_level_set("PLAY_POLY_RTTTL_CHORDS", ESP_LOG_ERROR);
    esp_log_level_set("I2S", ESP_LOG_ERROR);
    
    esp_log_level_set("AUTO_API", ESP_LOG_VERBOSE);
    esp_log_level_set("event", ESP_LOG_ERROR);  //MQTT event 

    esp_log_level_set("MQTT_FAUST", ESP_LOG_INFO);
    
/*
    esp_log_level_set("MQTT_FAUST_API", ESP_LOG_WARN);
    
    esp_log_level_set("MQTT_EXAMPLE", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_TCP", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT_SSL", ESP_LOG_VERBOSE);
    esp_log_level_set("TRANSPORT", ESP_LOG_VERBOSE);
    esp_log_level_set("OUTBOX", ESP_LOG_VERBOSE);
    */
    
    
    esp_log_level_set("*", ESP_LOG_ERROR);
    esp_log_level_set("MIDI_EVENT_CB", ESP_LOG_WARN);
    esp_log_level_set("MQTT_CLIENT", ESP_LOG_DEBUG);
    
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());
    
    
    esp_log_level_set("update_controls", ESP_LOG_ERROR);
    
  //  ESP_ERROR_CHECK(nvs_flash_init());
  //  ESP_ERROR_CHECK(esp_netif_init());
  //  ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    //ESP_ERROR_CHECK(example_connect());
    /*   
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    */
    
 /*   
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    wifi_init_sta();
    esp_mqtt_client_handle_t  mqtt_client =  mqtt_app_start();
    //esp_mqtt_client_handle_t  mqtt_client = 0; //to turn MQTT OFF
 */   
    
/**********************************************************
//CODE imported from esp_nimble_client_V2
***********************************************************/    
    
    
  printf("Starting BLE Client application...\n");

  BLEDevice::init("esp-nimble-client");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.

  BLEScan* pBLEScan = BLEDevice::getScan();
  
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);
  
  xTaskCreate(connectTask, "connectTask", 5000, NULL, 1, NULL);
  pBLEScan->start(5, false); 
   
/**********************************************************
//end of CODE imported from esp_nimble_client_V2
***********************************************************/    
    
    
    
    
    
    
    
    
    
    
    
    ESP_LOGW(TAG,"MQTT client started audio codec initialized"); 

    WM8978 wm8978;
    wm8978.init();
    wm8978.addaCfg(1,1); 
    wm8978.inputCfg(1,0,0);     
    wm8978.outputCfg(1,0); //da enabled ,  mic/linein/aux disabled
    wm8978.micGain(50);
    wm8978.auxGain(0);
    wm8978.lineinGain(0);
    wm8978.spkVolSet(0);
    wm8978.hpVolSet(hpVol_L,hpVol_R);
    wm8978.i2sCfg(2,0);
    ESP_LOGW(TAG,"WM8978 audio codec initialized");
    
 //   YOU MUST USE faust2api API calls
    int SR = 9600;  //48000 
    int BS = 32; //was 8   //32
    nbDelay(100);
    DSP = new DspFaust(SR,BS); 
    //nbDelay(100); //doesn't help
    DSP->start();
    //nbDelay(100); //doesn't help
    if (DSP->isRunning()) {
        ESP_LOGW(TAG,"DSP is running"); 
        /*
        if (mqtt_client) {            
            //subscribe to some MQTT topics
            msg_id = esp_mqtt_client_publish(mqtt_client, "/faust", "init OK!", 0, 0, 0);
            ESP_LOGW(TAG,"Sent SUCCES MQTT message to Nodered"); 
            msg_id = esp_mqtt_client_subscribe(mqtt_client, "/faust/api/#", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            msg_id = esp_mqtt_client_subscribe(mqtt_client, "/faust/api2/#", 1);
            ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
            //send JSONUI to Nodered
            msg_id = esp_mqtt_client_publish(mqtt_client, "/faust/jsonui", DSP->getJSONUI(), 0, 0, 0);  //to be implemented: publish the UI by remote request

          } else {
                 ESP_LOGW(TAG,"MQTT client not available"); 
            }
          */  
    } else {
        ESP_LOGE(TAG,"DSP not running");
        /*
        if (mqtt_client){
            msg_id = esp_mqtt_client_publish(mqtt_client, "/faust", "init NOK", 0, 0, 0);
            ESP_LOGW(TAG,"Sent Dsp absence message to Nodered");           
            } else {
                 ESP_LOGW(TAG,"MQTT client not available"); 
            }
            */
        } ;
        
tempo = 120;
loopLength = 1;        
timesig_num = 5;
beatDuration = 60*1000/tempo/portTICK_PERIOD_MS;
measureDuration = timesig_num * beatDuration;
loopDuration = loopLength*timesig_num*beatDuration;



/*
           //list tasks
           ESP_LOGI(TAG, "vTaskList()");
          char * thelist = "012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789"; 
          vTaskList(thelist);
          //print(thelist);
          ESP_LOGI(TAG, "The List %s", thelist );
*/
    bool even = true;
    printf("                    ");
    ESP_LOGI(TAG,"--------------------"); 
         char TaskListBuffer[400];
   ESP_LOGI(TAG,     "Name	State	Priority	High Water	Stack Number");
       vTaskList( TaskListBuffer);
       ESP_LOGI(TAG,"\n\n%s", TaskListBuffer); 
    ESP_LOGI(TAG,"--------------------"); 

/*
FCKXSequence seq;

ESP_LOGI(TAG,"-------seq defined-------------");
ESP_LOGI(TAG,"fresh beat: %5.1f", seq.getTempo());
seq.setTempo(55);
ESP_LOGI(TAG,"user defined beat: %5.1f", seq.getTempo());
*/









start_beat(TEMPO ,TIMESIG_NUM,TIMESIG_DENOM);


    while(1) {
            if (even) {
                printf("\r                       ");
                printf("\r<<<<<Loop TIME>>>>>"); 
                } 
                else {
                 printf("\r                   ");
                 printf("\r>>>>>Loop TIME<<<<<");};

    
  
       
       while(play_flag){
           /*
                if (mqtt_client){
                msg_id = esp_mqtt_client_publish(mqtt_client, "/faust", "song loop started", 0, 0, 0);
                }
                
            */    
         //    handle_Queue(DSP);   
      
               //players based on different ways of starting a tone
/* 
* keyOn / keyOff players
*/           
             //how to update controls??  VERY PREF OUTSIDE THE PLAYER
       
               //play_keys(DSP);                 // OK clean ladder
     //  play_keys_nb(DSP);              // OK with distortions
               //play_keys2(DSP);                // NOK overlapping keys, hangs?            

/* 
* propagateMidi players
*/
  //play_midi_rtttl_chords(song, DSP);

               //play_midi(DSP);  //OK with distortions at long release times midi over Nodered
 
/* 
* setVoiceParamValue path players
*/
             //play_setVoiceParam_path_nb(DSP);  //OK no distortions at all
//>>>play_poly_rtttl(song, DSP);       //OK, clean responds to controls 
          // test_fckx_sequencer_lib();
       //    start_metronome(5000);
         //  play_poly_rtttl_chords(song, DSP);       //OK, clean responds to controls                                   
                                                  
                  //play_poly_rtttl(songbuffer, DSP);         //not tested                         
             //play_setVoiceParam_path(DSP);        //OK clean responds to controls, not flawless
            
/* 
* setParamValue path players
*/

               //play_mono_rtttl(song, DSP);     // NOK crashes

/* 
* setVoiceParamValue ID players
*/

               //play_setVoiceParam_Id(DSP);     //OK, clean responds cleanly to controls


       //         msg_id = esp_mqtt_client_publish(mqtt_client, "/faust", "song loop finished", 0, 0, 0);
                }
                
                
            //update_controls();
            even = !even;
            vTaskDelay(500 / portTICK_PERIOD_MS);

        }; //while(play_flag)

    // Waiting forever, but code is never reached
    vTaskSuspend(nullptr);   
    
}
