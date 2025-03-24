
/*
===================ESP32 - Invernadero=================
Developed by: Jorge Cuadrado Criado (DKNS-JCC)
Date: 24/03/2025
Version: 2.0

===================DESCRIPTION========================
El programa a continuación es un ejemplo de cómo se puede utilizar un ESP32 para enviar datos de temperatura y humedad a través de 
BLE o WiFi + Firebase.
Si se puentean los pines 12 y GND, el ESP32 enviará los datos a BLE. Si no se puentean, el ESP32 se conectará a wifi
y enviará los datos a Firebase.
======================================================

===================CONNECTIONS========================
DHT22:
    - VCC: 3.3V
    - GND: GND
    - DATA: GPIO4

MODE PIN:
    - PIN_MODE: GPIO12 -> Puenteado a GND para BLE, sin puente para WiFi

===================LIBRARIES==========================
- DHT:
    - adafruit/DHT sensor library@^1.4.6
- WiFi:
    - ESP32 included library

======================================================
*/

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <DHT.h>
#include <WiFi.h>
#include <HTTPClient.h>

#define DHT_PIN 4
#define DHT_TYPE DHT22
DHT dht(DHT_PIN, DHT_TYPE);

// Pin para selección de modo
#define PIN_MODE 12

// WiFi y Firebase (Reemplazar)
const char* ssid = "";      
const char* password = "";  
const char* firebaseHost = "";  
const char* apiKey = "API_KEY";  

void conectarWiFi();
void enviarDatosFirebase(float temperatura, float humedad);
void iniciarBLE();
void actualizarBLE(float temperatura, float humedad);

// Definir UUIDs para BLE
#define SERVICE_UUID        "12345678-1234-5678-1234-56789abcdef0"

//NO TOCAR POR EL AMOR DE DIOS
#define TEMP_CHARACTERISTIC_UUID "00002a6e-0000-1000-8000-00805f9b34fb"
#define HUM_CHARACTERISTIC_UUID "00002a6f-0000-1000-8000-00805f9b34fb"

BLECharacteristic *pTempCharacteristic;
BLECharacteristic *pHumCharacteristic;

bool modoWiFi = false;


void setup() {
    Serial.begin(115200);
    dht.begin();
    pinMode(PIN_MODE, INPUT_PULLUP);
    pinMode(2, OUTPUT);

    setCpuFrequencyMhz(80); //Ahorro de energía

    // Determinar el modo de arranque
    if (digitalRead(PIN_MODE) == HIGH) {
        modoWiFi = true;
        Serial.println("Modo WiFi + Firebase activado");
        conectarWiFi();
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
        enviarDatosFirebase(t, h);
    } else {
        actualizarBLE(t, h);
    }
}

// ------------------ Funciones WiFi y Firebase ------------------
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

void enviarDatosFirebase(float temperatura, float humedad) {
    if (WiFi.status() == WL_CONNECTED) {
        HTTPClient http;
        String url = String(firebaseHost) + "/sensores.json?auth=" + apiKey;
        String payload = "{\"temperatura\": " + String(temperatura) + ", \"humedad\": " + String(humedad) + "}";

        http.begin(url);
        http.addHeader("Content-Type", "application/json");

        int httpResponseCode = http.PUT(payload);
        Serial.print("Código de respuesta HTTP: ");
        Serial.println(httpResponseCode);
        http.end();
    } else {
        Serial.println("Error de conexión WiFi");
    }
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
