//参考　https://randomnerdtutorials.com/esp32-esp-now-wi-fi-web-server/

#include <esp_now.h>
#include <WiFi.h>
#include "ESPAsyncWebServer.h"
#include <Arduino_JSON.h>

const char my_ssid[] = "ESP32AP-TEST";
const char my_pass[] = "12345678";       // パスワードは8文字以上
const IPAddress ip(192,168,123,45);
const IPAddress subnet(255,255,255,0);

JSONVar board;

AsyncWebServer server(80);
AsyncEventSource events("/events");

int rcv_data[10];

// espnowの受信コールバック
void OnDataRecv(const uint8_t * mac_addr, const uint8_t *incomingData, int len) { 
  //受信元とデータをシリアルモニタに表示
  char macStr[18];
  Serial.print("Packet received from: ");
  snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.println(macStr);
  Serial.printf("Last Packet Recv Data(%d): ", len);
    for (int i = 0; i < len; i++) {
        rcv_data[i] = incomingData[i];
        Serial.print(incomingData[i]);
        Serial.print(", ");
    }

  //0~255の範囲にして送られたデータを再度修正
  float voltage = (100.0 + float(rcv_data[0]) )/100.0;
  int rpm  = (150.0 + int(rcv_data[1])) *100.0;

  //htmlに埋め込むためにデータを渡している？
  board["id"] = 1;//この番号でBOARD #1or2どちらに表示するかが決まるらしい
  board["temperature"] = voltage;
  board["humidity"] = rpm;
  board["readingId"] = 0;
  String jsonString = JSON.stringify(board);
  events.send(jsonString.c_str(), "new_readings", millis());
  
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>ESP-NOW DASHBOARD</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    p {  font-size: 1.2rem;}
    body {  margin: 0;}
    .topnav { overflow: hidden; background-color: #2f4468; color: white; font-size: 1.7rem; }
    .content { padding: 20px; }
    .card { background-color: white; box-shadow: 2px 2px 12px 1px rgba(140,140,140,.5); }
    .cards { max-width: 700px; margin: 0 auto; display: grid; grid-gap: 2rem; grid-template-columns: repeat(auto-fit, minmax(300px, 1fr)); }
    .reading { font-size: 2.8rem; }
    .packet { color: #bebebe; }
    .card.temperature { color: #fd7e14; }
    .card.humidity { color: #1b78e2; }
  </style>
</head>
<body>
  <div class="topnav">
    <h3>ESP-NOW DASHBOARD</h3>
  </div>
  <div class="content">
    <div class="cards">
      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> BOARD #1 - TEMPERATURE</h4><p><span class="reading"><span id="t1"></span> &deg;C</span></p><p class="packet">Reading ID: <span id="rt1"></span></p>
      </div>
      <div class="card humidity">
        <h4><i class="fas fa-tint"></i> BOARD #1 - HUMIDITY</h4><p><span class="reading"><span id="h1"></span> &percnt;</span></p><p class="packet">Reading ID: <span id="rh1"></span></p>
      </div>
      <div class="card temperature">
        <h4><i class="fas fa-thermometer-half"></i> BOARD #2 - TEMPERATURE</h4><p><span class="reading"><span id="t2"></span> &deg;C</span></p><p class="packet">Reading ID: <span id="rt2"></span></p>
      </div>
      <div class="card humidity">
        <h4><i class="fas fa-tint"></i> BOARD #2 - HUMIDITY</h4><p><span class="reading"><span id="h2"></span> &percnt;</span></p><p class="packet">Reading ID: <span id="rh2"></span></p>
      </div>
    </div>
  </div>
<script>
if (!!window.EventSource) {
 var source = new EventSource('/events');
 
 source.addEventListener('open', function(e) {
  console.log("Events Connected");
 }, false);
 source.addEventListener('error', function(e) {
  if (e.target.readyState != EventSource.OPEN) {
    console.log("Events Disconnected");
  }
 }, false);
 
 source.addEventListener('message', function(e) {
  console.log("message", e.data);
 }, false);
 
 source.addEventListener('new_readings', function(e) {
  console.log("new_readings", e.data);
  var obj = JSON.parse(e.data);
  document.getElementById("t"+obj.id).innerHTML = obj.temperature.toFixed(2);
  document.getElementById("h"+obj.id).innerHTML = obj.humidity.toFixed(2);
  document.getElementById("rt"+obj.id).innerHTML = obj.readingId;
  document.getElementById("rh"+obj.id).innerHTML = obj.readingId;
 }, false);
}
</script>
</body>
</html>)rawliteral";

void setup() {
  Serial.begin(115200);

  //マイコンのmacアドレス表示
  Serial.print("ESP Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  //staモードでwifi起動
  WiFi.mode(WIFI_STA);
  
  WiFi.softAP(my_ssid, my_pass);           // SSIDとパスの設定
  delay(100);                        // このdelayを入れないと失敗する場合がある
  WiFi.softAPConfig(ip, ip, subnet); // IPアドレス、ゲートウェイ、サブネットマスクの設定
  
  IPAddress myIP = WiFi.softAPIP();  // WiFi.softAPIP()でWiFi起動

  // 各種情報を表示
  Serial.print("SSID: ");
  Serial.println(my_ssid);
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  Serial.print("Wi-Fi Channel: ");
  Serial.println(WiFi.channel());
  
  // Init ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }
  esp_now_register_recv_cb(OnDataRecv);//受信割り込みを設定

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html);
  });
   
  events.onConnect([](AsyncEventSourceClient *client){
    if(client->lastId()){
      Serial.printf("Client reconnected! Last message ID that it got is: %u\n", client->lastId());
    }
    // send event with message "hello!", id current millis
    // and set reconnect delay to 1 second
    client->send("hello!", NULL, millis(), 10000);
  });
  server.addHandler(&events);
  server.begin();
}
 
void loop() {
  static unsigned long lastEventTime = millis();
  static const unsigned long EVENT_INTERVAL_MS = 5000;//この時間がたつごとに関数を呼び出してる
  if ((millis() - lastEventTime) > EVENT_INTERVAL_MS) {
    events.send("ping",NULL,millis());
    lastEventTime = millis();
  }
  delay(1);
}


////////////
//ダミーデータ送信をするstick用プログラム

/*
#include <Arduino.h>
#include <M5StickC.h>
#include <esp_wifi.h>
#include <esp_now.h>
#include <WiFi.h>

esp_now_peer_info_t peerInfo;
uint8_t slaveAddress[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
//uint8_t slaveAddress[] = { 0xE8, 0x9F, 0x6D, 0x0C, 0xC6, 0x04 };

// 送信コールバック
void OnDataSent(const uint8_t *mac_addr, esp_now_send_status_t status) {
  char macStr[18];
  snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  Serial.print("Last Packet Sent to: ");
  Serial.println(macStr);
  Serial.print("Last Packet Send Status: ");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
  // 画面にも描画
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(0, 0);
  M5.Lcd.print("Last Packet Sent to: \n  ");
  M5.Lcd.println(macStr);
  M5.Lcd.print("Last Packet Send Status: \n  ");
  M5.Lcd.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

void setup() {
  M5.begin();
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setRotation(3);

  M5.Lcd.print("ESP-NOW Test\n");
  // ESP-NOW初期化
  WiFi.mode(WIFI_STA);

  if (esp_now_init() == ESP_OK) {
  Serial.println("ESPNow Init Success");
  }
  delay(20);
  memcpy(peerInfo.peer_addr, slaveAddress, 6);
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer_1");
    return;
  }

  // ESP-NOWコールバック登録
  esp_now_register_send_cb(OnDataSent);
}

void loop() {
  M5.update();
  // ボタンを押したら送信
  if ( M5.BtnA.wasPressed() ) {
    //送るデータ
    uint8_t data[2] = {random(0,255),random(0,255)};
    esp_err_t result = esp_now_send(peerInfo.peer_addr, data, sizeof(data));

    Serial.print("Send Status: ");
    if (result == ESP_OK) {
      Serial.println("Success");
    } else if (result == ESP_ERR_ESPNOW_NOT_INIT) {
      Serial.println("ESPNOW not Init.");
    } else if (result == ESP_ERR_ESPNOW_ARG) {
      Serial.println("Invalid Argument");
    } else if (result == ESP_ERR_ESPNOW_INTERNAL) {
      Serial.println("Internal Error");
    } else if (result == ESP_ERR_ESPNOW_NO_MEM) {
      Serial.println("ESP_ERR_ESPNOW_NO_MEM");
    } else if (result == ESP_ERR_ESPNOW_NOT_FOUND) {
      Serial.println("Peer not found.");
    } else {
      Serial.println("Not sure what happened");
    }
  }
  delay(1);
}
*/