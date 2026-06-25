/* =============================================================================
 * 作業練習2：ESP32 + WiFi + NTP + OpenWeatherMap + 紅外線遙控器 + LCD
 * -----------------------------------------------------------------------------
 * 功能說明：
 *   - 連上 WiFi，透過 NTP 取得臺灣時間，透過 OpenWeatherMap 取得天氣資料
 *   - 用紅外線遙控器選擇 10 個臺灣城市，將時間 / 天氣顯示在 I2C LCD 上
 *
 * 基本功能：
 *   - 自訂 10 個城市 (索引 0~9)
 *   - 數字鍵 0~9       ：切換到該城市，自動輪播「時間 + 天氣」訊息(3 頁交替)
 *   - PREV / NEXT 鍵   ：切換前一個 / 下一個城市，在 0~9 之間循環
 *   - VOL+ 鍵          ：只顯示「天氣訊息」(兩頁交替)
 *   - VOL- 鍵          ：只顯示「時間訊息」
 *   - EQ 鍵            ：顯示當下日期時間，時間每秒更新一次
 *   天氣訊息一個畫面顯示不下時，會自動以兩個頁面交替顯示
 *
 * 進階功能：
 *   - 依不同天氣，在 RGB LED 上顯示不同顏色 (晴/雲/雨/雪/雷...)
 *
 * 接線（對應 innovati AMA-EB-04 教學板）：
 *   - 紅外線接收器(左上 R Remote Receiver)：JP14 訊號 S -> IO15、+ -> 3V3、- -> GND
 *   - I2C LCD 1602                       ：SDA -> IO21、SCL -> IO22、電源 JP23 -> 3V3 / GND
 *   - RGB LED(左側 JP17，B G R，選做)     ：R -> IO25、G -> IO26、B -> IO27
 *   （RGB LED 已在板上，不需另外串電阻）
 *
 * 需安裝的函式庫：
 *   - WiFi (內建)、HTTPClient (內建)、time/sntp (內建)
 *   - ArduinoJson (作者 Benoit Blanchon)
 *   - IRremote    (作者 Armin Joachimsmeyer，v3 以上)
 *   - LiquidCrystal_I2C
 * =============================================================================
 */

//=============================================================================
// 1) WiFi 設定
//=============================================================================
#include <WiFi.h>
const char *ssid     = "ZZJ";          // 連上無線基地臺的 SSID
const char *password = "0981510923";   // 連上無線基地臺的密碼

void connect_to_wifi()
{
  WiFi.begin(ssid, password);            // 啟動 WiFi 連線
  Serial.printf("Connecting to %s ", ssid);
  while (WiFi.status() != WL_CONNECTED)  // 只要 WiFi 連線狀態不正常
  {
    delay(500);                          // 每 0.5 秒印出一個點
    Serial.print(".");
  }
  Serial.println(" CONNECTED!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());        // 印出 IP
}

//=============================================================================
// 2) NTP 校時設定（臺灣 GMT+8）
//=============================================================================
#include "time.h"
#include "sntp.h"
const char *ntpServer1 = "pool.ntp.org";        // NTP 伺服器網址 1
const char *ntpServer2 = "time.nist.gov";       // NTP 伺服器網址 2
const char *ntpServer3 = "time.stdtime.gov.tw"; // NTP 伺服器網址 3
const long  gmtOffset_sec      = 28800;         // GMT+8，28800 秒 = 8 小時
const int   daylightOffset_sec = 0;             // 臺灣無日光節約時間

//=============================================================================
// 3) 城市清單（10 個臺灣城市，索引 0~9）
//    名稱對應 ISO 3166 臺灣城市英文名，可向 OpenWeatherMap 查詢
//=============================================================================
const int CITY_COUNT = 10;
struct City {
  const char *displayName;   // LCD 上顯示的名稱
  const char *queryName;     // 向 API 查詢用的英文名
};
City cities[CITY_COUNT] = {
  {"Taipei",    "Taipei"},      // 0 臺北
  {"NewTaipei", "New Taipei"},  // 1 新北
  {"Taoyuan",   "Taoyuan"},     // 2 桃園
  {"Hsinchu",   "Hsinchu"},     // 3 新竹
  {"Taichung",  "Taichung"},    // 4 臺中
  {"Changhua",  "Changhua"},    // 5 彰化
  {"Chiayi",    "Chiayi"},      // 6 嘉義
  {"Tainan",    "Tainan"},      // 7 臺南
  {"Kaohsiung", "Kaohsiung"},   // 8 高雄
  {"Hualien",   "Hualien"}      // 9 花蓮
};
int currentCity = 0;            // 目前選擇的城市索引（預設臺北）

//=============================================================================
// 4) OpenWeatherMap API 設定與天氣資料
//=============================================================================
#include <HTTPClient.h>
#include <ArduinoJson.h>
HTTPClient http;
String ApiKey = "e65ae8efe382bbf39fc62bbdcccf6e2d";

// 解析後的天氣資料
String weatherMain;          // 天氣主分類 (Clouds / Rain ...)，用來決定 RGB 顏色
String weatherDescription;   // 天氣概況
String temp;                 // 溫度
String pressure;             // 大氣壓力
String humidity;             // 溼度
bool   weatherValid = false; // 此次抓取是否成功

void set_rgb_by_weather(String main);   // 函式原型宣告（實作在後面第6區）

void get_weather_data(int cityIndex)
{
  if (WiFi.status() != WL_CONNECTED)   // 若斷線則重新連線
    connect_to_wifi();

  // 城市英文名可能含空白，需做 URL 編碼，這裡簡單把空白換成 %20
  String q = String(cities[cityIndex].queryName);
  q.replace(" ", "%20");
  String url = "https://api.openweathermap.org/data/2.5/weather?q=" + q +
               ",TW&units=metric&appid=" + ApiKey;

  http.begin(url);                     // 開始連接網頁
  int httpCode = http.GET();           // 執行 GET 請求
  if (httpCode == HTTP_CODE_OK)        // 連線正常
  {
    String payload = http.getString(); // 取得回傳的 JSON 內容

    // -------- OpenWeatherMap JSON 格式解析 --------
    DynamicJsonDocument WeatherJson(payload.length() * 2);
    deserializeJson(WeatherJson, payload);
    weatherMain        = WeatherJson["weather"][0]["main"].as<String>();        // 天氣主分類
    weatherDescription = WeatherJson["weather"][0]["description"].as<String>(); // 天氣概況
    temp     = WeatherJson["main"]["temp"].as<String>();      // 溫度
    pressure = WeatherJson["main"]["pressure"].as<String>();  // 氣壓
    humidity = WeatherJson["main"]["humidity"].as<String>();  // 溼度
    weatherValid = true;

    Serial.printf("[%s] %s, %s C, %s %%, %s hPa\n",
                  cities[cityIndex].displayName, weatherDescription.c_str(),
                  temp.c_str(), humidity.c_str(), pressure.c_str());
    set_rgb_by_weather(weatherMain);   // 依天氣設定 RGB LED 顏色（進階功能）
  }
  else
  {
    weatherValid = false;
    Serial.print("HTTP GET failed, error code: ");
    Serial.println(httpCode);
  }
  http.end();                          // 結束連線
}

//=============================================================================
// 5) I2C LCD 設定 (1602)
//=============================================================================
#include <Wire.h>                      // I2C 通訊（指定 SDA/SCL 腳位用）
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);    // I2C 位址常見為 0x27 或 0x3F，依模組調整

// LCD 顯示一行（自動補空白清掉殘字，避免畫面殘留舊內容）
void lcd_line(int row, String text)
{
  while (text.length() < 16) text += " ";  // 補滿 16 字
  if (text.length() > 16) text = text.substring(0, 16);
  lcd.setCursor(0, row);
  lcd.print(text);
}

//=============================================================================
// 6) RGB LED 設定（進階功能：依天氣顯示不同顏色）
//    使用共陰極 RGB LED，HIGH 為亮
//=============================================================================
const int PIN_R = 25;
const int PIN_G = 26;
const int PIN_B = 27;

void set_rgb(bool r, bool g, bool b)
{
  digitalWrite(PIN_R, r);
  digitalWrite(PIN_G, g);
  digitalWrite(PIN_B, b);
}

void set_rgb_by_weather(String main)
{
  // 依 OpenWeatherMap 的 weather.main 主分類決定顏色
  if (main == "Clear")            set_rgb(1, 1, 0);   // 晴：黃色
  else if (main == "Clouds")      set_rgb(0, 0, 1);   // 雲：藍色
  else if (main == "Rain" ||
           main == "Drizzle")     set_rgb(0, 0, 1);   // 雨：藍色
  else if (main == "Thunderstorm")set_rgb(1, 0, 1);   // 雷雨：紫色
  else if (main == "Snow")        set_rgb(1, 1, 1);   // 雪：白色
  else if (main == "Mist"  ||
           main == "Fog"   ||
           main == "Haze")        set_rgb(0, 1, 1);   // 霧霾：青色
  else                            set_rgb(0, 1, 0);   // 其他：綠色
}

//=============================================================================
// 7) 紅外線遙控器設定（NEC 常見「車用 MP3」遙控器）
//=============================================================================
#include <IRremote.hpp>
const int IR_RECEIVE_PIN = 15;          // 紅外線接收器接腳

// 常見 NEC 遙控器的按鍵指令碼 (IrReceiver.decodedIRData.command)
#define KEY_0     0x16
#define KEY_1     0x0C
#define KEY_2     0x18
#define KEY_3     0x5E
#define KEY_4     0x08
#define KEY_5     0x1C
#define KEY_6     0x5A
#define KEY_7     0x42
#define KEY_8     0x52
#define KEY_9     0x4A
#define KEY_PREV  0x44              // |<<
#define KEY_NEXT  0x40              // >>|
#define KEY_VOL_DOWN 0x07           // VOL-
#define KEY_VOL_UP   0x15           // VOL+
#define KEY_EQ    0x09              // EQ

//=============================================================================
// 8) 顯示模式
//=============================================================================
// MODE_AUTO    ：數字鍵 0~9 / PREV / NEXT 選城市後，自動輪播「時間 + 天氣」多頁
// MODE_WEATHER ：VOL+ 只看天氣（兩頁交替）
// MODE_TIME    ：VOL- 只看時間
// MODE_DATETIME：EQ 顯示當下日期時間（每秒更新）
enum DisplayMode { MODE_AUTO, MODE_WEATHER, MODE_TIME, MODE_DATETIME };
DisplayMode mode = MODE_AUTO;           // 預設自動輪播時間+天氣

// 取得本地時間字串
bool get_time_strings(String &dateStr, String &timeStr)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return false;
  char d[20], t[20];
  strftime(d, sizeof(d), "%Y/%m/%d %a", &timeinfo);  // 2026/06/25 Thu
  strftime(t, sizeof(t), "%H:%M:%S", &timeinfo);     // 14:30:05
  dateStr = String(d);
  timeStr = String(t);
  return true;
}

//=============================================================================
// 9) 畫面更新
//=============================================================================
int  weatherPage = 0;                   // 天氣訊息的頁面 (0/1 交替)
int  autoPage    = 0;                   // 自動輪播頁面 (0=時間, 1=天氣1, 2=天氣2)
const int AUTO_PAGE_COUNT = 3;          // 自動輪播共 3 頁

// 自動輪播：把「時間」與「天氣」訊息分成 3 頁交替顯示（規格要求）
void show_auto()
{
  String name = String(cities[currentCity].displayName);
  if (autoPage == 0)                    // 第 1 頁：城市 + 時間（每秒更新）
  {
    String d, t;
    lcd_line(0, name);
    if (get_time_strings(d, t)) lcd_line(1, t);
    else                        lcd_line(1, "no time yet");
  }
  else if (!weatherValid)               // 天氣還沒抓到
  {
    lcd_line(0, name);
    lcd_line(1, "loading...");
  }
  else if (autoPage == 1)               // 第 2 頁：天氣分類 + 溫度 + 溼度
  {
    lcd_line(0, name + " " + weatherMain);
    lcd_line(1, "T:" + temp + "C H:" + humidity + "%");
  }
  else                                  // 第 3 頁：天氣概況 + 大氣壓力
  {
    lcd_line(0, weatherDescription);
    lcd_line(1, "P:" + pressure + "hPa");
  }
}

void show_weather()
{
  String name = String(cities[currentCity].displayName);
  if (!weatherValid)
  {
    lcd_line(0, name);
    lcd_line(1, "loading...");
    return;
  }
  // 天氣資料一個畫面放不下，分成兩頁交替顯示
  if (weatherPage == 0)
  {
    lcd_line(0, name + " " + weatherMain);        // 城市 + 天氣分類
    lcd_line(1, "T:" + temp + "C H:" + humidity + "%"); // 溫度 + 溼度
  }
  else
  {
    lcd_line(0, weatherDescription);              // 詳細天氣概況
    lcd_line(1, "P:" + pressure + "hPa");         // 大氣壓力
  }
}

void show_time()
{
  String d, t;
  lcd_line(0, String(cities[currentCity].displayName) + " time");
  if (get_time_strings(d, t)) lcd_line(1, t);
  else                        lcd_line(1, "no time yet");
}

void show_datetime()
{
  String d, t;
  if (get_time_strings(d, t))
  {
    lcd_line(0, d);   // 日期
    lcd_line(1, t);   // 時間（每秒更新）
  }
  else
  {
    lcd_line(0, "Getting time");
    lcd_line(1, "...");
  }
}

void update_display()
{
  switch (mode)
  {
    case MODE_AUTO:     show_auto();     break;
    case MODE_WEATHER:  show_weather();  break;
    case MODE_TIME:     show_time();     break;
    case MODE_DATETIME: show_datetime(); break;
  }
}

//=============================================================================
// 10) 處理遙控器按鍵
//=============================================================================
void handle_key(uint8_t cmd)
{
  switch (cmd)
  {
    // 數字鍵 0~9：切換到對應城市，並自動輪播「時間 + 天氣」訊息
    case KEY_0: currentCity = 0; goto pickCity;
    case KEY_1: currentCity = 1; goto pickCity;
    case KEY_2: currentCity = 2; goto pickCity;
    case KEY_3: currentCity = 3; goto pickCity;
    case KEY_4: currentCity = 4; goto pickCity;
    case KEY_5: currentCity = 5; goto pickCity;
    case KEY_6: currentCity = 6; goto pickCity;
    case KEY_7: currentCity = 7; goto pickCity;
    case KEY_8: currentCity = 8; goto pickCity;
    case KEY_9: currentCity = 9; goto pickCity;
    pickCity:
      mode = MODE_AUTO; autoPage = 0;    // 進入自動輪播，從時間頁開始
      Serial.printf("Select city %d: %s\n", currentCity, cities[currentCity].displayName);
      get_weather_data(currentCity);     // 抓取新城市的天氣
      update_display();
      break;

    // NEXT：下一個城市（0~9 循環），同樣自動輪播時間+天氣
    case KEY_NEXT:
      currentCity = (currentCity + 1) % CITY_COUNT;
      mode = MODE_AUTO; autoPage = 0;
      get_weather_data(currentCity);
      update_display();
      break;

    // PREV：上一個城市（0~9 循環）
    case KEY_PREV:
      currentCity = (currentCity - 1 + CITY_COUNT) % CITY_COUNT;
      mode = MODE_AUTO; autoPage = 0;
      get_weather_data(currentCity);
      update_display();
      break;

    // VOL+：只看「天氣」訊息（兩頁交替）
    case KEY_VOL_UP:
      mode = MODE_WEATHER; weatherPage = 0;
      update_display();
      break;

    // VOL-：只看「時間」訊息
    case KEY_VOL_DOWN:
      mode = MODE_TIME;
      update_display();
      break;

    // EQ：顯示當下日期時間（每秒更新）
    case KEY_EQ:
      mode = MODE_DATETIME;
      update_display();
      break;

    default:
      // 其他按鍵不處理
      break;
  }
}

//=============================================================================
// 11) setup / loop
//=============================================================================
unsigned long lastTickMillis    = 0;    // 每秒更新時間/交替天氣頁
unsigned long lastWeatherMillis = 0;    // 每 10 分鐘自動刷新天氣
const unsigned long WEATHER_REFRESH = 600000; // 600000ms = 10 分鐘

void setup()
{
  Serial.begin(9600);

  // RGB LED 腳位
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);
  set_rgb(0, 0, 0);

  // LCD 初始化（AMA-EB-04：I2C SDA=IO21、SCL=IO22）
  Wire.begin(21, 22);    // 指定 I2C 腳位，與板上接線一致
  lcd.init();
  lcd.backlight();
  lcd_line(0, "ESP32 Weather");
  lcd_line(1, "Connecting...");

  // 紅外線接收器初始化
  IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);

  // WiFi 連線
  connect_to_wifi();

  // NTP 校時
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer1, ntpServer2, ntpServer3);

  // 開機先抓一次預設城市（臺北）的天氣
  get_weather_data(currentCity);
  update_display();
}

void loop()
{
  // (1) 處理紅外線遙控器按鍵
  if (IrReceiver.decode())
  {
    uint8_t cmd = IrReceiver.decodedIRData.command;
    // 過濾 NEC 長按重複碼 (repeat)，0 通常為雜訊
    if (cmd != 0 && !(IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT))
      handle_key(cmd);
    IrReceiver.resume();   // 準備接收下一個訊號
  }

  unsigned long now = millis();

  // (2) 每 1 秒更新畫面（時間才會每秒跳動）；每 3 秒換一頁（方便閱讀）
  static int tickCount = 0;
  if (now - lastTickMillis >= 1000)
  {
    lastTickMillis = now;
    tickCount++;
    if (tickCount >= 3)                       // 每 3 秒換頁
    {
      tickCount = 0;
      if (mode == MODE_AUTO)
        autoPage = (autoPage + 1) % AUTO_PAGE_COUNT;   // 時間+天氣 3 頁輪播
      else if (mode == MODE_WEATHER)
        weatherPage = (weatherPage + 1) % 2;           // 天氣兩頁交替
    }
    update_display();                          // 重新整理畫面（時間每秒更新）
  }

  // (3) 每 10 分鐘自動刷新一次目前城市的天氣
  if (now - lastWeatherMillis >= WEATHER_REFRESH)
  {
    lastWeatherMillis = now;
    get_weather_data(currentCity);
  }
}
