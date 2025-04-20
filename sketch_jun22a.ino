#include <Arduino.h>
#include <RtcDS1302.h>
#include <U8g2lib.h>
#ifdef U8X8_HAVE_HW_SPI
#include <SPI.h>
#endif    
#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif
#include <WiFi.h> 
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <string>


// OLED display
U8G2_SSD1306_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE, 14, 12);

// RTC module
ThreeWire myWire(33, 32, 27); // Io, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);


// WiFi and NTP setup
/*const char* ssid = "abcde";
const char* password = "88888888";*/
//You may add your own WiFi
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp.ntsc.ac.cn", 8*3600, 1); // NTP client


#define countof(a) (sizeof(a) / sizeof(a[0]))

u8g2_uint_t offset;
u8g2_uint_t width;
const char *text = "";
//The text will be shown when it is booting
short year,month,day,hour,minute,second;

struct ap_info
{
  unsigned char count;
  String mac;
  signed char rssi;
}apinfo;

void wifi_scan()
{
  short n = WiFi.scanNetworks();
  apinfo.count =n;
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (short i = 0; i < n; ++i) {
      apinfo.rssi = WiFi.RSSI(i);
      apinfo.mac = WiFi.BSSIDstr(i);
      delay(10);
    }
  }
}

void days_to_date_and_time(unsigned long total_seconds_since_1970) {
    // 定义每月的天数，不考虑闰年
    short days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    // 一天的总秒数
    long seconds_in_day = 24 * 60 * 60;

    // 计算年份
    year = 1970;
    while (total_seconds_since_1970 >= 365 * seconds_in_day) {
        if ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0) {
            // 是闰年
            if (total_seconds_since_1970 >= 366 * seconds_in_day) {
                total_seconds_since_1970 -= 366 * seconds_in_day;
                (year)++;
            } else {
                break;
            }
        } else {
            // 不是闰年
            total_seconds_since_1970 -= 365 * seconds_in_day;
            (year)++;
        }
    }

    // 计算月份和日期
    month = 1;
    while (total_seconds_since_1970 >= days_in_month[month - 1] * seconds_in_day) {
        if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0)) {
            // 是闰年且是二月
            if (total_seconds_since_1970 >= 29 * seconds_in_day) {
                total_seconds_since_1970 -= 29 * seconds_in_day;
                (month)++;
            } else {
                break;
            }
        } else {
            // 非闰年或其他月份
            total_seconds_since_1970 -= days_in_month[month - 1] * seconds_in_day;
            (month)++;
        }
    }

    day = total_seconds_since_1970 / seconds_in_day + 1;
    total_seconds_since_1970 %= seconds_in_day;

    // 计算小时、分钟和秒
    hour = total_seconds_since_1970 / 3600;
    total_seconds_since_1970 %= 3600;
    minute = total_seconds_since_1970 / 60;
    second = total_seconds_since_1970 % 60;
}

std::string Week;
void Zeller()
{
  short W, c = year / 100, d = day, y, m;
  if(month == 1)y = year % 100 - 1, m = 13;
  else if(month == 2)y = year % 100 - 1, m = 14;
  else y = year % 100, m = month;
  int D = c/4 - 2*c + y + y/4 + (13*(m + 1)/5) + d - 1;
  W = D % 7;
  Week = "星期";
  switch(W)
  {
    case 1:
      Week += "一";
      break;
    case 2:
      Week += "二";
      break;
    case 3:
      Week += "三";
      break;
    case 4:
      Week += "四";
      break;
    case 5:
      Week += "五";
      break;
    case 6:
      Week += "六";
      break;
    case 0:
      Week += "日";
      break;
  }
}

void setRTCTimeFromNTP()
{
    /*timeClient.update();
    unsigned long epochTime = timeClient.getEpochTime();
    RtcDateTime ntpTime = RtcDateTime(epochTime);
    Rtc.SetDateTime(ntpTime);
    */
    timeClient.update();
    unsigned long days = timeClient.getEpochTime();

    // Convert epoch time to human-readable format
    days_to_date_and_time(days);
    RtcDateTime ntpTime(year, month, day, hour, minute, second);
    Rtc.SetDateTime(ntpTime);
    Serial.println("RTC time updated from NTP");
}

String JsonSerialization()
{
  DynamicJsonDocument doc(8192);
  String message;
  doc["timestamp"] = 1670987434289;
  doc["id"] = "21ac9d80-****-****-****-b3b116d640e2";
  doc["asset"]["id"] = "21ac9d80-****-****-****-b3b116d640e2";
  doc["asset"]["manufacturer"] = "esp32-wroom-32e";
  doc["location"]["timestamp"] = 1670987434289;
  //Change the id by your own
  for(short i = 0;i < apinfo.count;i++)
  {
    doc["location"]["wifis"][i]["macAddress"] = WiFi.BSSIDstr(i);
    doc["location"]["wifis"][i]["signalStrength"] = WiFi.RSSI(i);
  }
  serializeJson(doc, message);  // 序列化JSON数据并导出字符串
  return message;
}

String printDate(const RtcDateTime& dt)
{
    char datestring[26];
    snprintf_P(datestring, countof(datestring), PSTR("%02u/%02u/%04u"), dt.Day(), dt.Month(), dt.Year());
    return datestring;
}

String printTime(const RtcDateTime& dt)
{
    char datestring[26];
    snprintf_P(datestring, countof(datestring), PSTR("%02u:%02u:%02u"), dt.Hour(), dt.Minute(), dt.Second());
    return datestring;
}

short weather = -1;


void get_location(String postMessage)
{
  DynamicJsonDocument rep(8192);
  HTTPClient http;
  //http.begin("https://api.newayz.com/location/hub/v1/track_points?access_key="); 
  //You may add your own api
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Host", "api.newayz.com");
  http.addHeader("Connection", "keep-alive");
  short httpCode = http.POST(postMessage);
  String payload = http.getString();                                    
  Serial.println(httpCode);   
  PraseJson(payload); 
  http.end(); 
}
float longitude,latitude;

void PraseJson(String json)
{
  DynamicJsonDocument rep(8192);
  DeserializationError error = deserializeJson(rep, json);
  if (error) 
  {
    Serial.print(F("prase failed: "));
    Serial.println(error.f_str());
    return;
  }
  longitude = rep["location"]["position"]["point"]["longitude"];
  latitude = rep["location"]["position"]["point"]["latitude"];
  Serial.println(longitude);
  Serial.println(latitude);
}
float tempre,tempre_max,tempre_min,preci_sum;
void get_weather()
{
  DynamicJsonDocument rep(8192);
  HTTPClient http;
  std::string s = "https://api.open-meteo.com/v1/forecast?latitude=" + std::to_string(latitude) + "&longitude=" + std::to_string(longitude) + "&daily=temperature_2m_max,temperature_2m_min,precipitation_sum&current_weather=true&timezone=Asia%2FSingapore&forecast_days=1";
  http.begin(s.c_str()); 
  Serial.println(F(s.c_str()));     
  short httpCode = http.GET();
  String payload = http.getString(); 
  Serial.println(payload);
  Serial.println("httpcode");                                   
  Serial.println(httpCode);   
  DeserializationError error = deserializeJson(rep, payload);
  if (error) 
  {
    Serial.print(F("prase failed: "));
    Serial.println(error.f_str());
    return;
  } 
  tempre = rep["current_weather"]["temperature"];
  tempre_max = rep["daily"]["temperature_2m_max"][0];
  tempre_min = rep["daily"]["temperature_2m_min"][0];
  preci_sum = rep["daily"]["precipitation_sum"][0];
  Serial.println(tempre_max);
  http.end(); 
}
std::string path,day_wea,night,tom_d,tom_n;
void get_location_city()
{
  DynamicJsonDocument rep(8192);
  HTTPClient http;
  std::string s = "https://api.seniverse.com/v3/weather/daily.json?key=&location=" + std::to_string(latitude) + ":" + std::to_string(longitude) + "&language=zh-Hans";
  //Change the key by your own
  http.begin(s.c_str()); 
  Serial.println(F(s.c_str()));     
  short httpCode = http.GET();
  String payload = http.getString(); 
  Serial.println("httpcode");                                   
  Serial.println(httpCode); 
  Serial.println(payload);
  DeserializationError error = deserializeJson(rep, payload);
  if (error) 
  {
    Serial.print(F("prase failed: "));
    Serial.println(error.f_str());
    return;
  } 
  const char* p = rep["results"][0]["location"]["path"];
  const char* d = rep["results"][0]["daily"][0]["text_day"];
  const char* n = rep["results"][0]["daily"][0]["text_night"];
  const char* t_d = rep["results"][0]["daily"][1]["text_day"];
  const char* t_n = rep["results"][0]["daily"][1]["text_night"];
  Serial.println(p);
  path = p, day_wea = d, night = n, tom_d = t_d, tom_n = t_n;
  http.end(); 
}



void setup()
{
    // Initialize serial for debugging
    Serial.begin(921600);

    // Initialize RTC
    Rtc.Begin();

    // Initialize OLED display
    u8g2.begin();


    u8g2.setFont(u8g2_font_inb21_mf);
    width = u8g2.getUTF8Width(text);

    u8g2_uint_t x;
    for (short counter = 121; counter > 0; counter--)
    {
        u8g2.firstPage();
      do
      {
        u8g2.setFont(u8g2_font_timB10_tr);
        u8g2.drawStr(10,60,"40TH Anniversary");

        x = offset;
        u8g2.setFont(u8g2_font_inb21_mf);
        do
        {
          u8g2.drawUTF8(x,30,text);
          x += width;
        }while(x < u8g2.getDisplayWidth());
      }while(u8g2.nextPage());
      offset -= 1;
      if ((u8g2_uint_t)offset < (u8g2_uint_t)-width)
      {
          offset = 0;
      }
      delay(1);
    }
     // Connect to WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    short cnt = 0;
    while (WiFi.status() != WL_CONNECTED && cnt <= 20) {
        delay(500);
        Serial.println("Connecting to WiFi...");
        cnt++;
    }
    if(WiFi.status() == WL_CONNECTED)
    {
      Serial.println("Connected to WiFi");
        // Initialize NTP client
      timeClient.begin();
      timeClient.update();
      setRTCTimeFromNTP();
      Zeller();
      wifi_scan();
      get_location(JsonSerialization());
      get_weather();
      get_location_city();
    }
    

}
std::string s,rain;
short t = 0;
void loop()
{
  weather++;
  if(weather > 80)weather = -1;
  bool flag = false;
    if(WiFi.status() == WL_CONNECTED)
    {
      setRTCTimeFromNTP();
      if(weather == 0)
      {
        s = "Now:" + std::to_string(tempre).substr(0, std::to_string(tempre).find(".") + 2) + "°C";
        rain = path;
      }
      else if(weather == 20)
      {
        s = "Max:" + std::to_string(tempre_max).substr(0, std::to_string(tempre_max).find(".") + 2) + "°C";
        rain = "日间：" + day_wea;
      }
      else if(weather == 40)
      {
        s = "Min:" + std::to_string(tempre_min).substr(0, std::to_string(tempre_min).find(".") + 2) + "°C";
        rain = "晚间：" + night;
      }
      else if(weather == 60)
      {
        s = "Rain:" + std::to_string(preci_sum).substr(0, std::to_string(preci_sum).find(".") + 2) + "mm";
        if(tom_d == tom_n)rain = "明天气：" + tom_d;
        else rain = "明天气：" + tom_d + "转" + tom_n;
      }
      Serial.println(weather);
      Serial.println(s.c_str());
      flag = true;
    }
    RtcDateTime now = Rtc.GetDateTime();
    u8g2.firstPage();
    while (u8g2.nextPage())
    {
      u8g2.drawHLine(0, 10, 128);
      u8g2.drawHLine(0, 40, 128);
      u8g2.setFont(u8g2_font_5x8_tn);
      if(flag)
      {
        u8g2.drawStr(5, 64, printDate(now).c_str());
      }
      else 
      {
        u8g2.setFont(u8g2_font_timB10_tr);
        u8g2.drawStr(30, 64, printDate(now).c_str());
      }
      u8g2.setFont(u8g2_font_timB24_tr);
      u8g2.drawStr(5, 37, printTime(now).c_str());
      if(flag)
      {
        u8g2.setFont(u8g2_font_t0_11_tf);
        u8g2.drawUTF8(63 , 64 , s.c_str());
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2.drawUTF8(1 , 52 , rain.c_str());
        if(weather >= 20 && (weather < 60 || tom_d == tom_n))u8g2.drawUTF8(80, 52, Week.c_str());
      }
    }
    if(t == 3600)
    {
      if(flag)
      {
        wifi_scan();
        get_location(JsonSerialization());
        get_weather();
        get_location_city();
        Zeller();
      }
      t = -1;
    }
    t++;
    delay(1);
}
