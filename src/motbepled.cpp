#include "motbepled.h"

//----------------------------------------------------------------------
motbepled::motbepled(int8_t t0)
{
  xtipo[0]=t0; xtipo[1]=-1;
}


//----------------------------------------------------------------------
motbepled::motbepled(int8_t t0, int8_t t1)
{
  xtipo[0]=t0; xtipo[1]=t1;
}


//----------------------------------------------------------------------
void motbepled::pinsStep0(int8_t p0, int8_t p1, int8_t p2, int8_t p3, int8_t En1, int8_t En2)
{
  pinosStep[0][0]=p0; pinosStep[0][1]=p1; pinosStep[0][2]=p2; pinosStep[0][3]=p3; pinosStep[0][4]=En1; pinosStep[0][5]=En2;
}


//----------------------------------------------------------------------
void motbepled::pinsStep1(int8_t p0, int8_t p1, int8_t p2, int8_t p3, int8_t En1, int8_t En2)
{
  pinosStep[1][0]=p0; pinosStep[1][1]=p1; pinosStep[1][2]=p2; pinosStep[1][3]=p3; pinosStep[1][4]=En1; pinosStep[1][5]=En2;
}


//----------------------------------------------------------------------
void motbepled::pinsDC0(int8_t p0, int8_t p1, int8_t pwm)
{
  pinosDC[0][0]=p0; pinosDC[0][1]=p1; pinosDC[0][2]=pwm;
}


//----------------------------------------------------------------------
void motbepled::pinsDC1(int8_t p0, int8_t p1, int8_t pwm)
{
  pinosDC[1][0]=p0; pinosDC[1][1]=p1; pinosDC[1][2]=pwm;
}


//----------------------------------------------------------------------
void motbepled::pinsDC2(int8_t p0, int8_t p1, int8_t pwm)
{
  pinosDC[2][0]=p0; pinosDC[2][1]=p1; pinosDC[2][2]=pwm;
}


//----------------------------------------------------------------------
void motbepled::pinsDC3(int8_t p0, int8_t p1, int8_t pwm)
{
  pinosDC[3][0]=p0; pinosDC[3][1]=p1; pinosDC[3][2]=pwm;
}


//----------------------------------------------------------------------
void motbepled::pinBeep(int8_t pb)
{
  pinoBeep=pb;
}


//----------------------------------------------------------------------
void motbepled::pinLed(int8_t pl, int8_t niv)
{
  pinoLed=pl; nivLed=niv;
}


//----------------------------------------------------------------------
void motbepled::begin() {

  for (j=0; j<2; j++){ //inicializa pinos motores Step
    for (k=0; k<6; k++){
      if (pinosStep[j][k]>=0){pinMode(pinosStep[j][k], OUTPUT);digitalWrite(pinosStep[j][k], 0);}
    }
  }
  for (j=0; j<4; j++){ //inicializa pinos motores DC
    for (k=0; k<3; k++){ 
      if (pinosDC[j][k]>=0){pinMode(pinosDC[j][k], OUTPUT);digitalWrite(pinosDC[j][k], 0);}
    }
  }
  if (pinoBeep>=0){
    pinMode(pinoBeep, OUTPUT);                       //beep
    ledcAttachPin(pinoBeep, 4);                      //define pinoLed channel 4 (beep)
    ledcSetup(4, 1000, 8);                           //PWM sempre a 1KHz
    ledcWrite(4, 0);                                 //grava 0 nele (silencia)
    pwmSetup[4] = true;
  }

  if (pinoLed>=0){pinMode(pinoLed, OUTPUT);digitalWrite(pinoLed, !nivLed);}    //Led

  const uint8_t timerNumber = 0;
  hw_timer_t *timer100us = NULL;
  timer100us = timerBegin(timerNumber, 80, true);
  isrTable[timerNumber] = this;
  auto isr = getIsr(timerNumber);
  timerAttachInterrupt(timer100us, isr, false);
  timerAlarmWrite(timer100us, 100, true);
  timerAlarmEnable(timer100us);
}


//----------------------------------------------------------------------
void  motbepled::runStep(uint8_t n, uint32_t steps, uint8_t velstep, boolean cwstep)
{
  xvelstep[n]=600000L/passos[xtipo[n]]/velstep;
  xvelnow[n]=xvelstep[n];
  xcwstep[n]=cwstep;
  if (xcwstep[n]){xfase[n]=-1;}
  if (!xcwstep[n]){xfase[n]=4; if (xtipo[n]==3){xfase[n]=8;}}
  if (pinosStep[n][4]>=0){digitalWrite(pinosStep[n][4],1);} //habilita os enables do motor n
  if (pinosStep[n][5]>=0){digitalWrite(pinosStep[n][5],1);} //habilita os enables do motor n
  xsteps[n]=steps;
}


//----------------------------------------------------------------------
void  motbepled::runDC(uint8_t n, uint32_t time, uint8_t veldc, boolean cwdc)
{
  xveldc[n]=veldc;
  xcwdc[n]=cwdc;

  if (pinosDC[0][2]>=0){
    if (!pwmSetup[0]) { ledcAttachPin(pinosDC[0][2], 0); ledcSetup(0, 1000, 8); pwmSetup[0]=true; }
  } 
  if (pinosDC[1][2]>=0){
    if (!pwmSetup[1]) { ledcAttachPin(pinosDC[1][2], 1); ledcSetup(1, 1000, 8); pwmSetup[1]=true; }
  }  

  if (pinosDC[2][2]>=0){
    if (!pwmSetup[2]) { ledcAttachPin(pinosDC[2][2], 2); ledcSetup(2, 1000, 8); pwmSetup[2]=true; }
  }
  if (pinosDC[3][2]>=0){  
    if (!pwmSetup[3]) { ledcAttachPin(pinosDC[3][2], 3); ledcSetup(3, 1000, 8); pwmSetup[3]=true; }
  }  

  if (pwmSetup[n]) {
    ledcWrite(n, int(float(xveldc[n])/100.0*255.0));
  } else {
    // channel not configured for PWM; ignore write to avoid LEDC not initialized error
    // Serial output is avoided in library code for speed, but could be enabled for debugging
  }
  xtime[n]=time*10;
}


//----------------------------------------------------------------------
uint32_t  motbepled::stepstogo(uint8_t n)
{
  return xsteps[n];
}


//----------------------------------------------------------------------
uint32_t  motbepled::timetogo(uint8_t n)
{
  return xtime[n]/10;
}


//----------------------------------------------------------------------
void  motbepled::beep(int xbnum, int xbdur, int xbfreq, int xbinter)
{
  bdur=xbdur*10; bfreq=xbfreq; binter=xbinter*10; bnum=xbnum; 
}


//----------------------------------------------------------------------
void  motbepled::led(int xlnum, int xldur, int xlinter)
{
  ldur=xldur*10; linter=xlinter*10; lnum=xlnum; 
}


//----------------------------------------------------------------------
void  motbepled::setms(uint32_t yms)
{
  xms=yms*10;
}


//----------------------------------------------------------------------
uint32_t  motbepled::getms()
{
  return xms/10;
}


//----------------------------------------------------------------------
void  motbepled::stopStep(uint8_t n)
{
  xsteps[n]=0;
}


//----------------------------------------------------------------------
void  motbepled::stopDC(uint8_t n)
{
  if (pwmSetup[n]) ledcWrite(n, 0);
  xtime[n]=0;
}


//----------------------------------------------------------------------
void  motbepled::stopBeep()
{
  bnum=0;
}


//----------------------------------------------------------------------
void  motbepled::stopLed()
{
  lnum=0;
}


//----------------------------------------------------------------------
void IRAM_ATTR  motbepled::onTimer100us()
{
  if (xms>0){xms--;}

  //processa os steps---------------------------------------------------------------------------------
  for (k=0; k<2; k++){
    if (xtipo[k]>0){
      if (xsteps[k]!=0){
        xvelnow[k]--;
        if (xvelnow[k]==0){
          xvelnow[k]=xvelstep[k];
          int nf=3;if (xtipo[k]==3){nf=7;}
          if (xcwstep[k]){xfase[k]++;if (xfase[k]>nf){xfase[k]=0;}}else{xfase[k]--;if (xfase[k]<0){xfase[k]=nf;}}
          motbepled::go();
          xsteps[k]--;

          if (xsteps[k]==0){
            if (k==0){
              for (j=0; j<6; j++){
                if (pinosStep[0][j]>=0){digitalWrite(pinosStep[0][j], 0);}
              }
            }       
            if (k==1){
              for (j=0; j<6; j++){
                if (pinosStep[1][j]>=0){digitalWrite(pinosStep[1][j], 0);}
              }
            }       
          }
        }  
      }
    }  
  }
  

  //processa os DCs------------------------------------------------------------------------------------
  for (k=0; k<4; k++){
      if (xtime[k]>0){
      if ( xcwdc[k]){digitalWrite(pinosDC[k][0], 0);digitalWrite(pinosDC[k][1], 1);}
      if (!xcwdc[k]){digitalWrite(pinosDC[k][1], 0);digitalWrite(pinosDC[k][0], 1);}
      xtime[k]--;
      if (xtime[k]==0){
          digitalWrite(pinosDC[k][0], 0);
          digitalWrite(pinosDC[k][1], 0);
          if (pwmSetup[k]) ledcWrite(k, 0);
      }
    }
  }


  //processa o Beep------------------------------------------------------------------------------------
  if (bnum>0){
    if (bxpri){                           //if is the beginning of cycle to beep,
      bxinter=binter+1; bxdur=bdur;       //init the time variables
      bxpausa=false; bxpri=false;         //with default values or user values
      // ensure channel 4 is configured for PWM before writing
      ledcSetup(4, bfreq, 8);
      pwmSetup[4] = true;
    }                                     // 
    if (!bxpausa && (bxdur>0)) {          //if it is beeping 
      if (pwmSetup[4]) ledcWrite(4, 127); bxdur--;          //keep the beep beeping for bxdur ms
      if(bxdur==0){                       //at end,
        if (pwmSetup[4]) ledcWrite(4, 0);                  //stop the beep and advise 
        bxpausa=true;                     //that pause fase is to be initiated
      }
    }
    if (bxpausa && (bxinter>0)){          //if it is in pause
      if (pwmSetup[4]) ledcWrite(4, 0);bxinter--;          //keep the beep stoped for bxinter ms
      if(bxinter==0){                     //at end, exit from pause, subtract 1 from quantity of desired 
        bxpausa=false;bnum--;bxpri=true;  //beeps and advise to reload the variables for a new cycle
      }
    }
  }


  //processa o Led------------------------------------------------------------------------------------
  if (lnum>0){
    if (lxpri){                           //if is the beginning of cycle to blink led,
      lxinter=linter+1; lxdur=ldur;       //init the time variables
      lxpausa=false; lxpri=false;         //with default values or user valuess
    }                                     // 
    if (!lxpausa && (lxdur>0)) {          //if the led is on (out of pause fase)
      digitalWrite(pinoLed, nivLed);lxdur--;//keep the led on for lxdur ms
      if(lxdur==0){                       //at end,
        digitalWrite(pinoLed, !nivLed);   //turn off the led and advise
        lxpausa=true;                     //that pause fase is to be initiated
      }
    }
    if (lxpausa && (lxinter>0)){          //if the led is off (pause fase)
      digitalWrite(pinoLed, !nivLed);lxinter--; //keep the led off for lxinter ms
      if(lxinter==0){                     //at end, exit from pause, subtract 1 from quantity of desired
        lxpausa=false;lnum--;lxpri=true;  //blinks and advise to reload the variables for a new cycle
      }
    }
  }

}

 motbepled * motbepled::isrTable[SOC_TIMER_GROUP_TOTAL_TIMERS];


//----------------------------------------------------------------------
void  motbepled::go()
{
  switch (xtipo[k]) {
    case 1:  motbepled::move1(); break;   //28byj-48, 2048 steps, full step, low torque, low consumption
    case 2:  motbepled::move2(); break;   //28byj-48, 2048 steps, full step, high torque, high consumption
    case 3:  motbepled::move3(); break;   //28byj-48, 4096 steps, half step, high torque, high consumption
    case 4:  motbepled::move4(); break;   //NEMA17,    200 steps, only one mode
  }
}


//----------------------------------------------------------------------
void  motbepled::move1(){   //28byj-48, 2048 steps, full step, low torque, low consumption
  switch (xfase[k]) {
    case 0:  motbepled::writ(0,0,0,1); break;   //0x01
    case 1:  motbepled::writ(0,0,1,0); break;   //0x02
    case 2:  motbepled::writ(0,1,0,0); break;   //0x04
    case 3:  motbepled::writ(1,0,0,0); break;   //0x08
  }
}

void  motbepled::move2(){   //28byj-48, 2048 steps, full step, high torque, high consumption
  switch (xfase[k]) {
    case 0:  motbepled::writ(0,0,1,1); break;   //0x03
    case 1:  motbepled::writ(0,1,1,0); break;   //0x06
    case 2:  motbepled::writ(1,1,0,0); break;   //0x0C
    case 3:  motbepled::writ(1,0,0,1); break;   //0x09    
  }
}

void  motbepled::move3(){   //28byj-48, 4096 steps, half step, high torque, high consumption
  switch (xfase[k]) {
    case 0:  motbepled::writ(0,0,0,1); break;   //0x01
    case 1:  motbepled::writ(0,0,1,1); break;   //0x03
    case 2:  motbepled::writ(0,0,1,0); break;   //0x02
    case 3:  motbepled::writ(0,1,1,0); break;   //0x06
    case 4:  motbepled::writ(0,1,0,0); break;   //0x04
    case 5:  motbepled::writ(1,1,0,0); break;   //0x0C
    case 6:  motbepled::writ(1,0,0,0); break;   //0x08
    case 7:  motbepled::writ(1,0,0,1); break;   //0x09
  } 
}

void  motbepled::move4(){   //NEMA17, 200 steps, only one mode
  switch (xfase[k]) {
    case 0:  motbepled::writ(1,0,1,0); break;   //0x0A
    case 1:  motbepled::writ(0,1,1,0); break;   //0x06
    case 2:  motbepled::writ(0,1,0,1); break;   //0x05
    case 3:  motbepled::writ(1,0,0,1); break;   //0x09    
  }
}

//----------------------------------------------------------------------
void  motbepled::writ(uint8_t px1, uint8_t px2, uint8_t px3, uint8_t px4)
{
 digitalWrite(pinosStep[k][0],px1);digitalWrite(pinosStep[k][1],px2);digitalWrite(pinosStep[k][2],px3);digitalWrite(pinosStep[k][3],px4);
}
//----------------------------------------------------------------------








