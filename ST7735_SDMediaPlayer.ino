// ST7735 library example
// SD Media Player/File Viewer with File Browser
// Requires SdFat, Arduino_ST7735_STM and RREFont libraries and stm32duino
// (C)2019-20 Pawel A. Hernik
// YouTube videos: https://youtu.be/6Uh5Iu-erO0 and https://youtu.be/o3AqITHf0mo 

/*
 ST7735 128x160 1.8" LCD pinout (header at the top, from left):
 #1 LED   -> 3.3V
 #2 SCK   -> SCL/D13/PA5
 #3 SDA   -> MOSI/D11/PA7
 #4 A0/DC -> D8/PA1  or any digital
 #5 RESET -> D9/PA0  or any digital
 #6 CS    -> D10/PA2 or any digital
 #7 GND   -> GND
 #8 VCC   -> 3.3V

           SPI2/SPI1
 SD_SCK  - PB13/PA5
 SD_MISO - PB14/PA6
 SD_MOSI - PB15/PA7
 SD_CS   - PB12/PA4
*/

/*
 STM32 SPI1/SPI2 pins:
 
 SPI1 MOSI PA7
 SPI1 MISO PA6
 SPI1 SCK  PA5
 SPI1 CS   PA4

 SPI2 MOSI PB15
 SPI2 MISO PB14
 SPI2 SCK  PB13
 SPI2 CS   PB12
*/

/*
 Features:
 - SD file browser with one button
 - Short click for next file/switch stat mode
 - Long click to show file or exit the viewer
 - Semi-transparent progress bar
 - Long file names (up to 23 characters fit on the screen) and file size displayed

 Comments:
 - SD uses faster STM32 SPI1 interface which supports 36 Mbps
 - SPI1 is shared between LCD and SD card
 - Not all SD cards work at 36MBps
 - Fast card at 36Mbps gives 41fps for 160x128 video
 - SdFat library uses DMA for SPI transfer
 - Big buffer in RAM is used to speed up SPI/DMA transfer
*/

#include <SPI.h>
#include <Adafruit_GFX.h>

#if (__STM32F1__) // bluepill
#define TFT_CS  PA2
#define TFT_DC  PA1
#define TFT_RST PA0
#include <Arduino_ST7735_STM.h>
#else
#define TFT_CS 10
#define TFT_DC  8
#define TFT_RST 9
//#include <Arduino_ST7735_Fast.h>
#endif

#define SCR_WD 160
#define SCR_HT 128
Arduino_ST7735 lcd = Arduino_ST7735(TFT_DC, TFT_RST, TFT_CS);

#define NLINES 32
#define BUF_WD 160
uint16_t buf[BUF_WD*NLINES]; 
char txt[30];

#include "RREFont.h"
#include "rre_5x8.h"
RREFont font;

// -------------------------
// renders directly to LCD
void customRect(int x, int y, int w, int h, int c) { lcd.fillRect(x, y, w, h, c); }
// -------------------------
// renders to the buffer
void customRectBuf(int x, int y, int w, int h, int c)
{
  if(y>NLINES) return;
  if(y+h>NLINES) h=NLINES-y;
  for(int j=0;j<h;j++) for(int i=0;i<w;i++) buf[(y+j)*BUF_WD+x+i]=c;
}
// --------------------------------------------------------------------------

#include "SdFat.h"

#define USE_SDIO 0
//const uint8_t SD_CS = PB12;
//SdFat sd(2);
const uint8_t SD_CS = PA4;
SdFat sd(1);

SdFile file;

void lcdSPI()
{
  SPI.beginTransaction(SPISettings(36000000, MSBFIRST, SPI_MODE3, DATA_SIZE_16BIT));
}

// use 18 if your SD card doesn't work
#define SD_SPEED 36
//#define SD_SPEED 18
void sdSPI()
{
  SPI.beginTransaction(SD_SCK_MHZ(SD_SPEED));
}

// ------------------------------------------------
#define BUTTON PB9
int buttonState;
int prevState = HIGH;
long btDebounce    = 30;
long btMultiClick  = 600;
long btLongClick   = 500;
long btLongerClick = 2000;
long btTime = 0, btTime2 = 0;
int clickCnt = 1;

// 0=idle, 1,2,3=click, -1,-2=longclick
int checkButton()
{
  int state = digitalRead(BUTTON);
  if( state == LOW && prevState == HIGH ) { btTime = millis(); prevState = state; return 0; } // button just pressed
  if( state == HIGH && prevState == LOW ) { // button just released
    prevState = state;
    if( millis()-btTime >= btDebounce && millis()-btTime < btLongClick ) { 
      if( millis()-btTime2<btMultiClick ) clickCnt++; else clickCnt=1;
      btTime2 = millis();
      return clickCnt; 
    } 
  }
  if( state == LOW && millis()-btTime >= btLongerClick ) { prevState = state; return -2; }
  if( state == LOW && millis()-btTime >= btLongClick ) { prevState = state; return -1; }
  return 0;
}

int prevButtonState=0;

int handleButton()
{
  prevButtonState = buttonState;
  buttonState = checkButton();
  return buttonState;
}

// --------------------------------------------------------------------------
void error(char *err, uint16_t col, int8 halt=1)
{
  lcdSPI(); lcd.fillScreen(col);
  font.setColor(YELLOW);
  font.setFillRectFun(customRect);
  font.setBold(1);
  font.setSpacingY(4);
  font.printStr(4,4,err);
  font.setBold(0);
  if(halt) sd.errorHalt(err); else Serial.println(err);
}
// --------------------------------------------------------------------------
int statMode=1; // 0-0ff, 1-progress, 2-full, 3-fps

void darken(uint16_t *p)
{
  uint16_t c = *p;
  int r,g,b;
  r = (c>>8)&0xf8;
  g = (c>>3)&0xfc;
  b = (c<<3)&0xf8;
  //*p = RGBto565(r/2,g/2,b/2);  // blend to black
  *p = RGBto565(r+(60-r)/2,g+(60-g)/2,b+(60-b)/2); // blend to dark grey
}

void drawProgress(int y, int p)
{
  int i;
  for(i=0; i<p; i++) buf[10+(y+0)*BUF_WD+i]=buf[10+(y+1)*BUF_WD+i]=buf[10+(y+2)*BUF_WD+i]=buf[10+(y+3)*BUF_WD+i]=RED;
  //for(i=p; i<140; i++) buf[10+5*BUF_WD+i]=buf[10+6*BUF_WD+i]=buf[10+7*BUF_WD+i]=buf[10+8*BUF_WD+i]=RGBto565(120,120,120);
  for(i=p; i<140; i++) {
    darken(&buf[10+(y+0)*BUF_WD+i]);
    darken(&buf[10+(y+1)*BUF_WD+i]);
    darken(&buf[10+(y+2)*BUF_WD+i]);
    darken(&buf[10+(y+3)*BUF_WD+i]);
  }
}

// --------------------------------------------------------------------------
// Params:
// filename - file name
// x,y - start x,y on the screen
// wd,ht - width, height of the video (raw data has no header with such info)
// nl - num lines read in one operation (nl*wd*2 bytes are loaded)
// skipFr - num frames to skip
int showVideo(char *filename, int wd, int ht, int nl, int skipFr)
{
  sdSPI();
  if(!file.open(filename, O_CREAT | O_RDONLY)) {
    snprintf((char*)buf,100,"Cannot open\n%s\n",filename);
    error((char*)buf,BLUE,0);
    delay(1000);
    //return -1;
  }
  file.seekSet(0);
  unsigned long sdStartTime,frTime,lcdTime,sdTime=0,statTime=0,statStartTime;
  handleButton();
  while(file.available()) {
    sdTime = statTime = 0;
    frTime = millis();
    for(int i=0;i<ht/nl;i++) {
      sdStartTime = millis();
      int rd = file.read(buf,wd*2*nl);
      sdTime += millis()-sdStartTime;
      
      statStartTime = millis();
      if(i==(ht/nl)-1 && NLINES>=8 && statMode>0) {
        if(statMode==1) drawProgress(NLINES-8,140*(file.curPosition()/1000)/(file.fileSize()/1000)); else
        if(statMode>1) {
          font.setFillRectFun(customRectBuf);
          font.setColor(BLACK);
          font.printStr(1,NLINES-6,txt);
          font.printStr(2,NLINES-6,txt);
          font.printStr(3,NLINES-6,txt);
          font.printStr(1,NLINES-8,txt);
          font.printStr(2,NLINES-8,txt);
          font.printStr(3,NLINES-8,txt);
          font.printStr(1,NLINES-7,txt);
          font.printStr(3,NLINES-7,txt);
          font.setColor(YELLOW);
          font.printStr(2,NLINES-7,txt);
        }
      }
      statTime += millis()-statStartTime;

      lcdSPI();
      lcd.drawImage(0,i*nl,lcd.width(),nl,buf);
    }
    frTime = millis()-frTime-statTime;
    lcdTime = frTime-sdTime;
    if(buttonState>0) {
      if(++statMode>3) statMode=0;
    }
    if(statMode==2) snprintf(txt,30,"Fr/SD/LCD: %2ld/%2ld/%2ld FPS:%2d",frTime,sdTime,lcdTime,1000/frTime);
    if(statMode==3) snprintf(txt,30,"%2ld fps",1000/frTime);

    if(skipFr>0) file.seekCur(wd*ht*2*skipFr);
    if(handleButton()<0 && prevButtonState==0) break;
  }
  file.close();
  while(handleButton()==0);
  return 1;
}
// --------------------------------------------------------------------------
// Limited to LCD resolution
int showBMP(char *filename)
{
  int bmpWd, bmpHt, bmpBits, bmpNumCols, y=0;
  uint16_t pal[256];
  
  sdSPI();
  if(!file.open(filename, O_CREAT | O_RDONLY)) {
    lcdSPI(); lcd.fillScreen(YELLOW);
    Serial.print(F("Cannot open "));
    Serial.println(filename);
    delay(1000);
    //return -1;
  }
  file.seekSet(0);
  file.read(buf,54);
  uint8_t *buf8 = (uint8_t *)buf;
  bmpWd = buf8[18]+buf8[19]*256;
  bmpHt = buf8[22]+buf8[23]*256;
  bmpBits = buf8[28];
  bmpNumCols = buf8[46]+buf8[47]*256;
  //Serial.print(bmpWd); Serial.print(" x "); Serial.print(bmpHt); Serial.print(" x "); Serial.print(bmpBits); Serial.print(" bpp"); 
  //if(bmpBits<=8) { Serial.print(" / "); Serial.print(bmpNumCols); Serial.print(" colors"); }
  //Serial.println(); 
  if(bmpBits<=8) {
    file.read(buf,bmpNumCols*4);
    for(int i=0;i<bmpNumCols;i++) pal[i]=RGBto565(buf8[2+i*4],buf8[1+i*4],buf8[i*4]);
  }
  while(file.available() && y<bmpHt) {
    buf8 = (uint8_t *)buf+BUF_WD*2;
    if(bmpBits==4) {
      file.read(buf8,bmpWd/2);
      for(int i=0;i<bmpWd/2;i++) {
        buf[i*2+0] = pal[buf8[i]>>4];
        buf[i*2+1] = pal[buf8[i]&0xf];
      }
    } else
    if(bmpBits==8) {
      file.read(buf8,bmpWd);
      for(int i=0;i<bmpWd;i++) buf[i] = pal[buf8[i]];
    } else {
      file.read(buf8,bmpWd*3);
      for(int i=0;i<bmpWd;i++) buf[i] = RGBto565(buf8[i*3+2],buf8[i*3+1],buf8[i*3+0]);
    }
    lcdSPI(); lcd.drawImage((lcd.width()-bmpWd)/2,lcd.height()-1-y,bmpWd,1,buf);
    y++;
  }
  file.close();
  while(handleButton()==0 || prevButtonState!=0);
  return 1;
}

// --------------------------------------------------------------------------
int showTxt(char *filename)
{
  sdSPI();
  if(!file.open(filename, O_CREAT | O_RDONLY)) {
    lcdSPI(); lcd.fillScreen(YELLOW);
    Serial.print(F("Cannot open "));
    Serial.println(filename);
    delay(1000);
    //return -1;
  }
  file.seekSet(0);
  while(file.available()) {
    int rd = file.read(buf,NLINES*BUF_WD*2);
    char *txt = (char*)buf;
    txt[rd-1]=0;
    lcdSPI();
    lcd.fillScreen(RGBto565(0,0,100));
    font.setColor(YELLOW);
    font.setSpacingY(1);
    font.setCR(1);
    font.printStr(0,0,txt);
  }
  file.close();
  while(handleButton()==0 || prevButtonState!=0);
  return 1;
}

// --------------------------------------------------------------------------
char *getExt(char *filename)
{
  int len = strlen(filename);
  return len>3 ? filename+len-3 : filename;
}

int checkExt(char *filename, char *ext)
{
  return strcmp(getExt(filename),ext)==0;
}
// --------------------------------------------------------------------------
// SD file browser data
#define MAX_NAME_LEN 23
#define MAX_DIR_LEN  50
#define MAX_SIZE_LEN 5
const int charWd = 6;
const int lineHt = 12;
const int numScreenFilesMax = 9;
int numScreenFiles = 0;
char filesList[numScreenFilesMax][MAX_NAME_LEN+1];  // last char filesList[MAX_NAME_LEN] is used as file/dir mode
char filesSize[numScreenFilesMax][MAX_SIZE_LEN];
int selFile = 0;
int fileAvailable = 1;
uint32_t dirPos;
bool rootDir = true;
char curDir[MAX_DIR_LEN];
int ys = 14, xs = 2; 

void fileList(int rewind=0)
{
  numScreenFiles = 0;
  selFile = 0;
  sdSPI();
  if(rewind || !fileAvailable) {
    dirPos = 0;
    if(!rootDir) {
      strcpy(filesList[numScreenFiles],"..");
      filesList[numScreenFiles][MAX_NAME_LEN] = 2;
      numScreenFiles++;
    }
  }
  sd.vwd()->seekSet(dirPos);
  while(numScreenFiles<numScreenFilesMax) {
    fileAvailable = file.openNext(sd.vwd(), O_READ);
    if(!fileAvailable) { file.close(); break; }
    file.getName(filesList[numScreenFiles],MAX_NAME_LEN);
    filesList[numScreenFiles][MAX_NAME_LEN-1] = 0;
    //Serial.print(filesList[numScreenFiles]);
    if(file.isDir()) {
      filesList[numScreenFiles][MAX_NAME_LEN] = 2;
      //Serial.println("/");
    } else {
      if(checkExt(filesList[numScreenFiles],"raw") || checkExt(filesList[numScreenFiles],"txt") || checkExt(filesList[numScreenFiles],"bmp"))
        filesList[numScreenFiles][MAX_NAME_LEN] = 1;
      else
        filesList[numScreenFiles][MAX_NAME_LEN] = 0;
      uint32_t fsize = file.fileSize();
      if(fsize>1000*1000*1000) snprintf(filesSize[numScreenFiles],MAX_SIZE_LEN,"%dG",fsize>>30);
      else if(fsize>1000*1000) snprintf(filesSize[numScreenFiles],MAX_SIZE_LEN,"%dM",fsize>>20);
      else if(fsize>1999)      snprintf(filesSize[numScreenFiles],MAX_SIZE_LEN,"%dK",fsize>>10);
      else                     snprintf(filesSize[numScreenFiles],MAX_SIZE_LEN,"%dB",fsize);
      filesSize[numScreenFiles][MAX_SIZE_LEN-1] = 0;
      //Serial.print("\t\t"); Serial.println(filesSize[numScreenFiles]);
    }
    numScreenFiles++;
    file.close();
  }
  dirPos = sd.vwd()->curPosition();
  if(numScreenFiles>0) fileListShow();
}

// --------------------------------------------------------------------------
uint16_t bgCol(int i)
{
  return i&1 ? RGBto565(30,30,30) : RGBto565(50,50,50);
}

void selFrame(int i, uint16_t c)
{
  lcdSPI();
  lcd.drawRect(0,ys+i*lineHt,(MAX_NAME_LEN-1)*charWd+4,lineHt,c);
}

void selFrameActive()
{
  selFrame(selFile,filesList[selFile][MAX_NAME_LEN] ? GREEN : RED);
}

void fileListShow()
{
  lcdSPI(); lcd.fillScreen(BLACK);
  font.setFillRectFun(customRect);
  lcd.drawFastHLine(0,0,lcd.width(),RGBto565(0,0,180));
  lcd.drawFastHLine(0,ys-1,lcd.width(),RGBto565(0,0,180));
  lcd.drawFastHLine(0,1,lcd.width(),RGBto565(0,0,200));
  lcd.drawFastHLine(0,ys-2,lcd.width(),RGBto565(0,0,200));
  lcd.drawFastHLine(0,2,lcd.width(),RGBto565(0,0,220));
  lcd.drawFastHLine(0,ys-3,lcd.width(),RGBto565(0,0,220));
  lcd.fillRect(0,3,lcd.width(),ys-6,BLUE);
  font.setColor(YELLOW);
  font.printStr(xs,3,curDir);
  for(int i=0;i<numScreenFiles;i++) {
    lcd.fillRect(0,ys+i*lineHt,lcd.width(),lineHt,bgCol(i));
    if(filesList[i][MAX_NAME_LEN]==2) {
      font.setColor(YELLOW);
      font.printStr(xs+charWd,ys+2+i*lineHt,filesList[i]);
      font.printStr(xs,ys+2+i*lineHt,"["); font.printStr(xs+6+strlen(filesList[i])*charWd,ys+2+i*lineHt,"]");
      font.printStr(lcd.width()-3*charWd+1,ys+2+i*lineHt,"DIR");
    } else {
      font.setColor(filesList[i][MAX_NAME_LEN] ? WHITE : RGBto565(190,190,190)); 
      font.printStr(xs,ys+2+i*lineHt,filesList[i]);
      font.printStr(lcd.width()-strlen(filesSize[i])*charWd+1,ys+2+i*lineHt,filesSize[i]);
    }
  }
}

// --------------------------------------------------------------------------

int handleFile(char *filename)
{
  if(checkExt(filename,"raw")) return showVideo(filename, 160,128, 32,0); else
  if(checkExt(filename,"txt")) return showTxt(filename); else
  if(checkExt(filename,"bmp")) return showBMP(filename); else
  return 0;
}

// --------------------------------------------------------------------------

void setup(void)
{
  Serial.begin(115200);
  pinMode(BUTTON, INPUT_PULLUP);
  lcd.init();
  lcd.setRotation(3);
  lcd.fillScreen(BLACK);
  font.init(customRect, SCR_WD, SCR_HT); // custom fillRect function and screen width and height values
  font.setFont(&rre_5x8); font.setFontMinWd(5);

  //delay(8000);
  if(!sd.cardBegin(SD_CS, SD_SCK_MHZ(SD_SPEED)))  
    error("\nSD Card\ninitialization\nfailed.\n",RED);
  if(!sd.fsBegin()) 
    error("\nFile System\ninitialization\nfailed.\n",MAGENTA);
 
  strcpy(curDir,"/stm32");
  if(!sd.chdir(curDir)) strcpy(curDir,"/");
  //showVideo("budlight_160x128.raw", 160,128, 32,0);
  fileList(1);
  selFrameActive();
}

// --------------------------------------------------------------------------

void loop(void)
{
  handleButton();
  if(buttonState>0) {
    selFrame(selFile,bgCol(selFile));
    if(++selFile>=numScreenFiles) {
      fileList();
      if(numScreenFiles==0) fileList(1); 
    }
    selFrameActive();
  }
  if(buttonState<0 && prevButtonState==0) {
    if(filesList[selFile][MAX_NAME_LEN]==2) {
      if(filesList[selFile][0]=='.') {
        char *last = strrchr(curDir,'/');
        if(last && last!=curDir) *last=0;
        last = strrchr(curDir,'/');
        if(last==curDir) rootDir = true;
      } else {
        rootDir = false;
        if(strlen(curDir)+strlen(filesList[selFile])+2<MAX_DIR_LEN) {
          strcat(curDir,"/");
          strcat(curDir,filesList[selFile]);
        }
      }
      sd.chdir(curDir);
      fileList(1);
      selFrameActive();
    } else
    if(handleFile(filesList[selFile])) {
      fileListShow();
      selFrameActive();
    }
  }
}

// ------------------------------------------------

