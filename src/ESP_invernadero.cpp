#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <DHT.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>

#define DHT_PIN 4
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// Pin para selección de modo
#define PIN_MODE 12

//SSID y contraseña de la red WiFi !!!!!!!!!
// Cambiar por los valores de la red WiFi a la que se conectará el ESP32
const char *ssid = "-DeZX";
const char *password = "";

//!!!!!!!!CAMBIAR ESTOS VALORES POR LOS DEL PROYECTO!!!!!!!!
#define API_KEY ""
#define DATABASE_URL ""

void conectarWiFi();
void iniciarFirebase();
void enviarDatosFirestore(float temperatura, float humedad);

void iniciarBLE();
void actualizarBLE(float temperatura, float humedad);

// Definir UUIDs para BLE
#define SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"

//NO CAMBIAR LOS SIGUIENTES UUIDs
#define TEMP_CHARACTERISTIC_UUID "00002a6e-0000-1000-8000-00805f9b34fb"
#define HUM_CHARACTERISTIC_UUID "00002a6f-0000-1000-8000-00805f9b34fb"

BLECharacteristic *pTempCharacteristic;
BLECharacteristic *pHumCharacteristic;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

bool modoWiFi = false;

void setup() {
    Serial.begin(115200);
    dht.begin();
    pinMode(PIN_MODE, INPUT_PULLUP);
    pinMode(2, OUTPUT);

    setCpuFrequencyMhz(80); // Ahorro de energía

    // Determinar el modo de arranque
    if (digitalRead(PIN_MODE) == HIGH) {
        modoWiFi = true;
        Serial.println("Modo WiFi + Firestore activado");
        conectarWiFi();
        iniciarFirebase();
    } else {
        Serial.println("Modo BLE activado");
        iniciarBLE();
    }
}

void loop() {
    delay(2000);
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    
    if (isnan(h) || isnan(t)) {
        Serial.println("Error al leer el sensor DHT22");
        return;
    }

    Serial.print("Humedad: ");
    Serial.print(h);
    Serial.print(" %\t");
    Serial.print("Temperatura: ");
    Serial.print(t);
    Serial.println(" *C");

    if (modoWiFi) {
        enviarDatosFirestore(t, h);
    } else {
        actualizarBLE(t, h);
    }
}

// ------------------ Funciones WiFi y Firestore ------------------
void conectarWiFi() {
    WiFi.begin(ssid, password);
    Serial.print("Conectando a WiFi...");
    while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        Serial.print(".");
        digitalWrite(2, HIGH);
        delay(500);
        digitalWrite(2, LOW);
    }
    Serial.println("\nConectado a WiFi");
}

void iniciarFirebase() {
    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    config.signer.test_mode = true; // Modo de prueba
    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);
    Serial.println("Firebase inicializado");
}


void enviarDatosFirestore(float temperatura, float humedad) {
    if (Firebase.ready()) {
        String path = "/sensores/temperatura";
        if (Firebase.RTDB.setFloat(&fbdo, path, temperatura)) {
            Serial.println("Temperatura enviada a Firestore: " + String(temperatura));
        } else {
            Serial.println("Error al enviar temperatura: " + String(fbdo.errorReason().c_str()));
        }

        path = "/sensores/humedad";
        if (Firebase.RTDB.setFloat(&fbdo, path, humedad)) {
            Serial.println("Humedad enviada a Firestore: " + String(humedad));
        } else {
            Serial.println("Error al enviar humedad: " + String(fbdo.errorReason().c_str()));
        }
    } else {
        Serial.println("Firebase no está listo");
    }
    delay(600000); // 10 minutos
}

// ------------------ Funciones BLE ------------------
void iniciarBLE() {
    BLEDevice::init("ESP32-Invernadero");
    BLEServer *pServer = BLEDevice::createServer();
    BLEService *pService = pServer->createService(SERVICE_UUID);

    pTempCharacteristic = pService->createCharacteristic(
        TEMP_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pTempCharacteristic->addDescriptor(new BLE2902());

    pHumCharacteristic = pService->createCharacteristic(
        HUM_CHARACTERISTIC_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
    );
    pHumCharacteristic->addDescriptor(new BLE2902());

    pService->start();
    BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    BLEDevice::startAdvertising();

    Serial.println("BLE Server iniciado");
}

void actualizarBLE(float temperatura, float humedad) {
    pTempCharacteristic->setValue(String(temperatura).c_str());
    pTempCharacteristic->notify();

    delay(1000); 

    pHumCharacteristic->setValue(String(humedad).c_str());
    pHumCharacteristic->notify();
}
