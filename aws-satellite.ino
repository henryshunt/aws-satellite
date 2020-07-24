#include <Arduino.h>
#include <ArduinoJson.h>

#define ID_PIN 4

bool configured = false;
bool started = false;

bool windSpeedEnabled = false;
int windSpeedPin;
bool windDirectionEnabled = false;
int windDirectionPin;

int windSpeedCount = 0;


void setup()
{
    pinMode(ID_PIN, INPUT_PULLUP);
    Serial.begin(9600);
}

void loop()
{
    char command[100] = { '\0' };
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
    else if (strncmp(command, "START", 5) == 0)
        command_start();
    else if (strncmp(command, "SAMPLE", 6) == 0)
        command_sample();
    else if (strncmp(command, "STOP", 4) == 0)
        command_stop(true);
    else Serial.write("ERROR\n");
}


void command_ping()
{
    Serial.write("AWS Satellite Device\n");
}

void command_id()
{
    char response[3] = { '\0' };

    // ID is set in hardware by toggling a digital pin (which provides two unique IDs)
    sprintf(response, "%d\n", !digitalRead(ID_PIN) + 1);
    Serial.write(response);
}

void command_config(char* command)
{
    command_stop(false);
    configured = false;

    if (strlen(command) < 8)
    {
        Serial.write("ERROR\n");
        return;
    }

    if (configure(command + 7))
    {
        configured = true;

        if (windSpeedEnabled)
            pinMode(windSpeedPin, INPUT_PULLUP);

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

void command_start()
{
    if (configured)
    {
        attachInterrupt(digitalPinToInterrupt(windSpeedPin), wind_speed_interrupt, FALLING);
        started = true;

        Serial.write("OK\n");
    }
    else Serial.write("ERROR\n");
}

void command_sample()
{
    if (!started)
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


    char sampleOut[50] = { '\0' };
    strcat(sampleOut, "{");
    int length = 1;

    if (windSpeedEnabled)
        length += sprintf(sampleOut + length, "\"windSpeed\":%d", windSpeed);
    else length += sprintf(sampleOut + length, "\"windSpeed\":null");

    if (windDirectionEnabled)
        length += sprintf(sampleOut + length, ",\"windDirection\":%d", windDirection);
    else length += sprintf(sampleOut + length, ",\"windDirection\":null");

    strcat(sampleOut + length, "}\n");
    
    Serial.write(sampleOut);
}

void command_stop(bool respond)
{
    if (started)
    {
        if (windSpeedEnabled)
            detachInterrupt(digitalPinToInterrupt(windSpeedPin));
        
        started = false;
    }

    if (respond)
        Serial.write("OK\n");
}


void wind_speed_interrupt()
{
    windSpeedCount++;
}