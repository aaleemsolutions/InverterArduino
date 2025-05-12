public class MqttMessage
{
    public string Type { get; set; } // "config" or "command"
}

public class ModuleConfig
{
    public string WifiSsid { get; set; }
    public string WifiPassword { get; set; }
    public ConfigDetails config { get; set; } // Nested config object
}

public class ConfigDetails
{
    public string baudRate { get; set; } // Match ESP32's expected key
    public int txPin { get; set; }
    public int rxPin { get; set; }
}

public class CommandMessage : MqttMessage
{
    public string command { get; set; }

    public CommandMessage()
    {
        Type = "command";
    }
}
