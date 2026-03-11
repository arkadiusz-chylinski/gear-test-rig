#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_MLX90614.h>
#include <math.h>
#include <string.h>

// ================== KONFIGURACJA SPRZĘTOWA ==================
Adafruit_MLX90614 mlx = Adafruit_MLX90614();

// SEN0487 MEMS Microphone pins
const int MICROPHONE_PIN = A0;
const int MICROPHONE_ENABLE_PIN = 2;

// 2-Channel Relay pins
const int RELAY1_PIN = 3;
const int RELAY2_PIN = 4;
bool relay1State = false;
bool relay2State = false;

// ================== KONFIGURACJA EMISYJNOŚCI ==================
const float EMISSIVITY_STEEL = 0.69;

// ================== INTERWAŁY CZASOWE ==================
const unsigned long SEND_INTERVAL = 20;
const unsigned long MLX_INTERVAL  = 500;
const unsigned long MIC_INTERVAL  = 20;

// ================== ZNACZNIKI CZASU ==================
unsigned long lastSend    = 0;
unsigned long lastMLXRead = 0;
unsigned long lastMicRead = 0;

// ================== PARAMETRY ADC ==================
const float VREF = 3.0;
const int ADC_MAX_VALUE = 4095;

// ================== OSTATNIE WYNIKI ==================
float ambientTemp = 0.0;
float objectTemp  = 0.0;
float noiseLevel  = 0.0;

// ================== PROTOTYPY ==================
void sendSensorData();
float readMicrophoneDB();
void processIncomingCommand();
void toggleRelay(int channel);

// ================== SETUP ==================
void setup()
{
    pinMode(RELAY1_PIN, OUTPUT);
    pinMode(RELAY2_PIN, OUTPUT);
    digitalWrite(RELAY1_PIN, HIGH);
    digitalWrite(RELAY2_PIN, HIGH);

    pinMode(MICROPHONE_ENABLE_PIN, OUTPUT);
    digitalWrite(MICROPHONE_ENABLE_PIN, HIGH);

    Serial.begin(115200);
    Serial.setTimeout(5);

    while (!Serial) {;}

    Wire.begin();

    if (!mlx.begin())
    {
        while (1) {}
    }

    mlx.writeEmissivity(EMISSIVITY_STEEL);

    lastSend    = millis();
    lastMLXRead = millis();
    lastMicRead = millis();
}

// ================== LOOP ==================
void loop()
{
    unsigned long now = millis();

    if (now - lastMLXRead >= MLX_INTERVAL)
    {
        ambientTemp = mlx.readAmbientTempC();
        objectTemp  = mlx.readObjectTempC();
        lastMLXRead = now;
    }

    if (now - lastMicRead >= MIC_INTERVAL)
    {
        noiseLevel = readMicrophoneDB();
        lastMicRead = now;
    }

    if (now - lastSend >= SEND_INTERVAL)
    {
        sendSensorData();
        lastSend = now;
    }

    if (Serial.available())
    {
        processIncomingCommand();
    }
}

// ================== WYSYŁKA DANYCH ==================
void sendSensorData()
{
    char buffer[64];

    snprintf(buffer,
             sizeof(buffer),
             "D;%.2f;%.2f;%.2f\n",
             ambientTemp,
             objectTemp,
             noiseLevel);

    for (int i = 0; buffer[i] != '\0'; i++)
    {
        if (buffer[i] == '.')
            buffer[i] = ',';
    }

    Serial.print(buffer);
}

// ================== ODCZYT MIKROFONU ==================
float readMicrophoneDB()
{
    const int SAMPLES = 32;

    float sum = 0;

    for (int i = 0; i < SAMPLES; i++)
    {
        int raw = analogRead(MICROPHONE_PIN);
        sum += (raw * VREF) / ADC_MAX_VALUE;
    }

    float offset = sum / SAMPLES;

    float squareSum = 0;

    for (int i = 0; i < SAMPLES; i++)
    {
        int raw = analogRead(MICROPHONE_PIN);

        float voltage = (raw * VREF) / ADC_MAX_VALUE;

        float centered = voltage - offset;

        squareSum += centered * centered;
    }

    float Vrms = sqrt(squareSum / SAMPLES);

    const float refVrms = 0.0012;
    const float refSPL = 30.0;

    float dBSPL = refSPL + 20.0 * log10(Vrms / refVrms);

    if (isnan(dBSPL) || dBSPL < 0)
        dBSPL = 20;

    if (dBSPL > 120)
        dBSPL = 120;

    return dBSPL;
}

// ================== OBSŁUGA KOMEND ==================
void processIncomingCommand()
{
    char cmd[32];

    size_t len = Serial.readBytesUntil('\n', cmd, sizeof(cmd) - 1);

    cmd[len] = '\0';

    if (strncmp(cmd, "T;", 2) == 0)
    {
        int channel = atoi(cmd + 2);

        if (channel == 1 || channel == 2)
        {
            toggleRelay(channel);
        }
    }
}

// ================== STEROWANIE PRZEKAŹNIKAMI ==================
void toggleRelay(int channel)
{
    if (channel == 1)
    {
        relay1State = !relay1State;
        digitalWrite(RELAY1_PIN, relay1State ? LOW : HIGH);
    }
    else if (channel == 2)
    {
        relay2State = !relay2State;
        digitalWrite(RELAY2_PIN, relay2State ? LOW : HIGH);
    }
}
