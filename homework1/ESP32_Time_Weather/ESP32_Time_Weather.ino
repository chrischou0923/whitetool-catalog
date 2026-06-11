// =====================================================================
// 作業練習1：ESP32取得時間(每秒1次)、天氣資料(每10秒1次)，
//            將結果顯示在監看視窗上。
// 使用函式庫：WiFi、HTTPClient、ArduinoJson、time.h(NTP網路對時)
// =====================================================================

//---------------------------------------------------------------
#include <WiFi.h>
const char *ssid = "SSID";              // 連上無線基地臺的SSID
const char *password = "密碼";          // 連上無線基地臺的密碼
//---------------------------------------------------------------
void connect_to_wifi()
{
  WiFi.begin(ssid, password);           // 啟動WiFi連線
  Serial.printf("Connecting to %s ", ssid);
  while(WiFi.status() != WL_CONNECTED)  // 只要WiFi連線狀態不正常
  {
    delay(500);                         // 每0.5秒印出一個點
    Serial.print(".");
  }
  Serial.println(" CONNECTED!");
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());          // 印出SSID
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());       // 印出IP
  Serial.print("Subnet Mask IP: ");
  Serial.println(WiFi.subnetMask());    // 印出子網路遮罩
  Serial.print("Gateway IP: ");
  Serial.println(WiFi.gatewayIP());     // 印出閘道IP
  Serial.print("DNS IP: ");
  Serial.println(WiFi.dnsIP());         // 印出DNS IP
}
//---------------------------------------------------------------
// NTP網路對時設定
//---------------------------------------------------------------
#include <time.h>
const char *ntpServer = "pool.ntp.org"; // NTP伺服器
const long gmtOffset_sec = 8 * 3600;    // 臺灣時區GMT+8，偏移量(秒)
const int daylightOffset_sec = 0;       // 不使用日光節約時間

void print_local_time()
{
  struct tm timeinfo;                   // 宣告時間結構變數
  if (!getLocalTime(&timeinfo))         // 取得目前時間
  {
    Serial.println("Failed to obtain time!");
    return;
  }
  // 印出格式：2026/06/11 (Thursday) 12:34:56
  Serial.println(&timeinfo, "%Y/%m/%d (%A) %H:%M:%S");
}
//---------------------------------------------------------------
#include <HTTPClient.h>                 // 發送http請求，取得網站資料
HTTPClient http;

// 將網址中的城市、地區、API金鑰設定為變數，以方便後續程式操控
String city = "Taipei";
String countryCode = "TW";
String ApiKey = "API金鑰";
String url = "https://api.openweathermap.org/data/2.5/weather?q=" + city + "," + countryCode + "&units=metric&appid=" + ApiKey; // 網址
//---------------------------------------------------------------
#include <ArduinoJson.h>                // 解析JSON資料
String weatherDescription;              // 天氣概況
String temp;                            // 溫度
String pressure;                        // 大氣壓力
String humidity;                        // 溼度
//---------------------------------------------------------------
void get_weather_data()
{
  http.begin(url);                      // 開始連接網頁
  int httpCode = http.GET();            // 執行GET請求，回傳碼儲存於httpCode

  if (httpCode == HTTP_CODE_OK)         // 如果連線正常
  {
    String payload = http.getString();  // 傳回的網頁內容儲存於字串變數payload(承載量)

    // -------------------------------------------------------------
    // OpenWeatherMap JSON格式解析
    // -------------------------------------------------------------
    DynamicJsonDocument WeatherJson(payload.length() * 2);    // 宣告一個Json文件，名稱為WeatherJson（陣列格式）
    deserializeJson(WeatherJson, payload);                    // 解析payload為JSON Array格式

    weatherDescription = WeatherJson["weather"][0]["description"].as<String>();   // 取得天氣概況，weather是陣列，[0]是索引值
    temp = WeatherJson["main"]["temp"].as<String>();          // 取得溫度
    pressure = WeatherJson["main"]["pressure"].as<String>();  // 取得氣壓
    humidity = WeatherJson["main"]["humidity"].as<String>();  // 取得溼度
    Serial.println("----------------------------------");
    Serial.print("Weather description: ");
    Serial.println(weatherDescription);
    Serial.print("Temp: ");
    Serial.print(temp);
    Serial.println(" °C");
    Serial.print("Pressure: ");
    Serial.print(pressure);
    Serial.println(" hPa");
    Serial.print("Humidity: ");
    Serial.print(humidity);
    Serial.println(" %");
    Serial.println("----------------------------------");
  }
  else
  {
    Serial.print("HTTP GET failed, error code: ");      // 印出錯誤訊息
    Serial.println(httpCode);
  }

  http.end();                           // 結束連線
}
//---------------------------------------------------------------
// 非阻塞計時變數：用millis()控制兩個工作的執行週期，
// 避免使用delay()造成互相干擾
//---------------------------------------------------------------
unsigned long timePrevious = 0;             // 上次顯示時間的時刻
unsigned long weatherPrevious = 0;          // 上次取得天氣的時刻
const unsigned long timeInterval = 1000;    // 時間顯示週期：1秒
const unsigned long weatherInterval = 10000;// 天氣更新週期：10秒

void setup()
{
  //-------------------------------------------
  Serial.begin(9600);     // 啟用串列埠監看視窗
  //-------------------------------------------
  connect_to_wifi();      // 連線到WiFi
  //-------------------------------------------
  // 設定NTP網路對時（時區偏移、日光節約、NTP伺服器）
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  //-------------------------------------------
  get_weather_data();     // 開機先取得一次天氣資料
  //-------------------------------------------
}

void loop()
{
  unsigned long now = millis();             // 取得開機至今的毫秒數

  //-------------------------------------------
  // 每1秒顯示一次時間
  //-------------------------------------------
  if (now - timePrevious >= timeInterval)
  {
    timePrevious = now;
    print_local_time();
  }

  //-------------------------------------------
  // 每10秒取得一次天氣資料
  //-------------------------------------------
  if (now - weatherPrevious >= weatherInterval)
  {
    weatherPrevious = now;
    get_weather_data();
  }
}
