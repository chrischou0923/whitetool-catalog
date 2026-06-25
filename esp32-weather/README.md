# ESP32 工作站模式 — OpenWeatherMap 天氣資料練習

ESP32 透過 WiFi 工作站(STA)模式連網，取得 NTP 時間與 OpenWeatherMap 天氣資料。

## 目錄

| 資料夾 | 對應作業 | 內容 |
|--------|----------|------|
| `exercise1_time_weather/` | 作業練習1 | 串列埠顯示：時間每秒 1 次、天氣每 10 秒 1 次 |
| `exercise2_ir_lcd_weather/` | 作業練習2 | 紅外線遙控器選城市，LCD 顯示時間/天氣，RGB LED 依天氣變色 |

> 已將 WiFi（SSID `ZZJ`）與 OpenWeatherMap API 金鑰直接寫進程式碼，可直接燒錄測試。
> 公開分享前請記得移除金鑰與密碼。

## 需要的函式庫（Arduino 程式庫管理員安裝）

- **ArduinoJson** (Benoit Blanchon) — 解析 JSON
- **IRremote** (Armin Joachimsmeyer, v3 以上) — 紅外線解碼（僅練習2）
- **LiquidCrystal_I2C** — I2C LCD 1602（僅練習2）
- WiFi / HTTPClient / time / sntp 為 ESP32 內建

## 作業練習1 接線
只要 ESP32 + WiFi，不需外接元件，結果輸出在串列埠監看視窗（鮑率 9600）。

## 作業練習2 接線（對應 innovati AMA-EB-04 教學板）

板上模組已焊好，用杜邦線把各模組的 JP 針腳連到 ESP32 的 GPIO 即可：

| 模組（板上位置）| 板上針腳 | 接到 ESP32 |
|------|---------|-----------|
| 紅外線接收器（左上 R Remote Receiver）| JP14 訊號 `S` | `IO15` |
| 　└ 電源 | `+` / `-` | `3V3` / `GND` |
| I2C LCD 1602 | `SDA` / `SCL` | `IO21` / `IO22` |
| 　└ 電源（JP23）| `3V3` / `GND` | `3V3` / `GND` |
| RGB LED（左側 JP17，`B G R`，選做）| R / G / B | `IO25` / `IO26` / `IO27` |

> LCD 為 I2C 介面，程式用 `LiquidCrystal_I2C`、`Wire.begin(21, 22)`。
> 若改用並列(16 腳)LCD，需改用 `LiquidCrystal` 函式庫。

### 遙控器按鍵功能（常見 NEC「車用 MP3」遙控器）

| 按鍵 | 功能 |
|------|------|
| 數字 `0`~`9` | 切換到對應城市，自動輪播「時間 + 天氣」訊息（3 頁交替） |
| `PREV` / `NEXT` | 上一個 / 下一個城市（0~9 循環） |
| `VOL+` | 只顯示「天氣訊息」（兩頁交替） |
| `VOL-` | 只顯示「時間訊息」 |
| `EQ` | 顯示當下日期時間（每秒更新） |

時間 + 天氣一個畫面放不下，按數字鍵後會自動以 **3 頁交替**顯示
（頁1：城市+時間；頁2：城市+天氣分類、溫度+溼度；頁3：天氣概況、大氣壓力），
每 3 秒換一頁、時間每秒更新。

### 內建 10 個城市（索引 0~9）

`Taipei` / `New Taipei` / `Taoyuan` / `Hsinchu` / `Taichung` /
`Changhua` / `Chiayi` / `Tainan` / `Kaohsiung` / `Hualien`

### RGB LED 顏色對應（進階功能）

| 天氣 (weather.main) | 顏色 |
|---------------------|------|
| Clear 晴 | 黃 |
| Clouds 雲 / Rain 雨 / Drizzle 毛雨 | 藍 |
| Thunderstorm 雷雨 | 紫 |
| Snow 雪 | 白 |
| Mist/Fog/Haze 霧霾 | 青 |
| 其他 | 綠 |

## 備註：遙控器指令碼不同時

若按鍵沒反應，可能遙控器型號不同。先燒錄一份只印出 `IrReceiver.decodedIRData.command`
的測試程式，按下每個鍵記下實際的 HEX 值，再回頭修改 `exercise2_ir_lcd_weather.ino`
裡 `KEY_*` 的定義即可。
