#ifndef motbepled_H
#define motbepled_H

#include "Arduino.h"

class motbepled {

  public:

  motbepled (int8_t t0);
  motbepled (int8_t t0, int8_t t1);
  void pinsStep0(int8_t p0, int8_t p1, int8_t p2, int8_t p3, int8_t En1, int8_t En2);
  void pinsStep1(int8_t p0, int8_t p1, int8_t p2, int8_t p3, int8_t En1, int8_t En2);
  void pinsDC0(int8_t p0, int8_t p1, int8_t pwm);
  void pinsDC1(int8_t p0, int8_t p1, int8_t pwm);
  void pinsDC2(int8_t p0, int8_t p1, int8_t pwm);
  void pinsDC3(int8_t p0, int8_t p1, int8_t pwm);
  void pinBeep(int8_t pb);
  void pinLed(int8_t pl, int8_t niv);
  void begin();
  void runStep(uint8_t n, uint32_t steps, uint8_t velstep, boolean cwstep);
  void runDC(uint8_t n, uint32_t time, uint8_t veldc, boolean cwdc);
  void beep(int xbnum, int xbdur, int xbfreq, int xbinter);
  void led(int xlnum, int xldur, int xlinter);
  void setms(uint32_t yms);
  void stopStep(uint8_t n);
  void stopDC(uint8_t n);
  void stopBeep();
  void stopLed();

  uint32_t getms();
  uint32_t stepstogo(uint8_t n);
  uint32_t timetogo(uint8_t n);

  volatile int bdur=0, binter=0, bfreq=0, bnum=0;
  volatile int ldur=0, linter=0, lnum=0;
  volatile uint32_t xms=0;

  volatile int8_t      xtipo[2]={-1,-1};
  volatile uint32_t   xsteps[2]={0,0};
  volatile uint32_t xvelstep[2]={0,0};
  volatile boolean   xcwstep[2]={0,0};
  volatile int8_t      xfase[2]={0,0};
  volatile uint32_t  xvelnow[2]={0,0};

  volatile uint32_t    xtime[4]={0,0,0,0};
  volatile uint8_t     xveldc[4]={0,0,0,0};
  volatile boolean     xcwdc[4]={1,1,1,1};



  private:

  void onTimer100us();
  void go();
  void move1();
  void move2();
  void move3();
  void move4();
  void writ(uint8_t px1, uint8_t px2, uint8_t px3, uint8_t px4);
  static motbepled *isrTable[];
  using isrFunct = void (*)();
  template<uint8_t NUM_INTERRUPTS = SOC_TIMER_GROUP_TOTAL_TIMERS>
  static isrFunct getIsr(uint8_t timerNumber);

  //variaveis de controle do beep
  volatile bool bxpausa=false, bxpri=true;
  volatile int bxinter=0, bxdur=0;
  int8_t pinoBeep=-1;

  //variaveis de controle do led
  volatile bool lxpausa=false, lxpri=true;
  volatile int lxinter=0, lxdur=0;
  int8_t pinoLed=-1, nivLed=0;

  //variaveis auxiliares
  uint8_t i, j, k;

  //variaveis de controle dos step motors
  uint16_t passos[5]={ 0, 2048, 2048, 4096, 200};

  //pinos associados aos motores Step (4 fios mais 2 enables)
  int8_t pinosStep[2][6]={ {-1,-1,-1,-1,-1,-1}, {-1,-1,-1,-1,-1,-1} };

  //pinos associados aos motores DC
  int8_t pinosDC[4][3]={ {-1,-1,-1}, {-1,-1,-1}, {-1,-1,-1}, {-1,-1,-1} };
};


template<uint8_t NUM_INTERRUPTS>
motbepled::isrFunct motbepled::getIsr(uint8_t timerNumber) {
  if (timerNumber == (NUM_INTERRUPTS - 1)) {
    return [] {
      isrTable[NUM_INTERRUPTS - 1]->onTimer100us();
    };
  }
  return getIsr < NUM_INTERRUPTS - 1 > (timerNumber);
}

template<>
inline motbepled::isrFunct motbepled::getIsr<0>(uint8_t timerNumber) {
  (void) timerNumber;
  return nullptr;
}

#endif

