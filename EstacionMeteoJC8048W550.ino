
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Arduino_GFX_Library.h>
#include "TAMC_GT911.h"
#include <LittleFS.h>
#include <PNGdec.h>
#include "JpegFunc.h"
#include "local_data.h"
#include "Fonts/meteocons16pt7b.h"
#include "Fonts/SpaceMono_Regular16pt7b.h"
#include "Fonts/SpaceMono_Regular10pt7b.h"

#define TOUCH_GT911
#define TOUCH_GT911_SCL 20
#define TOUCH_GT911_SDA 19
#define TOUCH_GT911_INT 18
#define TOUCH_GT911_RST 38
#define TOUCH_GT911_ROTATION ROTATION_NORMAL
#define TOUCH_MAP_X1 800
#define TOUCH_MAP_X2 0
#define TOUCH_MAP_Y1 480
#define TOUCH_MAP_Y2 0

TAMC_GT911 ts = TAMC_GT911(TOUCH_GT911_SDA, TOUCH_GT911_SCL, TOUCH_GT911_INT, TOUCH_GT911_RST, max(TOUCH_MAP_X1, TOUCH_MAP_X2), max(TOUCH_MAP_Y1, TOUCH_MAP_Y2));

#define PNG_TERMOMETRO "/Termometro.png"
#define PNG_HUMEDAD "/Humedad.png"
#define RGB565_DARKGREY 0x4208

/* Uncomment your device in Arduino_GFX_dev_device.h */
#include "Arduino_GFX_dev_device.h"

static const size_t Horas_predict = 96;

PNG png;
File pngFile;
int16_t xOffset, yOffset;

int32_t SCREEN_WIDTH = gfx->width();
int32_t SCREEN_HEIGHT = gfx->height();

int current_city = 0;
unsigned long ttime, stime;
String current_date;
String last_weather_update;
float temperature;
float humidity;
int is_day;
String weather_description;

int time_values[Horas_predict];
float temperature_2m_values[Horas_predict];
float maxt[7];
float mint[7];
int precipitation_probability_values[Horas_predict];
float precipitation_values[Horas_predict];
float maxrain = 0;
int weather_code_val[7];
int startgap = 18;
int daygap = 4;
int toque = 0;

struct mi_tiempo {
    String addr;
    String desc;
    String dicon;
} weath_val;

mi_tiempo get_weather_values(int code)
{
    mi_tiempo localw;
    //Convert Open-Meteo weather code to a textual description.
    if (code == 0){
        localw.addr = "/Clear_Sky";
        localw.desc = "Soleado";
        localw.dicon = "B";}
    else if (code == 1){
        localw.addr = "/Mainly _Clear";
        localw.desc = "Despejado";
        localw.dicon = "E";}
    else if (code == 2){
        localw.addr = "/Partly_Cloudy";
        localw.desc = "Sol y nubes";
        localw.dicon = "H";}
    else if (code == 3){
        localw.addr = "/Overcast";
        localw.desc = "Nublado";
        localw.dicon = "Y";}
    else if (code == 45 || code == 48){
        localw.addr = "/Fog";
        localw.desc = "Niebla";
        localw.dicon = "M";}
    else if (code == 51 || code == 53 || code == 55){
        localw.addr = "/Drizzle";
        localw.desc = "Llovizna";
        localw.dicon = "Q";}
    else if (code == 56 || code == 57){
        localw.addr = "/Freezing_Drizzle";
        localw.desc = "Llovizna helada";
        localw.dicon = "U";}
    else if (code == 61 || code == 63 || code == 65){
        localw.addr = "/Rain_Showers";
        localw.desc = "Lluvia";
        localw.dicon = "R";}
    else if (code == 66 || code == 67){
        localw.addr = "/Freezing_Rain";
        localw.desc = "Lluvia helada";
        localw.dicon = "V";}
    else if (code == 71 || code == 73 || code == 75){
        localw.addr = "/Snow_Fall";
        localw.desc = "Nieve";
        localw.dicon = "W";} 
    else if (code == 77){
        localw.addr = "/Freezing_Rain";
        localw.desc = "Granizo";
        localw.dicon = "X";} 
    else if (code == 80 || code == 81){
        localw.addr = "/Rain_Showers";
        localw.desc = "Chaparrones";
        localw.dicon = "R";} 
    else if (code == 82){
        localw.addr = "/Heavy_Rain";
        localw.desc = "Aguaceros fuertes";
        localw.dicon = "8";} 
    else if (code == 85 || code == 86){
        localw.addr = "/Snow_Fall";
        localw.desc = "Nevada";
        localw.dicon = "X";} 
    else if (code == 95){
        localw.addr = "/Thunderstorm";
        localw.desc = "Tormenta";
        localw.dicon = "P";} 
    else if (code == 96 || code == 99){
        localw.addr = "/Thunderstorm";
        localw.desc = "Tormentas de granizo";
        localw.dicon = "U";} 
    else {
        localw.addr = "/Unknow_weather";
        localw.desc = "Indefinido";
        localw.dicon = ")";}
    return localw;
}

static int jpegDrawCallback(JPEGDRAW *pDraw)
{
  // Serial.printf("Draw pos = %d,%d. size = %d x %d\n", pDraw->x, pDraw->y, pDraw->iWidth, pDraw->iHeight);
  gfx->draw16bitBeRGBBitmap(pDraw->x, pDraw->y, pDraw->pPixels, pDraw->iWidth, pDraw->iHeight);
  return 1;
}

void *myOpen(const char *filename, int32_t *size)
{
  pngFile = LittleFS.open(filename, "r");

  if (!pngFile || pngFile.isDirectory())
  {
    Serial.println(F("ERROR: Failed to open file for reading"));
    gfx->println(F("ERROR: Failed to open file for reading"));
  }
  else
  {
    *size = pngFile.size();
    Serial.printf("Opened '%s', size: %d\n", filename, *size);
  }
  return &pngFile;
}

void myClose(void *handle)
{
  if (pngFile)
    pngFile.close();
}

int32_t myRead(PNGFILE *handle, uint8_t *buffer, int32_t length)
{
  if (!pngFile)
    return 0;
  return pngFile.read(buffer, length);
}

int32_t mySeek(PNGFILE *handle, int32_t position)
{
  if (!pngFile)
    return 0;
  return pngFile.seek(position);
}

// Function to draw pixels to the display
void PNGDraw(PNGDRAW *pDraw)
{
  uint16_t usPixels[320];
  uint8_t usMask[320];

  // Serial.printf("Draw pos = 0,%d. size = %d x 1\n", pDraw->y, pDraw->iWidth);
  png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_LITTLE_ENDIAN, 0x00000000);
  png.getAlphaMask(pDraw, usMask, 1);
  gfx->draw16bitRGBBitmapWithMask(xOffset, yOffset + pDraw->y, usPixels, usMask, pDraw->iWidth, 1);
}

void get_weather_data() {
  String latitud = ciudad[current_city].lat;
  String longitud = ciudad[current_city].lon;
  String nombre = ciudad[current_city].nom;
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    // Construct the API endpoint
    String url = String("http://api.open-meteo.com/v1/forecast?latitude=" + latitud + "&longitude=" + longitud + "&daily=weather_code&current=temperature_2m,relative_humidity_2m,is_day,precipitation,rain,weather_code&hourly=temperature_2m,precipitation_probability,precipitation");
    http.begin(url);
    int httpCode = http.GET(); // Make the GET request

    if (httpCode > 0) {
      // Check for the response
      if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();

        // Parse the JSON to extract the time
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (!error) {
          const char* datetime = doc["current"]["time"];
          temperature = doc["current"]["temperature_2m"];
          humidity = doc["current"]["relative_humidity_2m"];
          //is_day = String(doc["current"]["is_day"]).toInt();
          for (int i=0; i < Horas_predict; i++){
              time_values[i] = doc["hourly"]["time"][i];
              temperature_2m_values[i]= doc["hourly"]["temperature_2m"][i];
              precipitation_probability_values[i]= doc["hourly"]["precipitation_probability"][i];
              precipitation_values[i]= doc["hourly"]["precipitation"][i];
              if(precipitation_values[i] > maxrain) {maxrain = precipitation_values[i];}
          }
          for (int j=0; j < Horas_predict/24; j++){
            maxt[j] = 0; mint[j] = 100;
            for (int k=0; k < 24; k++){
              if(temperature_2m_values[k+j*24] > maxt[j]){
                maxt[j] = temperature_2m_values[k+j*24];
              }
              if(temperature_2m_values[k+j*24] < mint[j]){
                mint[j] = temperature_2m_values[k+j*24];
              }
            }
          }
          for (int i=0; i < 7; i++){
            weather_code_val[i] = doc["daily"]["weather_code"][i];
          }       

          gfx->fillScreen(RGB565_BLUE);
          weath_val = get_weather_values(weather_code_val[0]);
          String wdesc = weath_val.desc;
          String waddr = weath_val.addr;
          char buff[40];
          const char *tipo = ".jpg";
          strcpy(buff, waddr.c_str());
          strcat(buff, tipo);
          jpegDraw(buff, jpegDrawCallback, true /* useBigEndian */,
             0 /* x */, 0 /* y */, gfx->width() /* widthLimit */, gfx->height() /* heightLimit */);
          //tft.setFreeFont(&meteocons24pt7b);
          //tft.print(wicon);
        int rc;
        xOffset = 15;
        yOffset = 90;
        rc = png.open(PNG_TERMOMETRO, myOpen, myClose, myRead, mySeek, PNGDraw);
        if (rc == PNG_SUCCESS) {
          rc = png.decode(NULL, 0);
          png.close();
        }
          gfx->setFont(&SpaceMono_Regular16pt7b);
          gfx->setTextColor(RGB565_BLACK);
          gfx->setCursor(42,117);
          gfx->print(temperature, 1);
          gfx->print("'");
          gfx->print("C (");
          gfx->print(mint[0], 1);
          gfx->print("-");
          gfx->print(maxt[0], 1);
          gfx->print(")");
          gfx->setTextColor(RGB565_YELLOW);
          gfx->setCursor(40,115);
          gfx->print(temperature, 1);
          gfx->print("'");
          gfx->print("C (");
          gfx->setTextColor(RGB565_BLUE);
          gfx->print(mint[0], 1);
          gfx->setTextColor(RGB565_YELLOW);
          gfx->print("-");
          gfx->setTextColor(RGB565_RED);
          gfx->print(maxt[0], 1);
          gfx->setTextColor(RGB565_YELLOW);
          gfx->print(")");
          xOffset = 10;
          yOffset = 130;
          rc = png.open(PNG_HUMEDAD, myOpen, myClose, myRead, mySeek, PNGDraw);
          if (rc == PNG_SUCCESS) {
            rc = png.decode(NULL, 0);
            png.close();
          }
          //gfx->print(") / ");
          char tmpstr[4];
          snprintf(tmpstr, sizeof(tmpstr), "%f", humidity);
          strcat(tmpstr, "%");
          textshadow(40, 160, RGB565_WHITE, RGB565_BLACK, tmpstr);

          char nomstr[40];
          snprintf(nomstr, sizeof(nomstr), "%s%s%s%s", nombre, " (", wdesc, ")");
          textshadow(20, 75, RGB565_WHITE, RGB565_BLACK, nomstr);

          // Split the datetime into date and time
          String datetime_str = String(datetime);
          int splitIndex = datetime_str.indexOf('T');
          String current_date_eng = datetime_str.substring(0, splitIndex);
          String fecha_year = current_date_eng.substring(2, 4);
          String fecha_month = current_date_eng.substring(5, 7);
          String fecha_day = current_date_eng.substring(8, 10);
          current_date = fecha_day+"-"+fecha_month+"-"+fecha_year;
          dibujadias(fecha_year.toInt(),fecha_month.toInt(),fecha_day.toInt());
          dibujalluvia(Horas_predict, 50, precipitation_values);
          dibujatemp(Horas_predict, 50, temperature_2m_values);
          last_weather_update = datetime_str.substring(splitIndex + 1, splitIndex + 9); // Extract time portion
        } else {
          Serial.print("deserializeJson() failed: ");
          Serial.println(error.c_str());
        }
      }
      else {
        Serial.println("Failed");
      }
    } else {
      Serial.printf("GET request failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end(); // Close connection
  } else {
    Serial.println("Not connected to Wi-Fi");
  }
}

void textshadow (int x, int y, uint16_t color1, uint16_t color2, String texto)
{
  gfx->setCursor(x+2,y+2);
  gfx->setTextColor(color2);
  gfx->print(texto);
  gfx->setCursor(x,y);
  gfx->setTextColor(color1);
  gfx->print(texto);
}
void dibujatemp(int rango, int max, float t_values[Horas_predict] )
{
  float w2 = SCREEN_WIDTH/rango;
  int y1 = SCREEN_HEIGHT;
  int x1 = startgap;
  gfx->setFont(&SpaceMono_Regular10pt7b);
  gfx->setTextColor(RGB565_ORANGE);
  int j = -1;
  int tminpos[] = {0, 0, 0, 0};
  int tmaxpos[] = {0, 0, 0, 0};
  //int txtalt = 10;
  for (int i=0; i < rango; i++){
    if(i%24 == 0){
      j++;
    }
    int h2 = (int)(5*t_values[i]);
    int y2 = SCREEN_HEIGHT-h2;
    int x2 = (int)(startgap+w2*i);
    if(i){ 
      gfx->drawLine(x1, y1, x2, y2, RGB565_ORANGE);
      gfx->drawLine(x1, y1-1, x2, y2-1, RGB565_ORANGE); 
      gfx->drawLine(x1, y1+1, x2, y2+1, RGB565_ORANGE);           
      if(t_values[i] == maxt[j]){
        if(!tmaxpos[j]){
          gfx->setCursor(x1-14,y1-9);
          gfx->setTextColor(RGB565_BLACK);
          gfx->print(t_values[i], 1);
          gfx->setCursor(x1-15,y1-10);
          gfx->setTextColor(RGB565_RED);
          gfx->print(t_values[i], 1);
          tmaxpos[j] = 1;
        }

      }
      else if(t_values[i] == mint[j]){
        if((rango - i) < 2) {x1 -=6;}
        if(!tminpos[j]){
          gfx->setCursor(x1-14,y1+21);
          gfx->setTextColor(RGB565_WHITE);
          gfx->print(t_values[i], 1);
          gfx->setCursor(x1-15,y1+20);
          gfx->setTextColor(RGB565_BLUE);
          gfx->print(t_values[i], 1);
          tminpos[j] = 1;
        }

      }
    }
    x1 = x2;
    y1 = y2;
    //tft.setCursor(x1+ajst, SCREEN_HEIGHT-txtalt, 1);
    //tft.print(hora);
  }
}

void dibujalluvia(int rango, int max, float t_values[Horas_predict] )
{
  float w2 = SCREEN_WIDTH/rango;
  int x1 = 0;
  int ajst = startgap;
  int altmax = SCREEN_HEIGHT/2;
  int limprec = 5;
  if(max > limprec){
    limprec = max;
  }
  
  for (int i=0; i < rango; i++){
    int h2 = (int)((log(t_values[i]+1)/log(limprec))*altmax);
    int y1 = SCREEN_HEIGHT-h2;
    x1 = (int)(w2*i);
    gfx->fillRect(x1+2+ajst, y1-2, w2, h2, RGB565_DARKGREY);
    for (int j = 0; j < h2; j++)
      {
        int rgbmap = map(j, 0, altmax, 255, 64);
        //Serial.println(rgbmap); Serial.print(" ");
        gfx->drawFastHLine(x1+ajst, y1+j, w2, gfx->color565(64, 64, rgbmap));
      }
    //gfx->fillRect(x1+1+ajst, y1-2, w2, h2, RGB565_BLACK);
    
  }
}

void dibujadias(int anio, int mes, int dia){
  int w2 = daygap;
  int hoy = weekday (anio, mes, dia);
  int ndias = Horas_predict/24;
  int ancho_dia = (int)(SCREEN_WIDTH/ndias - 2*(ndias+1));
  gfx->setTextColor(RGB565_BLACK);
  for(int i=0; i<ndias; i++){
    xOffset = startgap+i*(w2+ancho_dia);
    yOffset = SCREEN_HEIGHT/2-10;
    gfx->fillRoundRect(startgap+2+i*(w2+ancho_dia), SCREEN_HEIGHT/2-8, ancho_dia-10, SCREEN_HEIGHT/2+12, 10, RGB565_DARKGREY);
    int diasp = hoy+i;
    if(diasp > 6) {diasp -= 7;}
    weath_val = get_weather_values(weather_code_val[i]);
    String wicon = weath_val.dicon;
    String waddr = weath_val.addr;
    char buff[40];
    const char *tipo = ".png";
    strcpy(buff, waddr.c_str());
    strcat(buff, tipo);
    int rc;
    rc = png.open(buff, myOpen, myClose, myRead, mySeek, PNGDraw);
    if (rc == PNG_SUCCESS)
    {
    /*  int16_t pw = png.getWidth();
      int16_t ph = png.getHeight();

      xOffset = (w - pw) / 2;
      yOffset = (h - ph) / 2;
*/
      rc = png.decode(NULL, 0);

      //Serial.printf("Draw offset: (%d, %d), time used: %lu\n", xOffset, yOffset, millis() - start);
      //Serial.printf("image specs: (%d x %d), %d bpp, pixel type: %d\n", png.getWidth(), png.getHeight(), png.getBpp(), png.getPixelType());
      png.close();
    }
    else
    {
      Serial.println("png.open() failed!");
    }
    //String wicon = getMeteoconIcon(weather_code_val[i]);
    gfx->setTextColor(RGB565_BLACK);
    gfx->setCursor(startgap+5+i*(6+ancho_dia), 21+SCREEN_HEIGHT/2);
    gfx->setFont(&SpaceMono_Regular10pt7b);
    gfx->print(diadelasemana[diasp]);
    gfx->setFont(&meteocons16pt7b);
    gfx->print(wicon);
    gfx->setTextColor(RGB565_WHITE);
    gfx->setCursor(startgap+4+i*(6+ancho_dia), 20+SCREEN_HEIGHT/2);
    gfx->setFont(&SpaceMono_Regular10pt7b);
    gfx->print(diadelasemana[diasp]);
    gfx->setFont(&meteocons16pt7b);
    gfx->print(wicon);
  }
  gfx->setFont(&SpaceMono_Regular16pt7b);
  char datstr[40];
  snprintf(datstr, sizeof(datstr), "%s %s", fullweekday[hoy], current_date);
  textshadow(20, 40, RGB565_YELLOW, RGB565_BLACK, datstr);
}

int weekday(int year, int month, int day)
/* Calculate day of week. Sunday == 0. */
{
  int adjustment, mm, yy;
  if (year<2000) year+=2000;
  adjustment = (14 - month) / 12;
  mm = month + 12 * adjustment - 2;
  yy = year - adjustment;
  return (day + (13 * mm - 1) / 5 +
    yy + yy / 4 - yy / 100 + yy / 400) % 7;
}

void setup() {
  ttime = millis();
  stime = millis();
  ts.begin();
  ts.setRotation(TOUCH_GT911_ROTATION);

  #ifdef DEV_DEVICE_INIT
  DEV_DEVICE_INIT();
  #endif

  #ifdef GFX_BL
    pinMode(GFX_BL, OUTPUT);
    digitalWrite(GFX_BL, HIGH);
    ledcSetup(0, 300, 8);
    ledcAttachPin(GFX_BL, 0);
    ledcWrite(0, 224); 
  #endif

  Serial.begin(115200);

  // Init Display
  if (!gfx->begin())
  // if (!gfx->begin(80000000)) /* specify data bus speed */
  {
    Serial.println("gfx->begin() failed!");
  }

  if (!LittleFS.begin())
    {
      Serial.println(F("ERROR: File System Mount Failed!"));
      gfx->println(F("ERROR: File System Mount Failed!"));
    }
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected to Wi-Fi network with IP Address: ");
  Serial.println(WiFi.localIP());
  
  get_weather_data();  
}

void loop() {
  ts.read();
  if(ts.isTouched && toque == 0){
    current_city++;
    if(current_city > 2){
      current_city = 0;
    }
    ledcWrite(0, 224); 
    get_weather_data();
    toque = 1;
    stime = millis();
  }
  else {
    toque = 0;
  }
  if((millis() - stime) > 30000 || millis() < ttime){
    ledcWrite(0, 64); 
    //stime = millis();
  }
  if((millis() - ttime) > 1200000 || millis() < ttime){
    get_weather_data();
    ttime = millis();
  }

}
 