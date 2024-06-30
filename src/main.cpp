#include <WiFi.h>
#include <FirebaseClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <StringSplitter.h>

#define WIFI_SSID "Wokwi-GUEST"
#define WIFI_PASSWORD ""

#define DATABASE_SECRET "2Ndf57Cyxnjr8UB6Z3OnTiFrh9S0PnwlqLzHdPq2"
#define DATABASE_URL "https://smart-medicine-box-2b0c5-default-rtdb.firebaseio.com/"

#define BUTTON 7
#define LED_12 19
#define LED_11 20
#define LED_10 21
#define LED_9 47
#define LED_8 48
#define LED_7 45
#define LED_6 35
#define LED_5 36
#define LED_4 37
#define LED_3 38
#define LED_2 39
#define LED_1 40

LiquidCrystal_I2C lcd(0x27, 20, 4);

WiFiClientSecure ssl;
DefaultNetwork network;
AsyncClientClass client(ssl, getNetwork(network));

FirebaseApp app;
RealtimeDatabase database;
AsyncResult result;
LegacyToken dbSecret(DATABASE_SECRET);
JsonDocument reminder_doc;
JsonDocument obat_doc;

int currBox = 0;
uint8_t LEDS[] = {LED_1, LED_2, LED_3, LED_4, LED_5, LED_6, LED_7, LED_8, LED_9, LED_10, LED_11, LED_12};
String currQTime = "";
int reminderTimeIdx = 0;
bool buttonPressed = false;

JsonDocument getReminders();

struct tm timeinfo;
int obatState = 0;

RTC_DS3231 rtc;
JsonDocument reminders;

void setup()
{

    Serial.begin(115200);

    // connect to wifi
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to Wi-Fi");

    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(F("."));
        delay(300);
    }


    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();
    // if (!rtc.begin())
    // {
    //     Serial.println("Couldn't find RTC");
    //     Serial.flush();
    //     while (1)
    //         delay(10);
    // }

    if (rtc.lostPower())
    {
        Serial.println("RTC lost power, setting the time!");
        // Menyetel waktu ke waktu default, bisa diganti dengan waktu yang diinginkan
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // firebase init
    ssl.setInsecure();

    initializeApp(client, app, getAuth(dbSecret), result);

    app.getApp<RealtimeDatabase>(database);

    database.url(DATABASE_URL);

    client.setAsyncResult(result);

    // code
    for (auto i : LEDS)
    {
        pinMode(i, OUTPUT);
    }
    pinMode(BUTTON, INPUT_PULLUP);

    Serial.println(F("Starting..."));
}

void loop()
{
    String time = "";
    int buttonState = digitalRead(BUTTON);
    // app.loop();
    // database.loop();

    if (app.ready())
    {
        DateTime now = rtc.now();

        reminders = getReminders();
        String datetime = String(now.day()) + "/" + String(now.month()) + "/" + String(now.year());

        if (datetime == reminders[currBox]["start_date"].as<String>() && reminderTimeIdx < reminders[currBox]["times"].as<JsonArray>().size())
        {
            StringSplitter *sp = new StringSplitter(reminders[currBox]["times"][reminderTimeIdx].as<String>(), ':', 2);

            if (now.hour() == sp->getItemAtIndex(0).toInt() && now.minute() == sp->getItemAtIndex(1).toInt() && now.second() >= 0 && now.second() <= 10)
            {
                Serial.println(F("Aktif"));
                obatState = 1;
            }

            delete sp;
        }
    }
    if (buttonState == LOW && !buttonPressed)
    {
        for (auto kv : reminders[currBox]["boxes"].as<JsonObject>())
        {
            StringSplitter *boxName = new StringSplitter(F(kv.key().c_str()), ' ', 2);
            Serial.println(LEDS[boxName->getItemAtIndex(1).toInt() - 1]);

            delete boxName;
        }
        buttonPressed = true;

        obatState = 2;
        if (reminderTimeIdx < reminders[currBox]["times"].as<JsonArray>().size())
        {
            reminderTimeIdx++;
            Serial.print(F("Pergantian waktu ke: "));
            Serial.println(String(reminderTimeIdx + 1));
        }
        else
        {
            currBox++;
            Serial.print(F("Pergantian jadwal ke: "));
            Serial.println(String(currBox + 1));
            reminderTimeIdx = 0;
        }
    }
    else if (buttonState == HIGH)
    {
        buttonPressed = false;
    }

    if (obatState == 1)
    {
        for (auto kv : reminders[currBox]["boxes"].as<JsonObject>())
        {
            StringSplitter *boxName = new StringSplitter(F(kv.key().c_str()), ' ', 2);
            digitalWrite(LEDS[boxName->getItemAtIndex(1).toInt() - 1], HIGH);

            delete boxName;
        }
    }
    else if (obatState == 2)
    {
        for (auto kv : reminders[currBox]["boxes"].as<JsonObject>())
        {
            StringSplitter *boxName = new StringSplitter(F(kv.key().c_str()), ' ', 2);
            digitalWrite(LEDS[boxName->getItemAtIndex(1).toInt() - 1], LOW);

            delete boxName;
        }
    }

    reminders.clear();
}

JsonDocument getReminders()
{
    String reminders_raw = database.get<String>(client, "/Reminders");

    DeserializationError error = deserializeJson(reminder_doc, reminders_raw);
    JsonDocument reminder_array;

    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(F(error.c_str()));
        return JsonArray();
    }

    for (auto kv : reminder_doc.as<JsonObject>())
    {
        JsonDocument _reminder_doc;
        deserializeJson(_reminder_doc, kv.value().as<String>());

        JsonObject value = _reminder_doc.as<JsonObject>();
        reminder_array.add<JsonObject>(value);
    }

    return reminder_array;
}

JsonObject getObat()
{

    Serial.println("Get JSON... ");

    String obat_raw = database.get<String>(client, "/List_Obat");

    DeserializationError error = deserializeJson(obat_doc, obat_raw);

    if (error)
    {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.c_str());
        return JsonObject();
    }

    return obat_doc.as<JsonObject>();
}