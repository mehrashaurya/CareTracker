#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

// Wi-Fi Credentials
const char* ssid = "realme 8s 5G";
const char* password = "aaaaaaaa";
const char* API_KEY = "JQT08N88FPY51H6T";
const char* SERVER = "api.thingspeak.com";
const int numElements = 5;
float data[numElements] = { 0 };
String url1 ;
int cnt=0;
void setup() {
  Serial.begin(9600);
  connect();
  randomSeed(analogRead(0));
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  IPAddress localIP = WiFi.localIP();
  String ipAddress = localIP.toString();
  String firstThreeParts = ipAddress.substring(0, ipAddress.lastIndexOf('.'));
  String finalIPAddress = firstThreeParts + ".59";
  url1= "http://"+finalIPAddress+":3000/";
  Serial.println(localIP);
}


void loop() {
  if(WiFi.status() != WL_CONNECTED){
    connect();
  }
  if (Serial.available()) {
    String dataString = Serial.readStringUntil('\n');
    // Parse the data string and split it into individual values
    if (dataString.substring(0, 2) == "::") {
      cnt++;
      int startIndex = 2;
      int commaIndex = dataString.indexOf(',');
      for (int i = 0; i < numElements; i++) {
        if (commaIndex != -1) {
          String valueString = dataString.substring(startIndex, commaIndex);
          data[i] += valueString.toFloat();
          startIndex = commaIndex + 1;
          commaIndex = dataString.indexOf(',', startIndex);
        } else {
          // Last element
          String valueString = dataString.substring(startIndex);
          data[i] = data[i]||(valueString.toInt());
        }
      }
      if(cnt==10){
        sendData(data[4], data[0]/10, data[3]/10, data[1]/10, data[2]/10);
        for(int i=0; i<5; i++){
          data[i]=0;
        }
        cnt=0;
      }
      
    }
  }

}





void sendData(float fall, float am, float gm, float lat, float lng) {
  WiFiClient client;
  HTTPClient http;

  String url = "http://" + String(SERVER) + "/update?api_key=" + String(API_KEY) + "&field1=" + String(fall) + "&field2=" + String(am) + "&field3=" + String(gm)+ "&field4="+String(random(82, 92));
  Serial.print("URL: ");
  Serial.println(url);

  if (http.begin(client, url)) {  // HTTP
    int httpCode = http.GET();
    if (httpCode > 0) {
      Serial.printf("[HTTP] GET... code: %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
        String payload = http.getString();
        Serial.println(payload);
      }
    } else {
      Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("Unable to connect to server");
  }

  if (fall == 1) {
    String url1a = url1 + "send-message?lat=" + String(lat) + "&lng=" + String(lng);
    Serial.print("URL: ");
    Serial.println(url1a);
    if (http.begin(client, url1a)) {
      int httpCode = http.GET();
      if (httpCode > 0) {
        Serial.printf("[HTTP] GET... code: %d\n", httpCode);
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          Serial.println(payload);
        }
      } else {
        Serial.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }
      http.end();
    }
    else {
      Serial.println("Unable to connect to server");
    }

  }
}

void connect(){
   WiFi.begin(ssid, password);

  Serial.print("Connecting to ");
  Serial.println(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    Serial.print(".");
  }
}
