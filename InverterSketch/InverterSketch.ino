#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <ArduinoOTA.h>  // OTA Update Library

// Enable serial debug output
#define USE_SERIAL

WiFiClient espClient;
PubSubClient client(espClient);

// MQTT settings
const char* mqtt_server = "broker.hivemq.com";
const char* subscribeTopic = "esp32/command";
const char* publishTopic = "esp32/status";
const char* debugTopic = "esp32/debug";

// UART settings
int txPin = 17;
int rxPin = 16;
int baudRate = 2400;

HardwareSerial InverterSerial(1);

// Debug output function
void debugOutput(const String &message, bool newline = true) {
  // Send to Serial
  #ifdef USE_SERIAL
    if(newline) {
      Serial.println(message);
    } else {
      Serial.print(message);
    }
  #endif
  
  // Send to MQTT
  if(client.connected()) {
    client.publish(debugTopic, message.c_str());
  }
}

void setupOTA() {
  ArduinoOTA.setHostname("inverter-controller");
  
  ArduinoOTA.onStart([]() {
    String type = ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "filesystem";
    debugOutput("OTA Update Started (" + type + ")");
  });
  
  ArduinoOTA.onEnd([]() {
    debugOutput("\nOTA Update Finished!");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int percent = (progress / (total / 100));
    debugOutput("Progress: " + String(percent) + "%", false);
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    debugOutput("OTA Error[" + String(error) + "]: ");
    if (error == OTA_AUTH_ERROR) debugOutput("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) debugOutput("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) debugOutput("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) debugOutput("Receive Failed");
    else if (error == OTA_END_ERROR) debugOutput("End Failed");
  });
  
  ArduinoOTA.begin();
  debugOutput("OTA Update Service Ready");
}

void setup() {
  #ifdef USE_SERIAL
    Serial.begin(115200);
    while(!Serial); // Wait for serial to initialize
  #endif

  debugOutput("\n\n=== System Starting ===");
  
  // Initialize serial for inverter
  InverterSerial.begin(baudRate, SERIAL_8N1, rxPin, txPin);
  debugOutput("Inverter serial initialized");
  
  // WiFiManager setup
  WiFiManager wm;
  bool res = wm.autoConnect("ESP32_ConfigAP");
  
  if(!res) {
    debugOutput("Failed to connect. Rebooting...");
    delay(3000);
    ESP.restart();
  }
  
  debugOutput("Connected to WiFi");
  debugOutput("IP Address: " + WiFi.localIP().toString());
  
  // Setup OTA updates
  setupOTA();
  
  // MQTT setup
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  if(client.connect("ESP32Client")) {
    client.subscribe(subscribeTopic);
    debugOutput("Subscribed to: " + String(subscribeTopic));
  } else {
    debugOutput("MQTT connection failed");
  }
}

void loop() {
  ArduinoOTA.handle();  // Handle OTA updates
  
  // Maintain WiFi connection
  if(WiFi.status() != WL_CONNECTED) {
    debugOutput("WiFi disconnected. Reconnecting...");
    WiFi.reconnect();
    delay(2000);
    return;
  }

  // Maintain MQTT connection
  if(!client.connected()) {
    reconnectMQTT();
  }
  client.loop();
  
  // Read from inverter
  if(InverterSerial.available()) {
    String data = InverterSerial.readStringUntil('\r');
    debugOutput("Inverter: " + data);
    
    if(client.connected()) {
      client.publish(publishTopic, data.c_str());
    }
  }
  
  // Periodic status query
  static unsigned long lastQuery = 0;
  if(millis() - lastQuery > 5000) {
    InverterSerial.print("QPIGS\r");
    lastQuery = millis();
    debugOutput("Sent QPIGS query");
  }
}

void reconnectMQTT() {
  static unsigned long lastAttempt = 0;
  if(millis() - lastAttempt < 2000) return;
  lastAttempt = millis();

  debugOutput("Attempting MQTT connection...");
  
  String clientId = "ESP32Client_" + String(random(1000, 9999));
  if(client.connect(clientId.c_str())) {
    debugOutput("MQTT connected");
    client.subscribe(subscribeTopic);
  } else {
    debugOutput("MQTT connect failed, rc=" + String(client.state()));
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Build complete message
  String message;
  for(unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  debugOutput("MQTT Callback [" + String(topic) + "]: " + message);

  // Parse JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if(error) {
    debugOutput("JSON parse failed: " + String(error.c_str()));
    return;
  }

  // Handle commands
  if(doc.containsKey("command")) {
    String cmd = doc["command"];
    debugOutput("Executing command: " + cmd);
    InverterSerial.print(cmd + "\r");
    
    String ack = "{\"status\":\"OK\",\"command\":\"" + cmd + "\"}";
    client.publish(publishTopic, ack.c_str());
  }
  
  // Handle config changes
  if(doc.containsKey("config")) {
    JsonObject config = doc["config"];
    
    if(config.containsKey("baudRate")) baudRate = config["baudRate"];
    if(config.containsKey("txPin")) txPin = config["txPin"];
    if(config.containsKey("rxPin")) rxPin = config["rxPin"];
    
    InverterSerial.end();
    InverterSerial.begin(baudRate, SERIAL_8N1, rxPin, txPin);
    debugOutput("Updated UART config");
    
    client.publish(publishTopic, "UART config updated");
  }
}