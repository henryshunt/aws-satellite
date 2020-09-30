#include <Arduino.h>
#include <ArduinoJson.h>

#define ID_PIN 4

bool configured = false;
bool started = false;

bool windSpeedEnabled;
int windSpeedPin;
bool windDirectionEnabled;
int windDirectionPin;

volatile unsigned int windSpeedCounter;


void setup()
{
    pinMode(ID_PIN, INPUT_PULLUP);
    Serial1.begin(115200);
}

void loop()
{
    char command[120] = { '\0' };
    int commandPosition = 0;
    bool commandEnded = false;
    bool commandOverflow = false;

    // Buffer received characters until we receive a new line character
    while (!commandEnded)
    {
        if (Serial1.available())
        {
            char newChar = Serial1.read();

            if (newChar != '\n')
            {
                // If there's no space left in the buffer and this character is
                // not a new line character then don't buffer it
                if (commandPosition == 120)
                {
                    commandOverflow = true;
                    continue;
                }

                command[commandPosition++] = newChar;
            }
            else commandEnded = true;
        }
    }

    if (commandOverflow)
    {
        Serial1.write("ERROR\n");
        return;
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
    else Serial1.write("ERROR\n");
}


/**
 * Responds to the PING serial command. Outputs a textual description of the
 * device for the purposes of differentiating it from other serial devices.
 */
void command_ping()
{
    Serial1.write("AWS Satellite Device\n");
}

/**
 * Responds to the ID serial command. Outputs the ID of the device for the
 * purposes of allowing multiple satellite devices in use at once. ID is set in
 * hardware by toggling a digital pin.
 */
void command_id()
{
    char response[3] = { '\0' };

    sprintf(response, "%d\n", !digitalRead(ID_PIN) + 1);
    Serial1.write(response);
}

/**
 * Responds to the CONFIG serial command. Sets which sensors are enabled and
 * configures those sensors. Outputs OK or ERROR.
 * @param command The received CONFIG command. Format is "CONFIG {JSON}".
 */
void command_config(char* command)
{
    if (strnlen(command, 8) < 8)
    {
        Serial1.write("ERROR\n");
        return;
    }

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

        configured = true;
        started = false;

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
 * Responds to the START serial command. Adds ISRs and resets counters for
 * interrupt-based sensors. Outputs OK or ERROR.
 */
void command_start()
{
    if (!configured || started)
    {
        Serial1.write("ERROR\n");
        return;
    }

    if (windSpeedEnabled)
        attachInterrupt(digitalPinToInterrupt(windSpeedPin), anemometer_interrupt, RISING);

    started = true;
    windSpeedCounter = 0;
    
    Serial1.write("OK\n");
}

/**
 * Responds to the SAMPLE serial command. Samples the enabled sensors. Outputs
 * a JSON string containing the values, or ERROR.
 */
void command_sample()
{
    if (!started)
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