#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <FS.h>
#include <MicroGear.h>

#define DBG_OUTPUT_PORT Serial

const char* ssid = "minino";
const char* password = "111222333";
const char* host = "node1";

#define APPID   "FishFeeder"
#define KEY     "xxx"
#define SECRET  "yyy"
#define ALIAS   "esp8266"

WiFiClient client;
int timer = 0;
MicroGear microgear(client);
ESP8266WebServer server(80);

String formatBytes(size_t bytes){
  if (bytes < 1024){
    return String(bytes)+"B";
  } else if(bytes < (1024 * 1024)){
    return String(bytes/1024.0)+"KB";
  } else if(bytes < (1024 * 1024 * 1024)){
    return String(bytes/1024.0/1024.0)+"MB";
  } else {
    return String(bytes/1024.0/1024.0/1024.0)+"GB";
  }
}

String getContentType(String filename){
  if(server.hasArg("download")) return "application/octet-stream";
  else if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

bool handleFileRead(String path){
  //DBG_OUTPUT_PORT.println("handleFileRead: " + path);
  if(path.endsWith("/")) path += "index.htm";
  String contentType = getContentType(path);
  String pathWithGz = path + ".gz";
  if(SPIFFS.exists(pathWithGz) || SPIFFS.exists(path)){
    if(SPIFFS.exists(pathWithGz)){
      path += ".gz";
      //DBG_OUTPUT_PORT.println("handleFileRead: " + path + " Found!");
    }
    File file = SPIFFS.open(path, "r");
    size_t sent = server.streamFile(file, contentType);
    file.close();
    return true;
  }
  return false;
}

void onMsghandler(char *topic, uint8_t* msg, unsigned int msglen) {    
    msg[msglen] = '\0';
    String tp = String(topic);
    String mg = String((char *)msg);
    DBG_OUTPUT_PORT.printf("Topic : %s , Data : %s\n",tp.c_str(),mg.c_str());
    if(tp == (String("/")+APPID+"/led")){
      digitalWrite(D0,(mg=="1")?0:1); 
    }
}

void onConnected(char *attribute, uint8_t* msg, unsigned int msglen) {
    DBG_OUTPUT_PORT.println("Connected to NETPIE...");
    microgear.setName(ALIAS);
    microgear.subscribe("/led");
    delay(1000);
}

void setup(void){
  DBG_OUTPUT_PORT.begin(115200);
  DBG_OUTPUT_PORT.print("\n");
  pinMode(D0,OUTPUT);
  SPIFFS.begin();
  
  DBG_OUTPUT_PORT.printf("Connecting to %s\n", ssid);
  if (String(WiFi.SSID()) != String(ssid)) {
    WiFi.begin(ssid, password);
  }
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    DBG_OUTPUT_PORT.print(".");
  }
  DBG_OUTPUT_PORT.println("");
  DBG_OUTPUT_PORT.print("Connected! IP address: ");
  DBG_OUTPUT_PORT.println(WiFi.localIP());
  MDNS.begin(host);
  DBG_OUTPUT_PORT.print("Open http://");
  DBG_OUTPUT_PORT.print(host);
  DBG_OUTPUT_PORT.println(".local/ in browser");
  server.on("/", HTTP_GET, [](){
    if(!handleFileRead("/index.html")) server.send(404, "text/plain", "FileNotFound");
  });
  server.onNotFound([](){
    if(!handleFileRead(server.uri()))
      server.send(404, "text/plain", "FileNotFound");
  });
  server.on("/all", HTTP_GET, [](){
    String json = "{";
    json += "\"heap\":"+String(ESP.getFreeHeap());
    json += ", \"analog\":"+String(analogRead(A0));
    json += ", \"gpio\":"+String((uint32_t)(((GPI | GPO) & 0xFFFF) | ((GP16I & 0x01) << 16)));
    json += "}";
    server.send(200, "text/json", json);
    json = String();
  });
  server.begin();
  DBG_OUTPUT_PORT.println("HTTP server started");

  //------ netpie setup ------//
  microgear.on(MESSAGE,onMsghandler);
  microgear.on(CONNECTED,onConnected);
  microgear.init(KEY,SECRET,ALIAS);
  microgear.connect(APPID);
}
 
void loop(void){
  server.handleClient();
  if (microgear.connected()) {
        microgear.loop();
        if (timer >= 3000) {
            DBG_OUTPUT_PORT.println("Publish...");
            microgear.publish("/time",String(millis()),true);
            timer = 0;
        } 
        else timer += 100;
  }
  else {
        DBG_OUTPUT_PORT.println("connection lost, reconnect...");
        if (timer >= 5000) {
            microgear.connect(APPID);
            timer = 0;
        }
        else timer += 100;
  }
  delay(100);
}
