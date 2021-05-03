#include <Arduino.h>
#include <ArduinoJson.h>

#define Serial1 Serial

#define MAX_COMMAND_LENGTH 120
char command[MAX_COMMAND_LENGTH + 1] = { '\0' };
int cmdPosition = 0;
bool cmdOverflow = false;

bool configured = false;

bool windSpeedEnabled;
int windSpeedPin;
volatile unsigned int windSpeedCounter = 0;
bool windDirEnabled;
int windDirPin;

void setup()
{
    Serial1.begin(115200);
}

/**
 * Receives a serial command and calls the appropriate function to deal with it.
 */
void loop()
{
    bool cmdEnded = false;

    if (Serial1.available())
    {
        char newChar = Serial1.read();

        if (newChar != '\n')
        {
            if (cmdPosition < MAX_COMMAND_LENGTH)
                command[cmdPosition++] = newChar;
            else cmdOverflow = true;
        }
        else
        {
            command[cmdPosition] = '\0';
            cmdEnded = true;
        }
    }

    if (cmdEnded)
    {
        cmdPosition = 0;

        if (!cmdOverflow)
        {
            if (strlen(command) > 7 && strncmp(command, "CONFIG ", 7) == 0)
                command_config(command + 7);
            else if (strlen(command) == 6 && strcmp(command, "SAMPLE") == 0)
                command_sample();
            else Serial1.write("ERROR\n");
        }
        else
        {
            Serial1.write("ERROR\n");
            cmdOverflow = false;
        }
    }
}

/**
 * Responds to the CONFIG serial command. Sets which sensors are enabled and configures those
 * sensors. Outputs OK or ERROR.
 * @param json JSON containing the configuration data sent with the command.
 */
void command_config(char* json)
{
    bool oldWindSpeedEnabled = windSpeedEnabled;
    int oldWindSpeedPin = windSpeedPin;

    if (extract_config(json))
    {
        if (configured)
        {
            if (oldWindSpeedEnabled)
                detachInterrupt(digitalPinToInterrupt(oldWindSpeedPin));
        }
        else configured = true;

        windSpeedCounter = 0;
        attachInterrupt(digitalPinToInterrupt(windSpeedPin),
            wind_speed_interrupt, RISING);

        Serial1.write("OK\n");
    }
    else Serial1.write("ERROR\n");
}

/**
 * Extracts the configuration values from a JSON string and stores them in the global variables.
 * @param json JSON containing the configuration data.
 * @return An indication of success or failure.
 */
bool extract_config(char* json)
{
    StaticJsonDocument<JSON_OBJECT_SIZE(4)> jsonDocument;

    if (deserializeJson(jsonDocument, json) != DeserializationError::Ok)
        return false;

    bool newWindSpeedEnabled = false;
    int newWindSpeedPin;
    bool newWindDirEnabled = false;
    int newWindDirPin;

    JsonObject jsonObject = jsonDocument.as<JsonObject>();


    if (jsonObject.containsKey("windSpeedEnabled"))
    {
        JsonVariant value = jsonObject.getMember("windSpeedEnabled");

        if (value.is<bool>())
        {
            newWindSpeedEnabled = value;
            if (newWindSpeedEnabled)
            {
                if (jsonObject.containsKey("windSpeedPin"))
                {
                    value = jsonObject.getMember("windSpeedPin");

                    if (value.is<int>() && value >= 0)
                        newWindSpeedPin = value;
                    else return false;
                }
                else return false;
            }
        }
        else return false;
    }
    else return false;

    if (jsonObject.containsKey("windDirEnabled"))
    {
        JsonVariant value = jsonObject.getMember("windDirEnabled");

        if (value.is<bool>())
        {
            newWindDirEnabled = value;
            if (newWindDirEnabled)
            {
                if (jsonObject.containsKey("windDirPin"))
                {
                    value = jsonObject.getMember("windDirPin");

                    if (value.is<int>() && value >= 0)
                        newWindDirPin = value;
                    else return false;
                }
                else return false;
            }
        }
        else return false;
    }
    else return false;


    windSpeedEnabled = newWindSpeedEnabled;
    windSpeedPin = newWindSpeedPin;
    windDirEnabled = newWindDirEnabled;
    windDirPin = newWindDirPin;

    return true;
}

/**
 * Responds to the SAMPLE serial command. Samples the enabled sensors. Outputs a JSON string
 * containing the values, or ERROR.
 */
void command_sample()
{
    if (!configured)
    {
        Serial1.write("ERROR\n");
        return;
    }

    int windSpeed;
    if (windSpeedEnabled)
    {
        // Don't calculate the final wind speed. It's up to the user to do that
        // as it's dependent on how often they decide to sample
        windSpeed = windSpeedCounter;

        windSpeedCounter = 0;
    }

    double windDir;
    if (windDirEnabled)
    {
        double adcVoltage = analogRead(windDirPin) * (5.0 / 1023.0);

        if (adcVoltage < 0.25)
            adcVoltage = 0.25;
        else if (adcVoltage > 4.75)
            adcVoltage = 4.75;
        
        // Convert voltage to degrees. For Inspeed E-Vane II, 5% of input is 0
        // degrees and 75% is 360 degrees
        windDir = (adcVoltage - 0.25) / (4.75 - 0.25) * 360;

        if (windDir == 360)
            windDir = 0;
    }

    char sampleJson[50] = { '\0' };
    sample_json(sampleJson, windSpeed, windDir);
    strcat(sampleJson, "\n");
    Serial1.write(sampleJson);
}

/**
 * Generates the JSON for a sample. The JSON will contain keys for all possible sensors, not just
 * the currently enabled ones.
 * @param jsonOut The JSON destination.
 * @param windSpeed The wind speed value.
 * @param windDir The wind direction value.
 */
void sample_json(char* jsonOut, int windSpeed, double windDir)
{
    strcat(jsonOut, "{");
    int length = 1;

    if (windSpeedEnabled)
        length += sprintf(jsonOut + length, "\"windSpeed\":%d", windSpeed);
    else
    {
        strcat(jsonOut, "\"windSpeed\":null");
        length += 16;
    }

    if (windDirEnabled)
    {
        // No default support for formatting floats with sprintf, so do it manually
        char windDirOut[10] = { '\0' };
        dtostrf(windDir, 9, 5, windDirOut);

        length += sprintf(jsonOut + length, ",\"windDir\":%s", windDirOut);
    }
    else
    {
        strcat(jsonOut, ",\"windDir\":null");
        length += 15;
    }

    strcat(jsonOut, "}");
}


/**
 * Interrupt service routine for the wind speed sensor. Increments the wind speed counter.
 */
void wind_speed_interrupt()
{
    windSpeedCounter++;
}