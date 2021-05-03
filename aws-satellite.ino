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
bool windDirectionEnabled;
int windDirectionPin;

void setup()
{
    Serial1.begin(115200);
}

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
                command_config(command);
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
 * Responds to the CONFIG serial command. Sets which sensors are enabled and
 * configures those sensors. Outputs OK or ERROR.
 * @param command The received CONFIG command. Format is "CONFIG {JSON}".
 */
void command_config(char* command)
{
    bool oldWindSpeedEnabled = windSpeedEnabled;
    int oldWindSpeedPin = windSpeedPin;

    if (extract_config(command + 7))
    {
        // Clear up previous configuration
        if (configured)
        {
            if (oldWindSpeedEnabled)
                detachInterrupt(digitalPinToInterrupt(oldWindSpeedPin));
        }

        windSpeedCounter = 0;
        attachInterrupt(digitalPinToInterrupt(windSpeedPin),
            anemometer_interrupt, RISING);

        configured = true;
        Serial1.write("OK\n");
    }
    else Serial1.write("ERROR\n");
}

/**
 * Extracts the configuration values from a JSON string and stores them.
 * @param json JSON string containing configuration values.
 * @return An indication of success or failure.
 */
bool extract_config(char* json)
{
    StaticJsonDocument<JSON_OBJECT_SIZE(4)> jsonDocument;
    DeserializationError jsonStatus = deserializeJson(jsonDocument, json);

    if (jsonStatus != DeserializationError::Ok)
        return false;

    bool newWindSpeedEnabled = false;
    int newWindSpeedPin;
    bool newWindDirectionEnabled = false;
    int newWindDirectionPin;

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

    if (jsonObject.containsKey("windDirectionEnabled"))
    {
        JsonVariant value = jsonObject.getMember("windDirectionEnabled");

        if (value.is<bool>())
        {
            newWindDirectionEnabled = value;
            if (newWindDirectionEnabled)
            {
                if (jsonObject.containsKey("windDirectionPin"))
                {
                    value = jsonObject.getMember("windDirectionPin");

                    if (value.is<int>() && value >= 0)
                        newWindDirectionPin = value;
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
    windDirectionEnabled = newWindDirectionEnabled;
    windDirectionPin = newWindDirectionPin;

    return true;
}

/**
 * Responds to the SAMPLE serial command. Samples the enabled sensors. Outputs
 * a JSON string containing the values, or ERROR.
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
        // Don't calculate the final wind speed. It is up to the user to do that
        // as it is dependent on how often they decide to sample
        windSpeed = windSpeedCounter;

        windSpeedCounter = 0;
    }

    double windDirection;
    if (windDirectionEnabled)
    {
        double adcVoltage = analogRead(windDirectionPin) * (5.0 / 1023.0);

        if (adcVoltage < 0.25)
            adcVoltage = 0.25;
        else if (adcVoltage > 4.75)
            adcVoltage = 4.75;
        
        // Convert voltage to degrees. For Inspeed E-Vane II, 5% of input is 0
        // degrees and 75% is 360
        windDirection = (adcVoltage - 0.25) / (4.75 - 0.25) * 360;

        if (windDirection == 360)
            windDirection = 0;
    }

    char sampleJson[50] = { '\0' };
    sample_json(sampleJson, windSpeed, windDirection);
    Serial1.write(sampleJson);
}

/**
 * Generates the JSON for a sample, containing the values for all sensors.
 * @param jsonOut JSON string destination.
 * @param windSpeed The wind speed value.
 * @param windDirection The wind direction value.
 */
void sample_json(char* jsonOut, int windSpeed, double windDirection)
{
    strcat(jsonOut, "{");
    int length = 1;

    // Ignore the first sample for wind speed since the counter value is not
    // considered valid between the CONFIG and first SAMPLE commands
    if (windSpeedEnabled)
        length += sprintf(jsonOut + length, "\"windSpeed\":%d", windSpeed);
    else
    {
        strcat(jsonOut + length, "\"windSpeed\":null");
        length += 16;
    }

    if (windDirectionEnabled)
    {
        // No default support for formatting floats with sprintf, so do it manually
        char windDirectionOut[10];
        dtostrf(windDirection, 4, 2, windDirectionOut);

        length += sprintf(jsonOut + length, ",\"windDirection\":%s", windDirectionOut);
    }
    else
    {
        strcat(jsonOut + length, ",\"windDirection\":null");
        length += 21;
    }

    strcat(jsonOut + length, "}\n");
}


/**
 * ISR for the anemometer. Increments the wind speed counter.
 */
void anemometer_interrupt()
{
    windSpeedCounter++;
}