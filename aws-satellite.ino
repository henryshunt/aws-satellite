#include <Arduino.h>
#include <ArduinoJson.h>

#define ID_PIN 4

bool configured = false;
bool firstSample = true;

bool windSpeedEnabled = false;
int windSpeedPin;
bool windDirectionEnabled = false;
int windDirectionPin;

volatile int windSpeedCount = 0;


void setup()
{
    pinMode(ID_PIN, INPUT_PULLUP);
    Serial.begin(115200);
}

void loop()
{
    char command[120] = { '\0' };
    int commandPosition = 0;
    bool commandEnded = false;

    // Buffer received characters until we receive a new line character
    while (!commandEnded)
    {
        if (Serial.available())
        {
            char newChar = Serial.read();

            if (newChar != '\n')
                command[commandPosition++] = newChar;
            else commandEnded = true;
        }
    }

    if (strncmp(command, "PING", 4) == 0)
        command_ping();
    else if (strncmp(command, "ID", 2) == 0)
        command_id();
    else if (strncmp(command, "CONFIG", 6) == 0)
        command_config(command);
    else if (strncmp(command, "SAMPLE", 6) == 0)
        command_sample();
    else Serial.write("ERROR\n");
}


void command_ping()
{
    Serial.write("AWS Satellite Device\n");
}

void command_id()
{
    char response[3] = { '\0' };

    snprintf(response, 3, "%d\n", !digitalRead(ID_PIN) + 1);
    Serial.write(response);
}

void command_config(char* command)
{
    if (configured)
    {
        if (windSpeedEnabled)
            detachInterrupt(digitalPinToInterrupt(windSpeedPin));
        
        configured = false;
        firstSample = true;
    }

    if (strlen(command) >= 8 && configure(command + 7))
    {
        configured = true;

        if (windSpeedEnabled)
        {
            pinMode(windSpeedPin, INPUT_PULLUP);
            attachInterrupt(digitalPinToInterrupt(windSpeedPin), wind_speed_interrupt, FALLING);
        }

        Serial.write("OK\n");
    }
    else Serial.write("ERROR\n");
}

bool configure(char* json)
{
    StaticJsonDocument<JSON_OBJECT_SIZE(4)> document;
    DeserializationError jsonStatus = deserializeJson(document, json);

    if (jsonStatus != DeserializationError::Ok)
        return false;

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
                }
                else return false;
            }
        }
        else return false;
    }
    else return false;

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
                }
                else return false;
            }
        }
        else return false;
    }
    else return false;

    return true;
}

void command_sample()
{
    if (!configured)
    {
        Serial.write("ERROR\n");
        return;
    }

    int windSpeed;
    if (windSpeedEnabled)
    {
        windSpeed = windSpeedCount;
        windSpeedCount = 0;
    }

    int windDirection;
    if (windDirectionEnabled)
    {
        float adcVoltage = analogRead(windDirectionPin) * (5.0 / 1023.0);
        
        if (adcVoltage < 0.25)
            adcVoltage = 0.25;
        else if (adcVoltage > 4.75)
            adcVoltage = 4.75;
        
        windDirection = round((adcVoltage - 0.25) / (4.75 - 0.25) * 360);

        if (windDirection == 360)
            windDirection = 0;
    }

    char sampleOut[50] = { '\0' };
    sample_json(sampleOut, windSpeed, windDirection);
    
    firstSample = false;
    Serial.write(sampleOut);
}

void sample_json(char* sampleOut, int windSpeed, int windDirection)
{
    strcat(sampleOut, "{");
    int length = 1;

    if (windSpeedEnabled && !firstSample)
        length += sprintf(sampleOut + length, "\"windSpeed\":%d", windSpeed);
    else length += sprintf(sampleOut + length, "\"windSpeed\":null");

    if (windDirectionEnabled)
        length += sprintf(sampleOut + length, ",\"windDirection\":%d", windDirection);
    else length += sprintf(sampleOut + length, ",\"windDirection\":null");

    strcat(sampleOut + length, "}\n");
}


void wind_speed_interrupt()
{
    windSpeedCount++;
}