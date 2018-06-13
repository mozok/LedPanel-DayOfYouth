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

const char time1[] = {"Поточний час\0"};       //1st row for time screen
char time2[] = {"00:00\0"};                    //current time from getTime

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
        ESPGetTime();

        // timerGetTime = millis();
        // timerScreenChange = millis();
    }
	
}

//call SNTP server for current time
void ESPGetTime()
{
    uint32_t t = cmd.GetTime();
    // Serial2.print("Get Time: ");
    // Serial2.println(t);

    char t2[3];

    itoa(((t % 86400L) / 3600), t2, 10);
    // Serial2.print("Hour: ");   //DEBUG
    // Serial2.println(t2);   //DEBUG
    strcpy(time2, t2); // get the hour (86400 equals secs per day)
    strcat(time2, ":");
    //Serial2.println((t % 3600) / 60); //DEBUG
    //Serial2.println(time2);           //DEBUG
    if (((t % 3600) / 60) < 10)
    {
        Serial2.println("Add 0"); //DEBUG
        // In the first 10 minutes of each hour, we'll want a leading '0'
        strcat(time2, "0");
    }
    //else {Serial2.println("min > 10");}    //DEBUG
    // Serial2.println(time2); //DEBUG
    itoa((t % 3600) / 60, t2, 10);
    // Serial2.print("Min: ");   //DEBUG
    // Serial2.println(t2);   //DEBUG
    strcat(time2, t2); // print the minute (3600 equals secs per minute)

    // Serial2.print("Time: ");   //DEBUG
    // Serial2.println(time2);   //DEBUG

    dmd.clearScreen(true);
    dmd.selectFont(UkrRusArial_14);

    dmd.drawString(12, 0, time1, /*sizeof(time1) / sizeof(*time1)*/ strlen(time1), GRAPHICS_NORMAL);
    dmd.drawString(34, 8, time2, /*sizeof(time2) / sizeof(*time2)*/ strlen(time2), GRAPHICS_NORMAL);
}

/*
  parse data for Ukrainian characters
  rus and ukr characters consist of 2 bytes, so we ignore 1st byte that are > then 0xCF
*/
void strChange(char *strToChange, char *strNew)
{
    byte lastChar;
    //char * buffer2 = malloc(strlen(buffer)+1);

    byte n = 0;

    while (*strToChange != '\0')
    {
        if ((byte)*strToChange < 0xCF)
        {
            strNew[n] = *strToChange;
            n++;
            strToChange++;
        }
        else
        {
            lastChar = *strToChange;
            strToChange++;

            switch (lastChar)
            {
            case 0xD0:
            {
                switch ((byte)*strToChange)
                {
                case 0x84: //D084 - Є
                {
                    strNew[n] = 0xC0;
                    n++;
                    strToChange++;
                    break;
                }
                case 0x86: //D086 І
                {
                    strNew[n] = 0xC1;
                    n++;
                    strToChange++;
                    break;
                }
                case 0x87: //D087 Ї
                {
                    strNew[n] = 0xC2;
                    n++;
                    strToChange++;
                    break;
                }
                case 0x81: //D081 Ё
                {
                    strNew[n] = 0x95; //Е
                    n++;
                    strToChange++;
                    break;
                }
                }
                break;
            }
            case 0xD1:
            {
                switch ((byte)*strToChange)
                {
                case 0x94: //D196 є
                {
                    strNew[n] = 0xC3;
                    n++;
                    strToChange++;
                    break;
                }
                case 0x96: //D196 і
                {
                    strNew[n] = 0xC4;
                    n++;
                    strToChange++;
                    break;
                }
                case 0x97: //D197 ї
                {
                    strNew[n] = 0xC5;
                    n++;
                    strToChange++;
                    break;
                }
                case 0x91: //D191 ё
                {
                    strNew[n] = 0xB5; //е
                    n++;
                    strToChange++;
                    break;
                }
                }

                break;
            }
            case 0xD2:
            {
                switch ((byte)*strToChange)
                {
                case 0x90: //D290 Ґ
                {
                    strNew[n] = 0x93; //замінюю на Г
                    n++;
                    strToChange++;
                    break;
                }
                case 0x91: //D291 ґ
                {
                    strNew[n] = 0xB3; //замінюю на г
                    n++;
                    strToChange++;
                    break;
                }
                }
                break;
            }
            }
        }
    }
    strNew[n] = '\0';
    // strcpy(strNew, strNew);

    // Serial2.print("buffer2: ");   //DEBUG
    // Serial2.println(buffer2);

    //return buffer2;
}