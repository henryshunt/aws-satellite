#include <Arduino.h>
#include <ArduinoJson.h>

int satelliteIdPin1 = 2;
int satelliteIdPin2 = 3;

bool windSpeedEnabled = false;
int windSpeedPin = 0;

int windSpeedSamplingBucket = 1;
int windSpeedSamplingBucket1 = 0;
int windSpeedSamplingBucket2 = 0;

bool windDirectionEnabled = false;
int windDirectionPin = 0;

void setup()
{
    Serial.begin(115200);
    pinMode(satelliteIdPin1, INPUT_PULLUP);
    pinMode(satelliteIdPin2, INPUT_PULLUP);

    while (true)
    {
        char command[200] = { '\0' };
        int position = 0;
        bool commandEnded = false;

        // Buffer received characters until we receive a new line character
        while (!commandEnded)
        {
            if (Serial.available())
            {
                char newChar = Serial.read();
                if (newChar != '\n')
                    command[position++] = newChar;
                else commandEnded = true;
            }
        }

        // Respond with the ID of the device (encoded as a binary number using two digital pins)
        if (strncmp(command, "PING", 4) == 0)
        {
            int satelliteId =
                (2 * !digitalRead(satelliteIdPin1)) + !digitalRead(satelliteIdPin2); // Binary to denary

            char response[7] = { '\0' };
            sprintf(response, "PING %d", satelliteId);
            Serial.println(response);
        }

        // Configure the device with a JSON string indicating sensor information
        else if (strncmp(command, "CONFIG {", 8) == 0)
        {
            StaticJsonDocument<JSON_OBJECT_SIZE(4)> document;
            DeserializationError jsonStatus = deserializeJson(document, command + 7);

            if (jsonStatus != DeserializationError::Ok)
            {
                Serial.println("CONFIG ERROR");
                continue;
            }

            bool fieldError = false;
            JsonObject jsonObject = document.as<JsonObject>();

            if (jsonObject.containsKey("windSpeedEnabled"))
            {
                JsonVariant value = jsonObject.getMember("windSpeedEnabled");
                if (value.is<bool>())
                {
                    windSpeedEnabled = value;
                    if (windSpeedEnabled)
                    {
                        if (jsonObject.containsKey("windSpeedPin"))
                        {
                            JsonVariant value2 = jsonObject.getMember("windSpeedPin");
                            if (value2.is<int>())
                                windSpeedPin = value2;
                            else fieldError = true;
                        } else fieldError = true;
                    }
                } else fieldError = true;
            } else fieldError = true;

            if (jsonObject.containsKey("windDirectionEnabled"))
            {
                JsonVariant value = jsonObject.getMember("windDirectionEnabled");
                if (value.is<bool>())
                {
                    windDirectionEnabled = value;
                    if (windDirectionEnabled)
                    {
                        if (jsonObject.containsKey("windDirectionPin"))
                        {
                            JsonVariant value2 = jsonObject.getMember("windDirectionPin");
                            if (value2.is<int>())
                                windDirectionPin = value2;
                            else fieldError = true;
                        } else fieldError = true;
                    }
                } else fieldError = true;
            } else fieldError = true;

            if (fieldError)
            {
                Serial.println("CONFIG ERROR");
                continue;
            }
            else Serial.println("CONFIG");

            if (windSpeedEnabled)
            {
                pinMode(windSpeedPin, INPUT);
                attachInterrupt(digitalPinToInterrupt(windSpeedPin), windSpeedInterrupt, RISING);
            }
        }

        // Move to the main loop function
        if (strncmp(command, "START", 5) == 0)
        {
            Serial.println("START");
            break;
        }
    }
}

void loop()
{
    while (true)
    {
        char command[10] = { '\0' };
        int position = 0;
        bool commandEnded = false;

        // Buffer received characters until we receive a new line character
        while (!commandEnded)
        {
            if (Serial.available())
            {
                char newChar = Serial.read();
                if (newChar != '\n')
                    command[position++] = newChar;
                else commandEnded = true;
            }
        }

        if (strncmp(command, "CLOCK", 5) == 0)
        {
            if (windSpeedSamplingBucket == 1)
                windSpeedSamplingBucket = 2;
            else windSpeedSamplingBucket = 1;

            int windDirection = 0;
            if (windDirectionEnabled)
            {
                float windDirectionADC = analogRead(windDirectionPin);
                
                // Convert voltage to degrees
                if (windDirectionADC < 0.25) windDirectionADC = 0.25;
                else if (windDirectionADC > 4.75) windDirectionADC = 4.75;
                
                windDirection = round(map(windDirectionADC, 0.25, 4.75, 0, 360));
                if (windDirection == 360) windDirection = 0;
            }

            const char* format = "{\"windSpeed\":%d,\"windDirection\":%d}";
            char samples[100] = { '\0' };

            int windSpeed = 0;
            if (windSpeedSamplingBucket == 2)
            {
                windSpeed = windSpeedSamplingBucket1;
                windSpeedSamplingBucket1 = 0;
            }
            else
            {
                windSpeed = windSpeedSamplingBucket2;
                windSpeedSamplingBucket2 = 0;
            }

            sprintf(samples, format, windSpeed, windDirection);
            Serial.println(samples);
        }
        else if (strncmp(command, "clear", 5) == 0)
        {
            
        }
    }
}

void windSpeedInterrupt()
{
    if (windSpeedSamplingBucket == 1)
        windSpeedSamplingBucket1++;
    else windSpeedSamplingBucket2++;
}
