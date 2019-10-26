/**
 * @file       main.cpp
 * @author     Volodymyr Shymanskyy
 * @license    This project is released under the MIT License (MIT)
 * @copyright  Copyright (c) 2015 Volodymyr Shymanskyy
 * @date       Mar 2015
 * @brief
 */
#include <wiringPi.h>
#include <vector>
#include <stdio.h>
#include <stdlib.h>
#include <wiringPiSPI.h>
#include <mcp3004.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <iostream>
#include <string>


#include <wiringPiI2C.h>
//for exit loop
#include <stdbool.h>
#include <string.h>
#include <termio.h>
#include <unistd.h>
//for interupt
#include <sys/time.h>
#include <ctime>
#include <iostream>

//#define BLYNK_DEBUG
#define BLYNK_PRINT stdout
#ifdef RASPBERRY
  #include <BlynkApiWiringPi.h>
#else
  #include <BlynkApiLinux.h>
#endif
#include <BlynkSocket.h>
#include <BlynkOptionsParser.h>

//Define buttons
#define CHANGE_INTERVAL_BUTTON 4
#define RESET_TIME_BUTTON 23 
#define DISMISS_ALARM_BUTTON 24 
#define STOP_START_BUTTON 25 

//RTC
const char RTCAddr = 0x6f;


//SPI Settings
#define BASE 100 
#define SPI_CHAN 0// Write your value here
#define SPI_SPEED 200000// Write your value here

//SPI Settings
#define SPI_CHAN1 1// Write your value here
#define SPI_SPEED1 220000// Write your value here


// define constants

const char SEC = 0x00; // see register table in datasheet
const char MIN = 0x01;
const char HOUR = 0x02;
const char TIMEZONE = 2; // +02H00 (RSA)

// define pins
const int SECS = 1;


//Function definitions
void dismiss_alarm(void);
void reset_time(void);
void change_interval(void);
void start_stop(void);
int setup_gpio(void);



// Function definitions
int hFormat(int hours);
int hexCompensation(int units);
int decCompensation(int units);
int getSecs(void);
void loopTime(void);


static BlynkTransportSocket _blynkTransport;
BlynkSocket Blynk(_blynkTransport);

static const char *auth, *serv;
static uint16_t port;

#include <BlynkWidgets.h>


//Global variables
int dhours, dmins, dsecs;
long lastInterruptTime = 0;
long debounceTime = 300000; //Used for button debounce
struct timeval last_change;


using namespace std;

//Global variables
double humidity=0;
double temperature=0;
double hum=0;
double tem=0;
double light=0;
double dacOut=0;
double tempC=0;
int ala = 0;
bool threadReady = false;
bool start= false; 
bool bala = false;

int RTC; //Holds the RTC instance
long dissmissTime;

BlynkTimer tmr;
WidgetLCD lcd(V2);
//string TimeString;
//char TimeString[128];
char TimeString[10];
int frequency = 1;
int up_interval = 1;
int count =0;
int timerID;
time_t start_time;
time_t current_time;
time_t interval_time;
time_t off_time;
#define HOUR 3600
#define MIN 60
int hours = 0;
int minutes = 0;
int seconds = 0;
string str1;

void change_interval(void)
{
    //printf("change interval button press");
     long interrupt_time = millis();
     if ( interrupt_time - lastInterruptTime > 600 )
     {
        frequency=frequency+1;
	if (frequency>3){
	    frequency = 1;
	}
	
     }
    lastInterruptTime=interrupt_time;


}


void reset_time(void)
{
     long interrupt_time = millis();
     if ( interrupt_time - lastInterruptTime > 600 )
     {
	start_time = time(NULL);
     }
    lastInterruptTime=interrupt_time;

}

void dismiss_alarm(void)
{
    long interrupt_time = millis();
    dissmissTime=interrupt_time;
    if ( interrupt_time - lastInterruptTime > 600 )
    {
	bala=false;
	ala = 0;
	printf("Alarm off");
	off_time = time(NULL);
    }
    lastInterruptTime=interrupt_time;

}

void start_stop (void)
{
     long interrupt_time = millis();
     if ( interrupt_time - lastInterruptTime > 600 )
     {
         start = !start;

     }
     lastInterruptTime=interrupt_time;

}


void updatevalue()
{
    Blynk.virtualWrite(V0, (light));
    Blynk.virtualWrite(V3, (hum));
    Blynk.virtualWrite(V4, (tempC));
    Blynk.virtualWrite(V1, ala);
    Blynk.virtualWrite(V5, (dacOut));
    string s=str1;
    char *a = new char[s.size()+1];
    a[s.size()]=0;
    memcpy(a,s.c_str(), s.size());
	
    lcd.print(0, 0, a);
    
    /*
    Blynk.virtualWrite(V0, static_cast<int>(light));
    Blynk.virtualWrite(V3, static_cast<int>(hum));
    Blynk.virtualWrite(V4, static_cast<int>(tempC));
    Blynk.virtualWrite(V1, ala);
    Blynk.virtualWrite(V5, static_cast<int>(dacOut));
    lcd.print(0, 0, TimeString);
    */
}


int setup_gpio(void)
{
    //Set up wiring Pi
    wiringPiSetup();
    RTC=wiringPiI2CSetup(RTCAddr); //Holds the RTC instance

    //setting up the SPI interface
    wiringPiSPISetup(SPI_CHAN, SPI_SPEED);
    mcp3004Setup(BASE,SPI_CHAN);

   //setting up the I2C interface
    RTC = wiringPiI2CSetup(RTCAddr);

    //setting up the buttons
    pinMode(CHANGE_INTERVAL_BUTTON, INPUT);
    pinMode(RESET_TIME_BUTTON, INPUT);
    pinMode(DISMISS_ALARM_BUTTON, INPUT);
    pinMode(STOP_START_BUTTON, INPUT);
   // pinMode(SECS, PWM_OUTPUT);
   // pwmWrite(SECS, 500);

    //Pull up
    pullUpDnControl(CHANGE_INTERVAL_BUTTON, PUD_DOWN);
    pullUpDnControl(RESET_TIME_BUTTON, PUD_DOWN);
    pullUpDnControl(DISMISS_ALARM_BUTTON, PUD_DOWN);
    pullUpDnControl(STOP_START_BUTTON, PUD_DOWN);
    
    
    //Main Thread
    wiringPiISR(CHANGE_INTERVAL_BUTTON,INT_EDGE_RISING,&change_interval);
    wiringPiISR(RESET_TIME_BUTTON,INT_EDGE_FALLING,&reset_time);
    wiringPiISR(DISMISS_ALARM_BUTTON,INT_EDGE_FALLING,&dismiss_alarm);
    wiringPiISR(STOP_START_BUTTON,INT_EDGE_RISING,&start_stop);

	

  
    return 0;
}


/*
 * Thread that handles writing to SPI
 *
 * You must pause writing to SPI if not playing is true (the player is paused)
  * When calling the function to write to SPI, take note of the last argument.
 * You don't need to use the returned value from the wiring pi SPI function
 * You need to use the buffer_location variable to check when you need to switch buffers
 */
void *adcThread(void *threadargs)
{
    // If the thread isn't ready, don't do anything
    while(!threadReady)
    {
        continue;
    }

    //Read adcValu
    humidity=analogRead(BASE+0);
    temperature=analogRead(BASE+1);
    light=analogRead(BASE+2);
    
    pthread_exit(NULL);
    
}





void setup()
{
    Blynk.begin(auth, serv, port);
    start_time = time(NULL);
    interval_time = start_time;
    tm *tm_local = localtime(&start_time);
    cout << "Current local time : " << tm_local->tm_hour << ":" << tm_local->tm_min << ":" << tm_local->tm_sec;
    Blynk.virtualWrite(V3, 0);
    Blynk.virtualWrite(V4, 0);
    Blynk.virtualWrite(V1, 0);
    lcd.print(0, 0, 0);
    timerID = tmr.setInterval(up_interval, updatevalue);
/*
    tmr.setInterval(up_interval, [](){
	Blynk.virtualWrite(V0, BlynkMillis()/1000);
	Blynk.virtualWrite(V3, count);
	Blynk.virtualWrite(V4, 25);
	Blynk.virtualWrite(V1, 100);
	lcd.print(0, 0, TimeString);
    });
*/
}

void reset_systime()
{
    start_time = time(NULL);
}

void cal_time(int a){
    hours = a/HOUR;
    seconds = a % HOUR;
    minutes = seconds/MIN;
    seconds %= MIN;
}



void loop()
{
    
    //Check for a frequency change
    switch (frequency)
    {
	case 1:
	    up_interval = 1;
	    //cout<<"change frequency to 1 \n";
	    break;
	case 2:
	    up_interval = 2;
	    //cout<<"change frequency to 2 \n";
	    break;
	case 3:
	    up_interval = 5;
	    //cout<<"change frequency to 5 \n";
	    break;
    }
    
    
    count = count+1;
    Blynk.run();
 //   tmr.run();


    
    //while(1)
    //{

	 //dacOut=(light*humidity)/(1023);
	 //tempC=(temperature-0.5)/0.01;
	 
	    //loopTime(); //Show current time

       //  long current_time = millis();
	// dissmisTime=interrupt_time;

	
//printf("done");

    humidity=analogRead(BASE+0);
    hum=(humidity*3.3)/1023;
    temperature=analogRead(BASE+1);
    tem=(temperature*3.3)/1023;
    light=analogRead(BASE+2);
    tempC=(tem-0.5)/0.1;
    dacOut=(light*hum)/(1023);
    //printf("digital read: %d", digitalRead(CHANGE_INTERVAL_BUTTON));

 //delay
    if(dacOut < 0.65 || dacOut > 2.65)
    {
	//secPWM(dsecs);
	int atime = difftime(current_time, off_time);
	if (atime>180){
	    bala = true;
	    ala = 1023;
	}
	

	//printf("Alarm ONN \n");

    }

   // }
    
    



    
    current_time = time(NULL);

    //calculate the difference between display time
    int d = difftime(current_time, interval_time);
    //calculate the difference for system up time
    int e = difftime(current_time, start_time);
//    cout<<"current time: "<<current_time<<"\n";
//    cout<<"current time: "<<interval_time<<"\n";
//    cout<<"difference time: "<<d<<"\n";

    //display according to frequency
    if (d >= up_interval){
	tm *tm_local = localtime(&current_time);

	interval_time = time(NULL);
	cal_time(e);
	//TimeString = to_string(hours)+":"+to_string(minutes)+":"+to_string(seconds);
	str1 = "";
	if (hours<10){
	    str1.append("0");
	}
	string temp = to_string(hours);
	str1.append(temp);
	string str2 = ":";
	str1.append(str2);
	if (minutes<10){
	    str1.append("0");
	}
	str1.append(to_string(minutes));
	str1.append(str2);
	if (seconds<10){
	    str1.append("0");
	}
	str1.append(to_string(seconds));

	//printf("%s", str1);

	if (start){
	    cout << "RTC Time : " << tm_local->tm_hour << ":" << tm_local->tm_min << ":" << tm_local->tm_sec<<"\n";
	    printf("System up time: %.2d:%.2d:%.2d\n", hours, minutes, seconds);
	    //cout << "System up time: "<<hours<<":"<<minutes<<":"<<seconds;
	    //cout<<TimeString<<"\n";
	    printf("Temperature: %.1f  \n",tempC);
	    printf("Humidity: %.1f  \n",hum);
	    printf("Light: %.1f  \n",light);
	    printf("Dac out: %.1f \n",dacOut);
	    if (bala){
		printf("Alarm is ON \n");
	    }
	    else{
		printf("Alarm is OFF \n");
	    }
	    
	    updatevalue();
	}
	
    }
/*
//getting current system time
	static int seconds_last = 99;
	timeval curTime;
	gettimeofday(&curTime, NULL);
	if (seconds_last == curTime.tv_sec)
		return;
	
	seconds_last = curTime.tv_sec;
	
	strftime(TimeString, 80, "%H:%M:%S", localtime(&curTime.tv_sec));
	
	
	cout<<TimeString<<"\n";
*/    
    
    

 //   updatevalue();
}


int main(int argc, char* argv[])
{
    parse_options(argc, argv, auth, serv, port);
    setup_gpio();
    setup();

/*
    wiringPiI2CWriteReg8(RTC, HOUR, 0x13+TIMEZONE);
    wiringPiI2CWriteReg8(RTC, MIN, 0x56);
    wiringPiI2CWriteReg8(RTC, SEC, 0x80);
*/
    //Threading
    pthread_attr_t tattr;
    pthread_t thread_id;
    //int newprio = 99;
    //sched_param param;

    pthread_attr_init (&tattr); // initialized with default attributes 
    //pthread_attr_getschedparam (&tattr, &param); // safe to get existing scheduling param 
    //param.sched_priority = newprio; // set the priority; others are unchanged 
    //pthread_attr_setschedparam (&tattr, &param); // setting the new scheduling param 
    pthread_create(&thread_id, NULL, adcThread, NULL); // with new priority specified 
    

    while(true) {
        loop();
    }
    
    //Join and exit the playthread
    pthread_join(thread_id, NULL);
    pthread_exit(NULL);


    return 0;
}

