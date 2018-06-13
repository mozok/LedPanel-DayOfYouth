/*
    LED Panel for Day Of Youth selebration

    working on STM32F103 via ESP module

    Developer: Mozok Evgen - mozokevgen@gmail.com
*/
#include <itoa.h>
#include <ELClient.h>
#include <ELClientCmd.h>
#include <ELClientMqtt.h>

#pragma region DMD-initialize

#include <SPI.h> //SPI.h must be included as DMD is written by SPI (the IDE complains otherwise)
#include <DMDSTM.h>

#include "UkrRusSystemFont5x7.h" //small Font
#include "UkrRusArial14.h"       //big font

//Fire up the DMD library as dmd
SPIClass SPI_2(2);
#define DISPLAYS_ACROSS 2
#define DISPLAYS_DOWN 2
DMD dmd(DISPLAYS_ACROSS, DISPLAYS_DOWN);

const byte imgTree[] = {0x00, 0x00, 0x00, 0x80, 0xe0, 0xb8, 0xf6, 0xfd, 0xee, 0xb8, 0xe0, 0x80, 0x00, 0x00, 0x00, 0x30, 0x78, 0xfe, 0xed, 0xff, 0xdf, 0xff, 0xfe, 0xf7, 0xbf, 0xfd, 0xff, 0xde, 0x78, 0x30};
const byte imgSnowMan[] = {0x40, 0x60, 0x80, 0x80, 0x00, 0x1c, 0x22, 0xc9, 0xcd, 0xc9, 0xc5, 0xc1, 0x22, 0x1c, 0x00, 0x80, 0x80, 0x60, 0x40, 0x00, 0x00, 0x00, 0x00, 0x1d, 0x22, 0x41, 0x80, 0x80, 0x80, 0x83, 0x80, 0x41, 0x22, 0x1d, 0x00, 0x00, 0x00, 0x00};

/*--------------------------------------------------------------------------------------
  Interrupt handler for Timer1 (TimerOne) driven DMD refresh scanning, this gets
  called at the period set in Timer1.initialize();
--------------------------------------------------------------------------------------*/
void ScanDMD()
{
    dmd.scanDisplayBySPI(SPI_2);
}

#pragma endregion

#pragma region ESPinit
// Initialize a connection to esp-link using the normal hardware Serial2 port both for
// SLIP and for debug messages.
ELClient esp(&Serial2, &Serial2);

// Initialize CMD client (for GetTime)
ELClientCmd cmd(&esp);

// Initialize the MQTT client
ELClientMqtt mqtt(&esp);

// Callback made from esp-link to notify of wifi status changes
// Here we just print something out for grins
void wifiCb(void *response)
{
    ELClientResponse *res = (ELClientResponse *)response;
    if (res->argc() == 1)
    {
        uint8_t status;
        res->popArg(&status, 1);

        if (status == STATION_GOT_IP)
        {
            Serial2.println("WIFI CONNECTED");
        }
        else
        {
            Serial2.print("WIFI NOT READY: ");
            Serial2.println(status);
        }
    }
}

bool connected;

char panel[] = "/DOYPanel/command\0"; //topic to subscribe, chage in setup() according to devID

// Callback when MQTT is connected
void mqttConnected(void *response)
{
    Serial2.println("MQTT connected!");
    mqtt.subscribe(panel);

    connected = true;
}

// Callback when MQTT is disconnected
void mqttDisconnected(void *response)
{
    Serial2.println("MQTT disconnected");
    connected = false;
}

//Callback when an MQTT message arrives for one of our subscriptions
void mqttData(void *response)
{
    //dmd.end();
    ELClientResponse *res = (ELClientResponse *)response;

    Serial2.print("Received: topic=");
    // String topic = res->popString();
    char topic[res->argLen() + 1];
    res->popChar(topic);
    Serial2.println(topic);

    Serial2.print("data=");
    char data[res->argLen() + 1];
    res->popChar(data);
    Serial2.println(data);

    // modeSwitch(data);
}

void mqttPublished(void *response)
{
    Serial2.println("MQTT published");
}

// Callback made form esp-link to notify that it has just come out of a reset. This means we
// need to initialize it!
void resetCb(void)
{
    //Serial2.println("EL-Client (re-)starting!");
    bool ok = false;
    do
    {
        ok = esp.Sync(); // sync up with esp-link, blocks for up to 2 seconds
        if (!ok)
            Serial2.println("EL-Client sync failed!");
    } while (!ok);
    Serial2.println("EL-Client synced!");
}
#pragma endregion

void setup()
{
    Serial2.begin(9600);

    SPI_2.begin();                          // Initialize the SPI_2 port.
    SPI_2.setBitOrder(MSBFIRST);            // Set the SPI_2 bit order
    SPI_2.setDataMode(SPI_MODE0);           // Set the  SPI_2 data mode 0
    SPI_2.setClockDivider(SPI_CLOCK_DIV64); // Use a different speed to SPI 1
    pinMode(SPI2_NSS_PIN, OUTPUT);

    dmd.clearScreen(true); //true is normal (all pixels off), false is negative (all pixels on)
    
    // Sync-up with esp-link, this is required at the start of any sketch and initializes the
    // callbacks to the wifi status change callback. The callback gets called with the initial
    // status right after Sync() below completes.
    esp.wifiCb.attach(wifiCb); // wifi status change callback, optional (delete if not desired)
    esp.resetCb = resetCb;
    resetCb();
	
    // Set-up callbacks for events and initialize with es-link.
    mqtt.connectedCb.attach(mqttConnected);
    mqtt.disconnectedCb.attach(mqttDisconnected);
    mqtt.publishedCb.attach(mqttPublished);
    mqtt.dataCb.attach(mqttData);
    mqtt.setup();
}

bool initialStart = true;

void loop()
{
    esp.Process();

    if (connected && initialStart)
    {
        delay(200);
        initialStart = false;

        //  Led Panel setup
        Timer3.setMode(TIMER_CH1, TIMER_OUTPUTCOMPARE);
        Timer3.setPeriod(3000);          // in microseconds
        Timer3.setCompare(TIMER_CH1, 1); // overflow might be small
        Timer3.attachInterrupt(TIMER_CH1, ScanDMD);

        //clear/init the DMD pixels held in RAM
        dmd.clearScreen(true); //true is normal (all pixels off), false is negative (all pixels on)
        dmd.selectFont(UkrRusArial_14);
        dmd.setBrightness(5000);
        // ESPGetTime();

        // timerGetTime = millis();
        // timerScreenChange = millis();
    }
	
}
