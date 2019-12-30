#include <ClickEncoder.h>
#include <TimerOne.h>
#include <SSD1306AsciiAvrI2c.h>
#include <EasyButton.h>
#include <SD.h>

#define SAVE_FILE                "fst.cfg"
#define LOG_FILE                 "fst.log"

#define I2C_ADDRESS              0x3C // 0X3C+SA0 - 0x3C or 0x3D

#define PIN_TONE                 A3
#define PIN_ENLA_RELAY           4
#define PIN_ENC_A                A1
#define PIN_ENC_B                A0
#define PIN_ENC_BTN              A2
#define PIN_L_BTN                7
#define PIN_R_BTN                8

#define MOD_PAUSE                0
#define MOD_MENUS                1
#define MOD_FSTOP                2
#define MOD_TIME                 3
#define MOD_STRIPES              4

#define I_LEFT                   0
#define I_CENTER_LEFT            32
#define I_CENTER                 64
#define I_CENTER_RIGHT           96
#define I_RIGHT                  127

#define LP_UNDF                 -123

#define SCR_MAIN                 0
#define SCR_FSTOP                1
#define SCR_TIME                 2
#define SCR_SETUP                3
#define SCR_STRIPE               4
//#define SCR_DEV_TIMER            5
const int8_t CNT_MAIN[]      = { 1, 4 };

#define SCR_DODGE                10
#define SCR_BURN                 11
#define OPT_FSTOP_F              12
const int8_t CNT_FSTOP[]     = { 10, 12 };

const int8_t CNT_DODGE[]     = { 100, 102 };
const int8_t CNT_BURN[]      = { 110, 112 };

#define OPT_STRIPE_N             30
#define OPT_STRIPE_F             31
const int8_t CNT_STRIPE[]    = { 30, 31 };

#define OPT_SETUP_CONTRAST       40
#define OPT_SETUP_TRIGGER        41
#define OPT_SETUP_SOUND          42
const int8_t CNT_SETUP[]     = { 40, 42 };

/*
#define OPT_DT_DEV               50
#define OPT_DT_STOP              51
#define OPT_DT_FIX               52
#define OPT_DT_WASH              53
const int8_t CNT_DT_TIMES[]  = { 50, 53 };
*/

const char M_MAIN[]       = "Main";
const char M_FSTOP[]      = "fStop";
const char M_DODGE[]      = "Dodge";
const char M_BURN[]       = "Burn";
const char M_TIME[]       = "Time";
const char M_STRIPE[]     = "Stripe";
const char M_SETUP[]      = "Setup";
//const char M_DEV_TIMER[]  = "Dev Timer";

const char S_LIGHT_ON[]   = "LIGHT ON";
const char S_CLEAR[]      = "        ";
const char S_SD_ERR[]     = "SDerr";
const char S_PAUSE[]      = " -PAUSE- ";
const char S_READY[]      = ".o.";
const char S_SEP          = ';';

const char F_D[]          = "%d";
const char F_FS[]         = "f/%s";
const char F_DFS[]        = "df=%s";
const char F_TS[]         = "t=%s";
const char F_DTS[]        = "dt=%s";
const char F_SS[]         = "%ss";
const char F_ND[]         = "n=%d";
const char F_DMS[]        = "%dms";
const char F_DODGED[]     = "Dodge=%d";
const char F_BURND[]      = "Burn=%d";
const char F_DODGES[]     = "Dodge=%s";
const char F_BURNS[]      = "Burn=%s";
/*
const char F_DT_DEV[]     = "Dev=%d";
const char F_DT_STOP[]    = "Stop=%d";
const char F_DT_FIX[]     = "Fix=%d";
const char F_DT_WASH[]    = "Wash=%d";
*/
SSD1306AsciiAvrI2c oled;
ClickEncoder *encoder;

File fstFile;
bool isSD = false;

EasyButton lBtn(PIN_L_BTN);
EasyButton rBtn(PIN_R_BTN);

struct fStop {
  float f;
  float tf;
};

fStop dodge[4];
fStop burn[4];
int8_t dodges = 0;
int8_t burns  = 0;
int8_t nDodge = 0;
int8_t nBurn = 0;

int8_t activeMode = MOD_MENUS;
int8_t lastMode;

int8_t encoderValue = 0;         // encoder value
int8_t lastPosition = LP_UNDF;   // last pos
int8_t menuPosition = SCR_FSTOP; // menu pos

int8_t screen = SCR_MAIN;        // scree no
uint16_t printCount = 0;         // flash counter

uint32_t timeStamp;

float t  = 4;                    // time [s]
float dt = 1;                    // encoder delta t
float f  = 2.0;                  // fstop number
float df = 0.5;                  // encoder delta fstop
float ct;                        // counter t
float cf;                        // counter f
float lt;                        // time left
float lf;                        // f left

float cft;

uint8_t sn = 5;                  // number of stripes
uint8_t csn;                     // counter nuber of stripes

uint8_t cont = 128;              // display contrast
uint16_t trg = 500;              // trigger delay
uint8_t snd = 2;                 // 0 - no sound, 1 - ui, 2 - ui+midtimes 3 - ui+midtimes+seconds

bool isLightOn = false;
bool inCounter = false;
bool breakCounter = false;

bool wait4Click = false;

char buf[20];
/*
uint16_t devTimer[4];
*/

void setup() {
  //Serial.begin(9600);

  isSD = SD.begin();

  if (isSD) {
    fstFile = SD.open(SAVE_FILE);
    if (fstFile) {
      printCount = fstFile.parseInt();
      cont = fstFile.parseInt();
      snd = fstFile.parseInt();
      sn = fstFile.parseInt();
      fstFile.close();
      printCount++;
    }
  }

  //for 0.96" oled
  //oled.begin(&Adafruit128x64, I2C_ADDRESS);
  
  //for 1.3" oled
  oled.begin(&SH1106_128x64, I2C_ADDRESS);
  oled.setFont(System5x7);
  oled.setContrast(cont);

  encoder = new ClickEncoder(PIN_ENC_A, PIN_ENC_B, PIN_ENC_BTN, 4);
  Timer1.initialize(30);
  Timer1.attachInterrupt(timerIsr); 
  encoder->setAccelerationEnabled(false);   

  pinMode(PIN_L_BTN, INPUT_PULLUP);
  lBtn.begin();
  lBtn.onPressed(lBtnPressShr);
  lBtn.onPressedFor(trg, lBtnPressLng);

  pinMode(PIN_R_BTN, INPUT_PULLUP);
  rBtn.begin();
  rBtn.onPressed(rBtnPressShr);
  rBtn.onPressedFor(trg, rBtnPressLng);

  pinMode(PIN_ENLA_RELAY, OUTPUT);
  digitalWrite(PIN_ENLA_RELAY, HIGH);

  for (int i = 0; i <= 3; i++) {
    dodge[i] = {0, 0};
    burn[i]  = {0, 0};
  }
}

void timerIsr() {
  encoder->service();
}

void beep(uint16_t hz, uint16_t b = 0, uint16_t a = 10) {
  if (snd >= 1) {
    delay(b);
    tone(PIN_TONE, hz);
    delay(a);
    noTone(PIN_TONE);
  }
}

void beepProg() {
  if (snd >= 3) {
    beep(3000, 50, 100);
    beep(3000, 50, 100);
  }
}

void beepAcl() {
  if (snd >= 2) {
    beep(3000, 50, 100);
    beep(1000, 50, 100);
    beep(2000, 50, 100);
  }
}

void prnItem(bool i, bool d, int8_t x, int8_t y, const char * s) {
  if (d) {
    oled.set2X();
  } else {
    oled.set1X();
  }
  int8_t l = strlen(s);
  int8_t cx = x;
  int8_t sc = d ? 12 : 6;
  switch (x) {
    case I_LEFT:
      cx = 0;
      break;
    case I_RIGHT:
      cx = x - l * sc;
      break;  
    default:
      cx = x - l * (sc / 2);
  }
  if (cx > sc) {
    oled.setInvertMode(false);
    oled.setCursor(cx - sc, y);
    oled.print(" ");
  }
  oled.setInvertMode(i);
  oled.setCursor(cx, y);
  oled.print(s);
  oled.setInvertMode(false);
  oled.print(" ");
}

void menuItem(int8_t pos, bool d, int8_t x, int8_t y, const char * s) {
  prnItem(menuPosition == pos, d, x, y, s);
}

void menuItem2(int8_t pos, bool d, int8_t x, int8_t y) {
  prnItem(menuPosition == pos, d, x, y, buf);
}

char * fts(const char * fmt, uint8_t w, uint8_t p, float f){
  char bfr[5];
  dtostrf(f, w, p, bfr);
  sprintf(buf, fmt, bfr);
  return buf;
}

char * lts(const char * fmt, int16_t l){
  sprintf(buf, fmt, l);
  return buf;
}

void mpRoll(const int8_t m[]) {
  menuPosition = menuPosition > m[1] ? m[0] : menuPosition < m[0] ? m[1] : menuPosition;
  lastPosition = menuPosition;
}

void swtScr(int8_t s, int8_t m, bool c) {
  if (isLightOn) {
    turnLight(false);
  }
  if (c) {
    oled.clear();
  }
  lastPosition = LP_UNDF;
  menuPosition = m;
  screen = s;
}

void swtScrIf(int8_t s, int8_t m, bool c) {
  if (menuPosition == s) {
    swtScr(s, m, c);
  }
}

void sHead(const char * s) {
  prnItem(false, false, I_LEFT,  0, s);
  prnItem(false, false, I_RIGHT, 0, lts(F_D, printCount));
  //prnItem(false, false, I_LEFT,  1, lts(F_D, screen));
  //prnItem(false, false, I_RIGHT, 1, lts(F_D, menuPosition));
  if (!isSD) {
    prnItem(false, false, I_LEFT, 1, S_SD_ERR);
  }
}

void turnLight(bool _on) {
  isLightOn = _on;
  if (_on) {
    digitalWrite(PIN_ENLA_RELAY, LOW);
    prnItem(true, false, I_CENTER, 0, S_LIGHT_ON);
  } else {
    digitalWrite(PIN_ENLA_RELAY, HIGH);
    prnItem(false, false, I_CENTER, 0, S_CLEAR);
  }
}

void svi(uint8_t i) {
  fstFile.print(i);
  fstFile.print(S_SEP);
}

void svf(float f) {
  fstFile.print(f);
  fstFile.print(S_SEP);
}

void saveCfg() {
  if (isSD) {
    SD.remove(SAVE_FILE);
    fstFile = SD.open(SAVE_FILE, FILE_WRITE);
    if (fstFile) {
      svi(printCount);
      svi(cont);
      svi(snd);
      svi(sn);
      fstFile.close();
      fstFile.flush();
    }
  }
}

void saveLog() {
  saveCfg();
  if (isSD) {
    fstFile = SD.open(LOG_FILE, FILE_WRITE);
    if (fstFile) {
      svi(printCount);
      svi(screen);
      svf(screen == SCR_STRIPE ? cf : f);
      svf(screen == SCR_STRIPE ? ct : t);
      fstFile.close();
      fstFile.flush();
    }
  } 
}

int sort(fStop a[]) {
  int i;
  bool flip;
  int ret;
  do {
    flip = false;
    ret = 0;
    for (i = 0; i <= 2; i++) {
      if (a[i].f < a[i + 1].f) {
        float mf = a[i].f;
        a[i].f = a[i + 1].f;
        a[i + 1].f = mf;
        flip = true;
      }
      if (a[i].f > 0) {
        ret++;
      }
    }
  } while (flip);
  return ret;
}

void calcDodges() {
  dodges = sort(dodge);
  fStop fs;
  dodge[3].tf = f;
  for (int i = 2; i >= 0; i--) {
    dodge[i].tf = dodge[i + 1].tf - dodge[i].f;
  }
  if (dodge[0].tf < -3.0) {
    prnItem(false, false, I_CENTER, 1, "too short");
  } else {
    prnItem(false, false, I_CENTER, 1, "         ");
  }
}

void calcBurns() {
  burns = sort(burn);
}

void lClick() {
  if (screen == OPT_FSTOP_F) {
    swtScr(SCR_FSTOP, OPT_FSTOP_F, false);
    return;
  }
  if (screen >= 10 && screen <= 11) {
    swtScr(SCR_FSTOP, screen, true);
    return;
  }
  if (screen >= 100 && screen <= 102) {
    calcDodges();
    swtScr(SCR_DODGE, screen, false);
    return;
  }
  if (screen >= 110 && screen <= 112) {
    calcBurns();
    swtScr(SCR_BURN, screen, false);
    return;
  }
  if (screen == OPT_STRIPE_N || screen == OPT_STRIPE_F) {
    swtScr(SCR_STRIPE, screen, false);
    return;
  }
  // Click in Setup screen on set Contrast or Sount
  if (screen == OPT_SETUP_CONTRAST || screen == OPT_SETUP_SOUND) {
    swtScr(SCR_SETUP, screen, false);
    return;
  }        
  // Click in Setup screen on set Trigger
  if (screen == OPT_SETUP_TRIGGER) {
    lBtn.onPressedFor(trg, lBtnPressLng);
    rBtn.onPressedFor(trg, rBtnPressLng);
    swtScr(SCR_SETUP, OPT_SETUP_TRIGGER, false);
    return;
  }      
  if (screen >= CNT_MAIN[0] && screen <= CNT_MAIN[1]) {
    if (screen == SCR_SETUP) {
      saveCfg();
    }
    swtScr(SCR_MAIN, screen, true);
  }
}

void rClick() {
  // Click in Main screen
  if (screen == SCR_MAIN) {
    // on fStop
    swtScrIf(SCR_FSTOP, OPT_FSTOP_F, true);
    // on Time
    swtScrIf(SCR_TIME, 0, true);
    // on Stripe
    swtScrIf(SCR_STRIPE, OPT_STRIPE_F, true);
    // on Setup
    swtScrIf(SCR_SETUP, OPT_SETUP_CONTRAST, true);
    // on Dev timer
    //swtScrIf(SCR_DEV_TIMER, OPT_DT_DEV, true);
    return;
  }
  // Click in fStop screen
  if (screen == SCR_FSTOP) {
    // on Dodge
    swtScrIf(SCR_DODGE, 100, true);
    // on Burn
    swtScrIf(SCR_BURN, 110, true);
    // on fStop
    swtScrIf(OPT_FSTOP_F, 0, false);
    return;
  }
  // Click in Dodge screen
  if (screen == SCR_DODGE) {
    swtScrIf(100, 0, false);
    swtScrIf(101, 0, false);
    swtScrIf(102, 0, false);
    swtScrIf(103, 0, false);
    swtScrIf(104, 0, false);
    return;
  }
  // Click in Burn screen
  if (screen == SCR_BURN) {
    swtScrIf(110, 0, false);
    swtScrIf(111, 0, false);
    swtScrIf(112, 0, false);
    swtScrIf(113, 0, false);
    swtScrIf(114, 0, false);
    return;
  }
  // Click in Time screen
  if (screen == SCR_TIME) {
    dt = dt == 0.1 ? 1.0 : dt == 1.0 ? 10.0 : dt == 10.0 ? 0.1 : 1.0; 
    lastPosition = LP_UNDF;
    return;      
  }
  // Click in Stripe screen
  if (screen == SCR_STRIPE) {
    // on edit f setting
    swtScrIf(OPT_STRIPE_F, 0, false);
    // on edit n setting
    swtScrIf(OPT_STRIPE_N, 0, false);
    return;
  }
  // Click in F stop screem Stripe screen, dodges and burns on set f
  if (screen == OPT_STRIPE_F || screen == OPT_FSTOP_F || (screen >= 100 && screen <= 112)) {
    df = df == 0.1 ? 0.3 : df == 0.3 ? 0.5 : df == 0.5 ? 1.0 : df == 1.0 ? 0.1 : 0.3; 
    lastPosition = LP_UNDF;
    return;
  }        
  // Click in Stripe screen on set n
  if (screen == OPT_STRIPE_N) {
    swtScr(SCR_STRIPE, OPT_STRIPE_N, false);
    return;
  }        
  // Click in Setup screen
  if (screen == SCR_SETUP) {
    // on edit Contrast setting
    swtScrIf(OPT_SETUP_CONTRAST, 0, false);
    // on edit Trigger setting
    swtScrIf(OPT_SETUP_TRIGGER, 0, false);
    // on edit Sound setting
    swtScrIf(OPT_SETUP_SOUND, 0, false);
    return;
  }
/*    
  // Click in Setup screen
  if (screen == SCR_DEV_TIMER) {
    // on edit Dev setting
    swtScrIf(OPT_DT_DEV, 0, false);
    // on edit Dev setting
    swtScrIf(OPT_DT_STOP, 0, false);
    // on edit Dev setting
    swtScrIf(OPT_DT_FIX, 0, false);
    // on edit Dev setting
    swtScrIf(OPT_DT_WASH, 0, false);
    return;
  }
  // Click in DevTimer on set times
  if (screen == OPT_DT_DEV || screen == OPT_DT_STOP || screen == OPT_DT_FIX || screen == OPT_DT_WASH) {
    swtScr(SCR_DEV_TIMER, screen, false);
    return;
  }  
*/  
} 

float rollFloatVar(float varF, float step, float minF, float maxF) {
  lastPosition = menuPosition;
  float deltaF = (float) encoderValue * step;
  if (varF + deltaF >= minF - 1 && varF + deltaF <= maxF) {
    varF += deltaF;        
  }
  return varF < minF ? minF : varF;
}

float rollIntVar(int16_t varI, uint8_t step, int16_t minI, int16_t maxI) {
  lastPosition = menuPosition;
  int16_t deltaI = encoderValue * step;
  if (varI + deltaI >= minI && varI + deltaI <= maxI) {
    varI += deltaI;        
  }
  return varI;
}

void fToT() {
  f = rollFloatVar(f, df, -3.0, 10.0);
  t = pow(2, f);
  cf = f + ((sn - 1) * df);
  ct = pow(2, cf);
}

void encoderTurn() {
	encoderValue = encoder->getValue();
  menuPosition += encoderValue;
  if (menuPosition != lastPosition) {
    beep(50);
    // main menu screen
    if (screen == SCR_MAIN) {
      mpRoll(CNT_MAIN);
      sHead(M_MAIN);
      menuItem(SCR_FSTOP,     true,  I_CENTER_LEFT,  4, M_FSTOP);
      menuItem(SCR_TIME,      true,  I_CENTER_RIGHT, 4, M_TIME);
      menuItem(SCR_STRIPE,    false, I_CENTER,       2, M_STRIPE);
      menuItem(SCR_SETUP,     false, I_CENTER,       7, M_SETUP);
      //menuItem(SCR_DEV_TIMER, false, I_CENTER,       2, M_DEV_TIMER);
    }
    // fStop screen
    if (screen == SCR_FSTOP) {
      mpRoll(CNT_FSTOP);
      sHead(M_FSTOP);
      menuItem(SCR_DODGE,   false, I_CENTER_LEFT,  2, lts(F_DODGED, dodges));
      menuItem(SCR_BURN,    false, I_CENTER_RIGHT, 2, lts(F_BURND,  burns));
      menuItem(OPT_FSTOP_F, true,  I_CENTER,       4, fts(F_FS,  2, 1, f));
      prnItem(false,        false, I_CENTER_LEFT,  7, fts(F_DFS, 3, 2, df));
      prnItem(false,        false, I_CENTER_RIGHT, 7, fts(F_TS,  3, 1, t));
    }
    if (screen == OPT_FSTOP_F) {
      fToT();
      sHead(M_FSTOP);
      calcDodges();
      calcBurns();
      prnItem(false, true,  I_CENTER, 4, fts(F_FS, 2, 1, f));
      prnItem(false, false, I_CENTER_LEFT,  7, fts(F_DFS, 3, 2, df));
      prnItem(false, false, I_CENTER_RIGHT, 7, fts(F_TS,  3, 1, t));
    }
    if (screen == SCR_DODGE) {
      mpRoll(CNT_DODGE);
      sHead("Dodge");
      menuItem(100, false,  I_CENTER, 3, fts(F_DODGES, 3, 2, dodge[0].f));
      menuItem(101, false,  I_CENTER, 4, fts(F_DODGES, 3, 2, dodge[1].f));
      menuItem(102, false,  I_CENTER, 5, fts(F_DODGES, 3, 2, dodge[2].f));
      prnItem(false, false, I_CENTER, 7, fts(F_DFS, 3, 2, df));
    }
    if (screen >= 100 && screen <= 102) {
      uint8_t pos = screen - 100; 
      dodge[pos].f = rollFloatVar(dodge[pos].f, df, 0.0, 10.0);
      prnItem(false, false, I_CENTER, pos + 3, fts(F_DODGES, 3, 2, dodge[pos].f));
      prnItem(false, false, I_CENTER, 7, fts(F_DFS, 3, 2, df));
    }
    if (screen == SCR_BURN) {
      mpRoll(CNT_BURN);
      sHead("Burn");
      menuItem(110,  false, I_CENTER, 3, fts(F_BURNS, 3, 2, burn[0].f));
      menuItem(111,  false, I_CENTER, 4, fts(F_BURNS, 3, 2, burn[1].f));
      menuItem(112,  false, I_CENTER, 5, fts(F_BURNS, 3, 2, burn[2].f));
      prnItem(false, false, I_CENTER, 7, fts(F_DFS, 3, 2, df));
    }
    if (screen >= 110 && screen <= 112) {
      uint8_t pos = screen - 110; 
      burn[pos].f = rollFloatVar(burn[pos].f, df, 0.0, 10.0);
      prnItem(false, false, I_CENTER, pos + 3, fts(F_BURNS, 3, 2, burn[pos].f));
      prnItem(false, false, I_CENTER, 7, fts(F_DFS, 3, 2, df));
    }
    // Time screen
    if (screen == SCR_TIME) {
      t = rollFloatVar(t, dt, 0.1, 1024.0);
      sHead(M_TIME);
      prnItem(false, true,  I_CENTER,       4, fts(F_SS, 3, 1, t));
      prnItem(false, false, I_CENTER_LEFT,  7, fts(F_DTS, 3, 1, (float) dt));      
      lf = log10(t) / log10(2);
      prnItem(false, false, I_CENTER_RIGHT, 7, fts(F_FS, 3, 1, lf >= -3 ? lf : -3));
    }
    // Stripe
    if (screen == SCR_STRIPE) {
      mpRoll(CNT_STRIPE);
      sHead(M_STRIPE);
      menuItem(OPT_STRIPE_N, false, I_CENTER,       2, lts(F_ND, sn));
      menuItem(OPT_STRIPE_F, true,  I_CENTER,       4, fts(F_FS,  2, 1, f));
      prnItem(false,         false, I_CENTER_LEFT,  7, fts(F_DFS, 3, 2, df));
      cf = f + ((sn - 1) * df);
      prnItem(false,         false, I_CENTER_RIGHT, 7, fts(F_FS,  3, 2, cf));
    }
    // Stripe screen - f setting option
    if (screen == OPT_STRIPE_F) {
      fToT();
      sHead(M_STRIPE);
      prnItem(false, false, I_CENTER,       2, lts(F_ND, sn));
      prnItem(false, true,  I_CENTER,       4, fts(F_FS,  2, 1, f));
      prnItem(false, false, I_CENTER_LEFT,  7, fts(F_DFS, 3, 2, df));
      prnItem(false, false, I_CENTER_RIGHT, 7, fts(F_FS,  3, 2, cf));
    }
    // Stripe screen - df setting option
    if (screen == OPT_STRIPE_N) {
      sn = rollIntVar(sn, 1, 2, 20);
      cf = f + ((sn - 1) * df);
      prnItem(false, false, I_CENTER,       2, lts(F_ND, sn));
      prnItem(false, false, I_CENTER_RIGHT, 7, fts(F_FS, 3, 2, cf));
    }
    // Setup screen
    if (screen == SCR_SETUP) {
      mpRoll(CNT_SETUP);
      sHead(M_SETUP);
      prnItem(false,               false, I_LEFT,  2, "Contrast");
      menuItem(OPT_SETUP_CONTRAST, false, I_LEFT,  3, lts(F_D, cont));
      prnItem(false,               false, I_LEFT,  4, "Trigger");
      menuItem(OPT_SETUP_TRIGGER,  false, I_LEFT,  5, lts(F_DMS, trg));
      prnItem(false,               false, I_LEFT,  6, "Sound");
      menuItem(OPT_SETUP_SOUND,    false, I_LEFT,  7, lts(F_D, snd));
    }
    // Setup screen - contrast setting option
    if (screen == OPT_SETUP_CONTRAST) {
      cont = rollIntVar(cont, 1, 1, 255);
      prnItem(false, false, I_LEFT, 3, lts(F_D, cont));
      oled.setContrast(cont);
    }
    // Setup screen - trigger delay setting option
    if (screen == OPT_SETUP_TRIGGER) {
      trg = rollIntVar(trg, 100, 200, 3000);
      prnItem(false, false, I_LEFT, 5, lts(F_DMS, trg));
    }
    // Setup screen - sound setting option
    if (screen == OPT_SETUP_SOUND) {
      snd = rollIntVar(snd, 1, 0, 4);
      prnItem(false, false, I_LEFT, 7, lts(F_D, snd));
    }
/*    
    // Development timer screen
    if (screen == SCR_DEV_TIMER) {
      mpRoll(CNT_DT_TIMES);
      sHead(M_DEV_TIMER);
      menuItem(OPT_DT_DEV,  false, I_CENTER_LEFT,  2, lts(F_DT_DEV,  devTimer[0]));
      menuItem(OPT_DT_STOP, false, I_CENTER_RIGHT, 2, lts(F_DT_STOP, devTimer[1]));
      menuItem(OPT_DT_FIX,  false, I_CENTER_LEFT,  7, lts(F_DT_FIX,  devTimer[2]));
      menuItem(OPT_DT_WASH, false, I_CENTER_RIGHT, 7, lts(F_DT_WASH, devTimer[3]));
    }
    // Click in Dev time
    if (screen == OPT_DT_DEV) {
      devTimer[0] = rollIntVar(devTimer[0], 10, 1, 999);
      prnItem(false,  false, I_CENTER_LEFT,  2, lts(F_DT_DEV,  devTimer[0]));
    }
    if (screen == OPT_DT_STOP) {
      devTimer[1] = rollIntVar(devTimer[1], 10, 1, 999);
      prnItem(false,  false, I_CENTER_RIGHT, 2, lts(F_DT_STOP, devTimer[1]));
    }
    if (screen == OPT_DT_FIX) {
      devTimer[2] = rollIntVar(devTimer[2], 10, 1, 999);
      prnItem(false,  false, I_CENTER_LEFT,  7, lts(F_DT_FIX,  devTimer[2]));
    }
    if (screen == OPT_DT_WASH) {
      devTimer[3] = rollIntVar(devTimer[3], 10, 1, 999);
      prnItem(false,  false, I_CENTER_RIGHT, 7, lts(F_DT_WASH, devTimer[3]));
    }
*/
  }
}

void encoderClick() {
  ClickEncoder::Button b = encoder->getButton();
  if (b != ClickEncoder::Open) {
    switch (b) {
      case ClickEncoder::Pressed:
        break;
      case ClickEncoder::Held:
        break;
      case ClickEncoder::Released:
        rBtnPressLng();
        break;
      case ClickEncoder::Clicked:
        lBtnPressShr();
        break;
      case ClickEncoder::DoubleClicked:
        rBtnPressShr();
        break;
    }
  }
}

void lBtnPressShr() {
  beep(500);
  if (activeMode == MOD_MENUS) {  
    lClick();
    return;
  }
}

void lBtnPressLng() {
  beep(500);
  if (activeMode == MOD_MENUS) {  
    turnLight(!isLightOn);
    return;
  }
}

void rBtnPressShr() {
  beep(500);
  if (activeMode == MOD_MENUS) {  
    rClick();
    return;
  }
  if (!wait4Click && (activeMode == MOD_FSTOP || activeMode == MOD_TIME || activeMode == MOD_STRIPES)) {
    lastMode = activeMode;
    activeMode = MOD_PAUSE;
    turnLight(false);
    noTone(PIN_TONE);
    prnItem(false, false, I_CENTER, 0, S_PAUSE);
    ct = lt;
    return;
  }
  if (activeMode == MOD_PAUSE) {
    activeMode = lastMode;
    turnLight(true);
    timeStamp = millis();
    return;
  }
}

void chgMode(uint8_t m, float pt, float pf) {
  prnItem(false, false, I_CENTER, 0, S_READY);
  delay(1000);
  activeMode = m;
  turnLight(true);
  timeStamp = millis();
  ct = pt;
  cft = pt;
  cf = pf;
}

void rBtnPressLng() {
  beep(500);
  if (!isLightOn && activeMode == MOD_MENUS) {  
    if (screen == OPT_FSTOP_F) {
      nDodge = 0;
      nBurn = 0;
      chgMode(MOD_FSTOP, t, f);
    }
    if (screen == SCR_TIME) {
      chgMode(MOD_TIME, t, f);
    }
    if (screen == OPT_STRIPE_F) {
      csn = 0;
      chgMode(MOD_STRIPES, ct, cf);
    }
    return;
  }
  if (activeMode == MOD_PAUSE) {  
    prnItem(false, false, I_CENTER, 0, S_CLEAR);
    activeMode = MOD_MENUS;
    lastPosition = LP_UNDF;
    saveLog();
    printCount++;
    return;
  }
  if (activeMode == MOD_STRIPES || activeMode == MOD_FSTOP) {
    wait4Click = false;
    return;
  } 
}

void timerCounter() {
  uint32_t dts = millis() - timeStamp; // delta timestamp
  
  lt = ct - (float) dts / 1000;        // time left
  lf = log10(cft - lt) / log10(2);     // f left
    
  prnItem(false, true,  I_CENTER, 4, fts(F_SS, 3, 1, lt >= 0 ? lt : 0));
  if (activeMode == MOD_FSTOP || activeMode == MOD_TIME || activeMode == MOD_STRIPES) {
    prnItem(false, false, I_CENTER_RIGHT, 7, fts(F_FS, 3, 1, lf >= -3 ? lf : -3));
  }

  if (lt > 0) {
    if (snd >= 4 && dodge[0].f == 0 && (long)(lt * 10) % 10 == 0) {
      tone(PIN_TONE, 2000, 100);
    }
    if (screen == OPT_FSTOP_F && lf >= dodge[nDodge].tf && dodge[nDodge].f > 0) {
      nDodge++;
      tone(PIN_TONE, 2000, 200);
    }    
    if (activeMode == MOD_FSTOP) {
      prnItem(false, false, I_CENTER_LEFT,  2, lts(F_DODGED, nDodge));
      prnItem(false, false, I_CENTER_RIGHT, 2, lts(F_BURND, nBurn));
    }
    if (activeMode == MOD_STRIPES && lf >= f + ( df * csn )) {
      ct = lt;
      csn++;
      prnItem(false, false, I_CENTER, 2, lts(F_ND, sn - csn));
      turnLight(false);
      beepProg();
      saveLog();
      wait4Click = true;
      do {
        rBtn.read();
      } while (wait4Click);
      wait4Click = false;
      timeStamp = millis();
      turnLight(true);
    }
  } else {
    turnLight(false);
    if (screen == OPT_FSTOP_F && burn[nBurn].f > 0) {
      beepProg();
      cft = pow(2, f + burn[nBurn].f);
      ct = cft - pow(2, f);
      nBurn++;
      prnItem(false, false, I_CENTER_RIGHT, 2, lts(F_BURND, nBurn));
      wait4Click = true;
      do {
        rBtn.read();
      } while (wait4Click);
      wait4Click = false;
      turnLight(true);
      timeStamp = millis();
    } else {
      beepAcl();
      saveLog();
      delay(3000);
      activeMode = MOD_MENUS;
      lastPosition = LP_UNDF;
      printCount++;
    }
  }
}

void loop() {
  lBtn.read();
  rBtn.read();

  if (activeMode == MOD_MENUS) {
    encoderTurn();
    encoderClick();
  }
  
  if (activeMode == MOD_FSTOP || activeMode == MOD_TIME || activeMode == MOD_STRIPES) {
    timerCounter();
  }
}