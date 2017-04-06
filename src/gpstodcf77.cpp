#include "Arduino.h"

#include "Timezone.h"
#include "TinyGPSPlus.h"
#include <TimerOne.h>

/*
 Emulator DCF77

 Uses Time library to facilitate time computation 

 William Welliver 2/22/2017
 
 Fuso68 05/12/2015

 created 4 Sep 2010
 by Michael Margolis
 modified 9 Apr 2012
 by Tom Igoe
 updated for the ESP8266 12 Apr 2015 
 by Ivan Grokhotkov

 This code is in the public domain.
 */

#define DcfPin 2
#define LedPin 13

//how many total pulses we have
#define MaxPulseNumber 60

//0 = no pulse, 1=100msec, 2=200msec
int ArrayImpulses[MaxPulseNumber];

int ContaImpulsi = 0;
int UscitaDcfOn = 0;
int SubSecond = 0;
int Hour,Minute,Seconds,Day,Month,Year, DayOfW;

int notCalced = 0;

int Dls;                    //DayLightSaving

TinyGPSPlus gps;

TimeChangeRule usEDT = {"EDT", Second, Sun, Mar, 2, -240};  //UTC - 4 hours
TimeChangeRule usEST = {"EST", First, Sun, Nov, 2, -300};   //UTC - 5 hours
Timezone usEastern(usEDT, usEST);


void CalculateArray();
void DcfOut();
void LogAndEncodeTime();

int Bin2Bcd(int dato) {
  int msb,lsb;

  if (dato < 10)
    return dato;
  msb = (dato / 10) << 4;
  lsb = dato % 10; 
  return msb + lsb;
}

void setup()
{
    Serial.begin(9600);
    Serial1.begin(9600);
 // while (!Serial) {
 //   ; // wait for serial port to connect. Needed for native USB port only
 // }

  Serial.println();
  Serial.println("DCF77 emulator INIT");

 
  digitalWrite(DcfPin, LOW);
  pinMode(DcfPin, OUTPUT);

  // flash the led a few times
  pinMode(LedPin, OUTPUT);
  for(int i = 0; i < 3; i++) {
    delay(500);
    digitalWrite(LedPin, HIGH);
    delay(500);
    digitalWrite(LedPin, LOW);
  }

  //gestione inpulsi DCF
  Timer1.initialize(100000);
  Timer1.attachInterrupt(DcfOut, 100000);

  //first 2 pulses: 1 + blank to simulate the packet beginning
  //il primo bit e' un 1
  ArrayImpulses[0] = 1;
  //segue l'impulso mancante che indica il sincronismo di ricerca inizio minuto
  ArrayImpulses[1] = 2;

  //last pulse after the third 59° blank
  ArrayImpulses[59] = 0;
  
  ContaImpulsi = 0;
  UscitaDcfOn = 0;    //we begin with the output OFF
}

void loop(){
int isValid = 0;
int age = 0;
while (Serial1.available() > 0)
  gps.encode(Serial1.read());

isValid = gps.time.isValid();
if(isValid) {
  age = gps.location.age();
  if(age > 1000) return; // only work with fresh data.
    if((UscitaDcfOn == 0 && ContaImpulsi == 0) || (ContaImpulsi == 59 && notCalced))
        LogAndEncodeTime();
  }
}

time_t makeTime(int hr,int min,int sec,int dy, int mnth, int yr){
  tmElements_t tm;          
// year can be given as full four digit year or two digts (2010 or 10 for 2010);  
 //it is converted to years since 1970
  if( yr > 99)
      yr = yr - 1970;
  else
      yr += 30;  
  tm.Year = yr;
  tm.Month = mnth;
  tm.Day = dy;
  tm.Hour = hr;
  tm.Minute = min;
  tm.Second = sec;
  return makeTime(tm);
}

void LogAndEncodeTime() {

      notCalced = 0;

      int iYear; int age;
      byte iMonth, iDay, iHour, iMinute, iSecond, CSec;
      time_t currentTime, localTime;  
      int t;
    //calculate actual day to evaluate the summer/winter time of day light saving
    t = millis();
    age = gps.date.age();
    iDay = gps.date.day();
    iMonth = gps.date.month();
    iYear = gps.date.year();
    iHour = gps.time.hour();
    iMinute = gps.time.minute();
    iSecond = gps.time.second();
    CSec = gps.time.centisecond();
    currentTime = makeTime(iHour, iMinute, iSecond, iDay, iMonth, iYear);
    currentTime += 120;
    localTime = usEastern.toLocal(currentTime);

    DayOfW = weekday(localTime);
    Day = day(localTime);
    Month = month(localTime);
    Year = year(localTime);
    Serial.print("Tempo Locale ");       // UTC is the time at Greenwich Meridian (GMT)
    Serial.print(Day);
    Serial.print('/');
    Serial.print(Month);
    Serial.print('/');
    Serial.print(Year);
    Serial.print(' ');
  
    Dls = usEastern.locIsDST(localTime);
    
    Serial.print("DST:");
    Serial.print(Dls);
    Serial.print(' ');
    //add one hour if we are in summer time

    //now that we know the dls state, we can calculate the time too
    // print the hour, minute and second:
    Hour = hour(localTime);
    Minute = minute(localTime);
    Seconds = second(localTime);
    
    Serial.print(Hour); // print the hour
    Serial.print(':');
    Serial.print(Minute); // print the minute
    Serial.print(':');
    Serial.print(Seconds); // print the second
    Serial.print(':');
    Serial.println(CSec); // print the second

    //calculate bits array for the first minute
    CalculateArray();
   
    //how many to the minute end ?
    int DaPerdere = 600 - ((Seconds * 10) + CSec) ;
    DaPerdere = (DaPerdere * 100) - (age + (millis() - t));
    if(DaPerdere > 0) {
      
      Serial.print("sleep ");
      Serial.println(DaPerdere);
      Serial.print("age "  );
      Serial.println(age);
      Serial.print("age 2 " );
      Serial.println(millis() - t);
      delay(DaPerdere);
    }
    //begin
    Serial.print("~");
    UscitaDcfOn = 1;
}

void CalculateArray() {
  int n,Tmp,TmpIn;
  int ParityCount = 0;

  //i primi 20 bits di ogni minuto li mettiamo a valore logico zero
  for (n=0;n<20;n++)
    ArrayImpulses[n] = 1;

  //DayLightSaving bit
  if (Dls == 1)
    ArrayImpulses[17] = 2;
  else
    ArrayImpulses[18] = 2;
    
  //il bit 20 deve essere 1 per indicare tempo attivo
  ArrayImpulses[20] = 2;

  //calcola i bits per il minuto
  TmpIn = Bin2Bcd(Minute);
  for (n=21;n<28;n++) {
    Tmp = TmpIn & 1;
    ArrayImpulses[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  };
  if ((ParityCount & 1) == 0)
    ArrayImpulses[28] = 1;
  else
    ArrayImpulses[28] = 2;

  //calcola i bits per le ore
  ParityCount = 0;
  TmpIn = Bin2Bcd(Hour);
  for (n=29;n<35;n++) {
    Tmp = TmpIn & 1;
    ArrayImpulses[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  if ((ParityCount & 1) == 0)
    ArrayImpulses[35] = 1;
  else
    ArrayImpulses[35] = 2;
   ParityCount = 0;
  //calcola i bits per il giorno
  TmpIn = Bin2Bcd(Day);
  for (n=36;n<42;n++) {
    Tmp = TmpIn & 1;
    ArrayImpulses[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  //calcola i bits per il giorno della settimana
  TmpIn = Bin2Bcd(DayOfW);
  for (n=42;n<45;n++) {
    Tmp = TmpIn & 1;
    ArrayImpulses[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  //calcola i bits per il mese
  TmpIn = Bin2Bcd(Month);
  for (n=45;n<50;n++) {
    Tmp = TmpIn & 1;
    ArrayImpulses[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  //calcola i bits per l'anno
  TmpIn = Bin2Bcd(Year - 2000);   //a noi interesa solo l'anno con ... il millenniumbug !
  for (n=50;n<58;n++) {
    Tmp = TmpIn & 1;
    ArrayImpulses[n] = Tmp + 1;
    ParityCount += Tmp;
    TmpIn >>= 1;
  }
  //parita' di data
  if ((ParityCount & 1) == 0)
    ArrayImpulses[58] = 1;
  else
    ArrayImpulses[58] = 2;

  //ultimo impulso mancante
  ArrayImpulses[59] = 0;
}

void DcfOut() {
  if (UscitaDcfOn == 1) {
    switch (SubSecond++) {
      case 0:
        if(ContaImpulsi == 0 && notCalced == 0) notCalced = 1;
      Serial.println(ContaImpulsi);
if (ArrayImpulses[ContaImpulsi] != 0) {
  digitalWrite(LedPin, HIGH);
  pinMode (DcfPin, OUTPUT) ; // drive pin low
        }
        break;
      case 1:
        if (ArrayImpulses[ContaImpulsi] == 1) {
  digitalWrite(LedPin, LOW);
  pinMode (DcfPin, INPUT) ; // hi-Z state
        }
        break;
      case 2:
  digitalWrite(LedPin, LOW);
  pinMode (DcfPin, INPUT) ; // hi-Z state
        break;
      case 9:
        if (ContaImpulsi++ == (MaxPulseNumber -1 )){     //one less because we FIRST tx the pulse THEN count it
          ContaImpulsi = 0;
          UscitaDcfOn = 0;
        };
        SubSecond = 0;
        break;
    }
  }
}


