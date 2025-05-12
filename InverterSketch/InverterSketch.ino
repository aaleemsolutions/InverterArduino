#include <WiFiManager.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

WiFiClient espClient;
PubSubClient client(espClient);

// Default MQTT settings
const char* mqtt_server = "broker.hivemq.com";
const char* subscribeTopic = "esp32/command";  // Where ESP32 receives commands
const char* publishTopic = "esp32/status";     // Where ESP32 publishes data

// UART settings with defaults
int txPin = 17;
int rxPin = 16;
int baudRate = 2400;  // Changed to 2400 (common for inverters)

HardwareSerial InverterSerial(1);

void setup() {
  Serial.begin(115200);
  
  // Initialize serial for inverter communication
  InverterSerial.begin(baudRate, SERIAL_8N1, rxPin, txPin);
  
  // WiFiManager setup
  WiFiManager wm;
  bool res = wm.autoConnect("ESP32_ConfigAP");
  
  if (!res) {
    Serial.println("Failed to connect. Rebooting...");
    delay(3000);
    ESP.restart();
  }
  
  Serial.println("Connected to WiFi");
  
  // MQTT setup
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  // Subscribe to a test topic
if (client.connect("ESP32TestClient")) {
 
  client.subscribe(subscribeTopic); // Add this
  Serial.println("Subscribed to    esp32/command");
}
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi disconnected. Reconnecting...");
    WiFi.reconnect();
    delay(2000);
  }
  if (!client.connected()) {
    Serial.println("MQTT disconnected. Reconnecting...");
    reconnectMQTT();
  } else {
    static unsigned long lastStatus = 0;
    if (millis() - lastStatus > 5000) {
      Serial.println("MQTT connected. State: " + String(client.state()));
      Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
      lastStatus = millis();
    }
  }
  client.loop();
  
  // Check for incoming serial data from inverter
  if (InverterSerial.available()) {
    String data = InverterSerial.readStringUntil('\r');
    Serial.print("Inverter: ");
    Serial.println(data);
    
    // Forward to MQTT
    if (client.connected()) {
      client.publish(publishTopic, data.c_str());
    }
  }
  
  // Periodically query inverter status
  static unsigned long lastQuery = 0;
  if (millis() - lastQuery > 5000) {
    InverterSerial.print("QPIGS\r");
    lastQuery = millis();
  }
}

void reconnectMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    String clientId = "ESP32Client_" + String(random(1000, 9999)); // Unique client ID
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      Serial.print("Subscribing to: ");
      Serial.println(subscribeTopic);
      bool subscribed = client.subscribe(subscribeTopic, 1); // QoS 1 for reliability
      if (subscribed) {
        Serial.println("Subscription successful");
      } else {
        Serial.println("Subscription failed");
      }
    } else {
      Serial.print("failed, rc=");
      Serial.println(client.state());
      Serial.println("Retrying in 2 seconds...");
      delay(2000);
    }
  }
}
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Callback triggered! Topic: ");
  Serial.println(topic);
  Serial.print("Payload: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  
  // Create a string from the payload
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  
  // Parse JSON
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, message);
  
  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    return;
  }

  // Log parsed JSON
  serializeJson(doc, Serial);
  Serial.println();

  // Handle commands
  if (doc.containsKey("command")) {
    String cmd = doc["command"];
    Serial.println("Received command: " + cmd);
    InverterSerial.print(cmd + "\r");
    
    String ack = "{\"status\":\"OK\",\"command\":\"" + cmd + "\"}";
    client.publish(publishTopic, ack.c_str());
  }
  
  // Handle configuration changes
  if (doc.containsKey("config")) {
    JsonObject config = doc["config"];
    
    if (config.containsKey("baudRate")) {
      baudRate = config["baudRate"];
    }
    if (config.containsKey("txPin")) {
      txPin = config["txPin"];
    }
    if (config.containsKey("rxPin")) {
      rxPin = config["rxPin"];
    }
    
    InverterSerial.end();
    InverterSerial.begin(baudRate, SERIAL_8N1, rxPin, txPin);
    
    client.publish(publishTopic, "UART config updated");
  }
}