public class AppSettings
{
    public WiFiConfig WiFi { get; set; }
    public MQTTConfig MQTT { get; set; }
    public SerialConfig SerialConfig { get; set; }
    public TopicConfig Topics { get; set; }
    public ModuleConfig ModuleConfig { get; set; }
}

public class WiFiConfig
{
    public string SSID { get; set; }
    public string Password { get; set; }
}

public class MQTTConfig
{
    public string BrokerAddress { get; set; }
    public int Port { get; set; }
    public string ClientId { get; set; }
    public string Username { get; set; }
    public string Password { get; set; }
}

public class SerialConfig
{
    public string BaudRate { get; set; }
    public int TxPin { get; set; }
    public int RxPin { get; set; }
}

public class TopicConfig
{
    public string PublishTopic { get; set; }
    public string SubscribeTopic { get; set; }
}
