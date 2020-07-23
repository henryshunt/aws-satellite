#include <Arduino.h>
#include <ArduinoJson.h>

#define ID_PIN 4

bool configured = false;

bool windSpeedEnabled = false;
int windSpeedPin;
bool windDirectionEnabled = false;
int windDirectionPin;

int windSpeedCount = 0;


void setup()
{
    pinMode(ID_PIN, INPUT_PULLUP);
    Serial.begin(9600);

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


        // PING command. Respond with a description of the device
        if (strncmp(command, "PING", 4) == 0)
            Serial.println("AWS Satellite Device\n");

        // ID command. Respond with the ID of the satellite device
        if (strncmp(command, "ID", 2) == 0)
        {
            char response[3] = { '\0' };

            // ID is set in hardware by toggling a digital pin (which provides two unique IDs)
            sprintf(response, "%d\n", !digitalRead(ID_PIN) + 1);
            Serial.println(response);
        }

        // CONFIG command. Configure the satellite device using the sent JSON
        else if (strncmp(command, "CONFIG {", 8) == 0)
        {
            if (configure(command + 7))
            {
                configured = true;
                Serial.println("CONFIG\n");
            }
            else
            {
                Serial.println("CONFIG ERROR\n");
                continue;
            }

            if (windSpeedEnabled)
                pinMode(windSpeedPin, INPUT_PULLUP);
        }

        // START command. Exit setup() and move to loop() for sensor sampling
        if (strncmp(command, "START", 5) == 0)
        {
            if (configured)
            {
                Serial.println("START\n");
                attachInterrupt(digitalPinToInterrupt(windSpeedPin), windSpeedInterrupt, FALLING);
                break;
            }
            else Serial.println("START ERROR\n");
        }
    }
}

bool configure(char* json)
{
    StaticJsonDocument<JSON_OBJECT_SIZE(4)> document;
    DeserializationError jsonStatus = deserializeJson(document, json);

    if (jsonStatus != DeserializationError::Ok)
        return;

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
                    value = jsonObject.getMember("windSpeedPin");

                    if (value.is<int>())
                        windSpeedPin = value;
                    else return false;
                } else return false;
            }
        } else return false;
    } else return false;

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
                    value = jsonObject.getMember("windDirectionPin");

                    if (value.is<int>())
                        windDirectionPin = value;
                    else return false;
                } else return false;
            }
        } else return false;
    } else return false;

    return true;
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


        // SAMPLE command. Read sensor values and return them in a JSON string
        if (strncmp(command, "SAMPLE", 6) == 0)
        {
            int windSpeed;
            if (windSpeedEnabled)
            {
                windSpeed = windSpeedCount;
                windSpeedCount = 0;
            }

            int windDirection;

            // Read wind direction
            if (windDirectionEnabled)
            {
                float adcValue = analogRead(windDirectionPin);
                
                if (adcValue < 0.25)
                    adcValue = 0.25;
                else if (adcValue > 4.75)
                    adcValue = 4.75;
                
                windDirection = round(map(adcValue, 0.25, 4.75, 0, 360));

                if (windDirection == 360)
                    windDirection = 0;
            }


            char sample_out[50] = { '\0' };
            strcat(sample_out, "SAMPLE {");
            int length = 8;

            if (windSpeedEnabled)
                length += sprintf(sample_out + length, "\"windSpeed\":%d", windSpeed);
            else length += sprintf(sample_out + length, "\"windSpeed\":null");

            if (windDirectionEnabled)
                length += sprintf(sample_out + length, ",\"windDirection\":%d", windDirection);
            else length += sprintf(sample_out + length, ",\"windDirection\":null");

            strcat(sample_out + length, "}");
            
            Serial.println(sample_out);
        }
    }
}

void windSpeedInterrupt()
{
    windSpeedCount++;
}