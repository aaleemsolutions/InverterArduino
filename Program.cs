using System;
using System.Text;
using System.Threading.Tasks;
using Microsoft.Extensions.Configuration;
using MQTTnet;
using MQTTnet.Client;
using MQTTnet.Client.Options;
using Microsoft.Extensions.Configuration;
using System.Text.Json;


public static class ConfigLoader
{
    public static AppSettings Load()
    {
        var config = new ConfigurationBuilder()
            .AddJsonFile("appsettings.json", optional: false)
            .Build();

        return config.Get<AppSettings>();
    }

   

}
 
class Program
{
    static async Task Main(string[] args)
    {
        try
        {
            var settings = ConfigLoader.Load();
            var factory = new MqttFactory();
            var mqttClient = factory.CreateMqttClient();

            var options = new MqttClientOptionsBuilder()
                .WithClientId(settings.MQTT.ClientId)
                .WithTcpServer(settings.MQTT.BrokerAddress, settings.MQTT.Port)
                .WithCleanSession()
                .Build();

            mqttClient.UseConnectedHandler(async e =>
            {
                Console.WriteLine(" Connected to broker");

                await mqttClient.SubscribeAsync(new MQTTnet.Client.Subscribing.MqttClientSubscribeOptionsBuilder()
                    .WithTopicFilter(settings.Topics.SubscribeTopic)
                    .Build());

                Console.WriteLine($" Subscribed to topic: {settings.Topics.SubscribeTopic}");
            });

            mqttClient.UseApplicationMessageReceivedHandler(e =>
            {
               var message = e.ApplicationMessage.Payload!=null ? Encoding.UTF8.GetString(e.ApplicationMessage.Payload) : "";
                Console.WriteLine($" Received message: {message}");
            });

            mqttClient.UseDisconnectedHandler(e =>
            {
                Console.WriteLine("Disconnected from broker");
            });

            await mqttClient.ConnectAsync(options);

            Console.WriteLine("Type 'config' to send settings, 'command' to send a command, or 'exit' to quit.");

            string input;
            while ((input = Console.ReadLine()?.Trim().ToLower()) != "exit")
            {
                string jsonPayload = "";

                if (input == "config")
                {
                    var config = new ModuleConfig
                    {
                        WifiSsid = settings.WiFi.SSID,
                        WifiPassword = settings.WiFi.Password,
                        config = new ConfigDetails
                        {
                            baudRate = settings.SerialConfig.BaudRate,
                            txPin = settings.SerialConfig.TxPin,
                            rxPin = settings.SerialConfig.RxPin
                        }
                    };

                    jsonPayload = JsonSerializer.Serialize(config);
                }
                else if (input == "command")
                {
                    var command = new CommandMessage
                    {
                        command = Prompt("Enter Command (e.g., QPIGS, restart)")
                    };
                    jsonPayload = JsonSerializer.Serialize(command);
                }
                else
                {
                    Console.WriteLine("Invalid input. Try 'config', 'command', or 'exit'.");
                    continue;
                }

                var message = new MqttApplicationMessageBuilder()
                    .WithTopic(settings.Topics.PublishTopic)
                    .WithPayload(jsonPayload)
                    .WithExactlyOnceQoS()
                    .WithRetainFlag(false)
                    .Build();

                Console.WriteLine($"Sending payload: {jsonPayload}");
                await mqttClient.PublishAsync(message);
                Console.WriteLine("Message sent.\n");
            }


                await mqttClient.DisconnectAsync();
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Error: {ex.Message}");
        }
    }

    static string Prompt(string label)
    {
        Console.Write($"{label}: ");
        return Console.ReadLine();
    }
}
