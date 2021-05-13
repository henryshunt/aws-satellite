#include <Arduino.h>
#include <ArduinoJson.h>

#define MAX_COMMAND_LENGTH 130
char command[MAX_COMMAND_LENGTH + 1] = { '\0' };
int cmdPosition = 0;
bool cmdOverflow = false;

/**
 * Represents configuration data for the satellite device.
 */
struct cfg
{
    bool windSpeedEnabled;
    int windSpeedPin;
    bool windDirEnabled;
    int windDirPin;
    bool sunDurEnabled;
    int sunDurPin;
};

bool configured = false;
cfg config;

volatile unsigned int windSpeedCounter;

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
        const char newChar = Serial1.read();

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
 * Responds to the CONFIG serial command. Loads the configuration and configures the sensors.
 * Outputs OK or ERROR.
 * @param json A JSON string containing the configuration data sent with the command.
 */
void command_config(const char* const json)
{
    cfg newConfig;

    if (!extract_config(json, &newConfig))
    {
        Serial1.write("ERROR\n");
        return false;
    }
    
    if (configured)
    {
        if (config.windSpeedEnabled)
            detachInterrupt(digitalPinToInterrupt(config.windSpeedPin));
    }
    
    config = newConfig;
    configured = true;

    if (config.windSpeedEnabled)
    {
        windSpeedCounter = 0;
        attachInterrupt(digitalPinToInterrupt(config.windSpeedPin),
            wind_speed_interrupt, RISING);
    }

    Serial1.write("OK\n");
}

/**
 * Extracts the configuration values from a JSON string and puts them in a cfg struct.
 * @param json The JSON string containing the configuration data.
 * @param configOut The configuration destination.
 * @return An indication of success or failure.
 */
bool extract_config(const char* const json, cfg* const configOut)
{
    StaticJsonDocument<JSON_OBJECT_SIZE(6)> jsonDocument;

    DeserializationError deserError =
        deserializeJson(jsonDocument, (char* const)json);
    
    if (deserError != DeserializationError::Ok)
        return false;

    const JsonObject jsonObject = jsonDocument.as<JsonObject>();

    if (jsonObject.containsKey("windSpeed") &&
        jsonObject.getMember("windSpeed").is<bool>())
    {
        configOut->windSpeedEnabled = jsonObject.getMember("windSpeed");

        if (configOut->windSpeedEnabled)
        {
            if (jsonObject.containsKey("windSpeedPin"))
            {
                const JsonVariant value = jsonObject.getMember("windSpeedPin");

                if (value.is<int>() && value >= 0)
                    configOut->windSpeedPin = value;
                else return false;
            }
            else return false;
        }
    }
    else return false;

    if (jsonObject.containsKey("windDir") &&
        jsonObject.getMember("windDir").is<bool>())
    {
        configOut->windDirEnabled = jsonObject.getMember("windDir");

        if (configOut->windDirEnabled)
        {
            if (jsonObject.containsKey("windDirPin"))
            {
                const JsonVariant value = jsonObject.getMember("windDirPin");

                if (value.is<int>() && value >= 0)
                    configOut->windDirPin = value;
                else return false;
            }
            else return false;
        }
    }
    else return false;

    if (jsonObject.containsKey("sunDur") &&
        jsonObject.getMember("sunDur").is<bool>())
    {
        configOut->sunDurEnabled = jsonObject.getMember("sunDur");

        if (configOut->sunDurEnabled)
        {
            if (jsonObject.containsKey("sunDurPin"))
            {
                const JsonVariant value = jsonObject.getMember("sunDurPin");

                if (value.is<int>() && value >= 0)
                    configOut->sunDurPin = value;
                else return false;
            }
            else return false;
        }
    }
    else return false;

    return true;
}

/**
 * Responds to the SAMPLE serial command. Samples the enabled sensors and outputs a JSON string
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
    if (config.windSpeedEnabled)
    {
        // Don't calculate the final wind speed. It's up to the user to do that
        // as it's dependent on how often they decide to sample
        windSpeed = windSpeedCounter;

        windSpeedCounter = 0;
    }

    double windDir;
    if (config.windDirEnabled)
    {
        double adcVoltage = analogRead(config.windDirPin) * (5.0 / 1023.0);

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

    bool sunDur;
    if (config.sunDurEnabled)
        sunDur = digitalRead(config.sunDurPin) == HIGH;

    char sampleJson[70] = { '\0' };
    sample_json(sampleJson, windSpeed, windDir, sunDur);
    strcat(sampleJson, "\n");
    Serial1.write(sampleJson);
}

/**
 * Generates the JSON string for a sample. The JSON will contain keys for all possible sensors, not
 * just the currently enabled ones.
 * @param jsonOut The JSON string destination.
 * @param windSpeed The wind speed value.
 * @param windDir The wind direction value.
 * @param sunDur The sunshine duration value.
 */
void sample_json(char* const jsonOut, int windSpeed, double windDir, bool sunDur)
{
    strcat(jsonOut, "{");
    int length = 1;

    if (config.windSpeedEnabled)
        length += sprintf(jsonOut + length, "\"windSpeed\":%d", windSpeed);
    else
    {
        strcat(jsonOut, "\"windSpeed\":null");
        length += 16;
    }

    if (config.windDirEnabled)
    {
        // No default support for formatting floats with sprintf, so do it manually
        const char windDirOut[10] = { '\0' };
        dtostrf(windDir, 9, 5, windDirOut);

        length += sprintf(jsonOut + length, ",\"windDir\":%s", windDirOut);
    }
    else
    {
        strcat(jsonOut, ",\"windDir\":null");
        length += 15;
    }

    if (config.sunDurEnabled)
    {
        if (sunDur)
            strcat(jsonOut, ",\"sunDur\":true");
        else strcat(jsonOut, ",\"sunDur\":false");
    }
    else strcat(jsonOut, ",\"sunDur\":null");

    strcat(jsonOut, "}");
}


/**
 * Interrupt service routine for the wind speed sensor. Increments the wind speed counter.
 */
void wind_speed_interrupt()
{
    windSpeedCounter++;
}