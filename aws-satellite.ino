#include <Arduino.h>
#include <ArduinoJson.h>

bool windSpeedEnabled = false;
int windSpeedPin = 0;
bool windDirectionEnabled = false;
int windDirectionPin = 0;

void setup()
{
    Serial.begin(9600);

    while (true)
    {
        char command[200] = { '\0' };
        int position = 0;
        bool lineEnded = false;

        // Store received characters until we receive a new line character
        while (!lineEnded)
        {
            while (Serial.available())
            {
                char newChar = Serial.read();
                if (newChar != '\n')
                    command[position++] = newChar;
                else lineEnded = true;
            }
        }

        StaticJsonDocument<JSON_OBJECT_SIZE(2)> document;
        DeserializationError jsonStatus = deserializeJson(document, command);
        bool fieldError = false;
        
        if (jsonStatus == DeserializationError::Ok)
        {
            JsonObject jsonObject = document.as<JsonObject>();

            // Check that all values are present in the JSON
            if (jsonObject.containsKey("windSpeed"))
            {
                JsonVariant value = jsonObject.getMember("windSpeed");
                if (value.is<bool>())
                {
                    windSpeedEnabled = value;
                    if (jsonObject.containsKey("windSpeedPin"))
                    {
                        JsonVariant value2 = jsonObject.getMember("windSpeedPin");
                        if (value2.is<int>())
                            windSpeedPin = value2;
                        else fieldError = true;
                    } else fieldError = true;
                } else fieldError = true;
            } else fieldError = true;

            if (jsonObject.containsKey("windDirection"))
            {
                JsonVariant value = jsonObject.getMember("wind_direction");
                if (value.is<bool>())
                {
                    windDirectionEnabled = value;
                    if (jsonObject.containsKey("windDirectionPin"))
                    {
                        JsonVariant value2 = jsonObject.getMember("windDirectionPin");
                        if (value2.is<int>())
                            windDirectionPin = value2;
                        else fieldError = true;
                    } else fieldError = true;
                } else fieldError = true;
            } else fieldError = true;
        }

        if (!fieldError) break;
    }

    if (windSpeedEnabled)
    {
        pinMode(windSpeedPin, INPUT_PULLUP);
        attachInterrupt(digitalPinToInterrupt(windSpeedPin), windSpeedInterrupt, FALLING);
    }

    if (windDirectionEnabled)
        pinMode(windDirectionPin, INPUT);
}

void loop()
{
    // put your main code here, to run repeatedly:
}

void windSpeedInterrupt()
{

}
