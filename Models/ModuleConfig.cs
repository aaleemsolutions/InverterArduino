public class MqttMessage
{
    public string Type { get; set; } // "config" or "command"
}

public class ModuleConfig : MqttMessage
{
    public string WifiSsid { get; set; }
    public string WifiPassword { get; set; }
    public string Baudrate { get; set; }
    public int TxPin { get; set; }
    public int RxPin { get; set; }

    public ModuleConfig()
    {
        Type = "config";
    }
}

public class CommandMessage : MqttMessage
{
    public string Action { get; set; }

    public CommandMessage()
    {
        Type = "command";
    }
}
