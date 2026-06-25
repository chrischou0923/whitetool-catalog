/* =============================================================================
 * 作業練習1：ESP32 取得時間(每秒1次) 與 天氣資料(每10秒1次)，顯示在串列埠監看視窗
 * -----------------------------------------------------------------------------
 *   - 時間：透過 NTP 校時，每 1 秒印出一次目前日期時間
 *   - 天氣：透過 OpenWeatherMap API，每 10 秒取得一次天氣資料並印出
 *   - 使用 millis() 計時，避免 delay() 卡住程式，時間與天氣可各自獨立更新
 * 接線：只需要 ESP32 + WiFi，不需要外接元件
 * =============================================================================
 */

//---------------------------------------------------------------
#include <WiFi.h>
const char *ssid     = "ZZJ";          // 連上無線基地臺的 SSID
const char *password = "0981510923";   // 連上無線基地臺的密碼
//---------------------------------------------------------------
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
  Serial.print("SSID: ");        Serial.println(WiFi.SSID());        // 印出 SSID
  Serial.print("IP: ");          Serial.println(WiFi.localIP());     // 印出 IP
  Serial.print("Subnet Mask: "); Serial.println(WiFi.subnetMask());  // 印出子網路遮罩
  Serial.print("Gateway IP: ");  Serial.println(WiFi.gatewayIP());   // 印出閘道 IP
  Serial.print("DNS IP: ");      Serial.println(WiFi.dnsIP());       // 印出 DNS IP
}

//---------------------------------------------------------------
// NTP 校時設定（臺灣時區 UTC+8）
//---------------------------------------------------------------
#include <time.h>
const char *ntpServer      = "pool.ntp.org";  // NTP 伺服器
const long  gmtOffset_sec  = 8 * 3600;        // 臺灣時區 UTC+8 = 8 小時 = 28800 秒
const int   daylightOffset = 0;               // 臺灣沒有日光節約時間

void print_local_time()
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))   // 取得目前本地時間
  {
    Serial.println("Failed to obtain time");
    return;
  }
  // 格式化輸出：2026/06/25 (Thu) 14:30:05
  char buf[64];
  strftime(buf, sizeof(buf), "%Y/%m/%d (%a) %H:%M:%S", &timeinfo);
  Serial.print("Time: ");
  Serial.println(buf);
}

//---------------------------------------------------------------
#include <HTTPClient.h>          // 發送 http 請求，取得網站資料
HTTPClient http;
// 將城市、地區、API 金鑰設定為變數，方便後續操控
String city        = "Taipei";
String countryCode = "TW";
String ApiKey      = "e65ae8efe382bbf39fc62bbdcccf6e2d";
String url = "https://api.openweathermap.org/data/2.5/weather?q=" + city + "," +
             countryCode + "&units=metric&appid=" + ApiKey;
//---------------------------------------------------------------
#include <ArduinoJson.h>         // 解析 JSON 資料
String weatherDescription;       // 天氣概況
String temp;                     // 溫度
String pressure;                 // 大氣壓力
String humidity;                 // 溼度

void get_weather_data()
{
  if (WiFi.status() != WL_CONNECTED)   // 若斷線則重新連線
  {
    connect_to_wifi();
  }
  http.begin(url);                     // 開始連接網頁
  int httpCode = http.GET();           // 執行 GET 請求，回傳碼儲存於 httpCode
  if (httpCode == HTTP_CODE_OK)        // 如果連線正常
  {
    String payload = http.getString(); // 傳回的網頁內容儲存於字串變數 payload

    // -------- OpenWeatherMap JSON 格式解析 --------
    DynamicJsonDocument WeatherJson(payload.length() * 2);  // 宣告 JSON 文件
    deserializeJson(WeatherJson, payload);                  // 解析 payload
    weatherDescription = WeatherJson["weather"][0]["description"].as<String>(); // 天氣概況
    temp     = WeatherJson["main"]["temp"].as<String>();      // 溫度
    pressure = WeatherJson["main"]["pressure"].as<String>();  // 氣壓
    humidity = WeatherJson["main"]["humidity"].as<String>();  // 溼度

    Serial.println("----------------------------------");
    Serial.print("City: ");                Serial.println(city);
    Serial.print("Weather description: "); Serial.println(weatherDescription);
    Serial.print("Temp: ");      Serial.print(temp);      Serial.println(" °C");
    Serial.print("Pressure: ");  Serial.print(pressure);  Serial.println(" hPa");
    Serial.print("Humidity: ");  Serial.print(humidity);  Serial.println(" %");
    Serial.println("----------------------------------");
  }
  else
  {
    Serial.print("HTTP GET failed, error code: ");
    Serial.println(httpCode);
  }
  http.end();                          // 結束連線
}

//---------------------------------------------------------------
// 利用 millis() 做非阻塞計時
//---------------------------------------------------------------
unsigned long lastTimeMillis    = 0;   // 上次印出時間的時間戳
unsigned long lastWeatherMillis = 0;   // 上次取得天氣的時間戳
const unsigned long TIME_INTERVAL    = 1000;    // 時間每 1 秒更新
const unsigned long WEATHER_INTERVAL = 10000;   // 天氣每 10 秒更新

void setup()
{
  Serial.begin(9600);                  // 啟用串列埠監看視窗
  connect_to_wifi();                   // 連線到 WiFi
  configTime(gmtOffset_sec, daylightOffset, ntpServer);  // 啟動 NTP 校時
  get_weather_data();                  // 開機先抓一次天氣
}

void loop()
{
  unsigned long now = millis();

  // 每 1 秒印出一次時間
  if (now - lastTimeMillis >= TIME_INTERVAL)
  {
    lastTimeMillis = now;
    print_local_time();
  }

  // 每 10 秒取得一次天氣
  if (now - lastWeatherMillis >= WEATHER_INTERVAL)
  {
    lastWeatherMillis = now;
    get_weather_data();
  }
}
