#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "esp_bt.h"

#define NUMPIXELS   120
#define PIN         2

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);

volatile bool newCommand = false;
volatile bool connected = false;
String pendingCommand = "";

void applyCommand(String cmd) {
  cmd.trim();

  if (cmd.equalsIgnoreCase("OFF")) {
    pixels.clear();
    pixels.show();
    return;
  }

  if (cmd.equalsIgnoreCase("RAINBOW")) {
   for (int i = 0; i < NUMPIXELS; i++) {
      int pixelHue = (i * 256 / NUMPIXELS) & 255;
      pixels.setPixelColor(i, pixels.ColorHSV(pixelHue * 256));
    }
    pixels.show();
    return;
  }

  int commas = 0;
  for (char c : cmd) if (c == ',') commas++;

  if (commas == 1 && cmd.substring(cmd.indexOf(',') + 1).equalsIgnoreCase("ARAINBOW")) {
    int delayTime = cmd.substring(0, cmd.indexOf(',')).toInt();
    while (!newCommand && connected) {
      for (int j = 0; j < 256; j++) {
        for (int i = 0; i < NUMPIXELS; i++) {
          int pixelHue = (i * 256 / NUMPIXELS + j) & 255;
          pixels.setPixelColor(i, pixels.ColorHSV(pixelHue * 256));
        }
        pixels.show();
        delay(delayTime);
        if (newCommand || !connected) break;
      }
    }
  }

  else if (commas == 2) {
    int r = cmd.substring(0, cmd.indexOf(',')).toInt();
    cmd = cmd.substring(cmd.indexOf(',') + 1);
    int g = cmd.substring(0, cmd.indexOf(',')).toInt();
    int b = cmd.substring(cmd.indexOf(',') + 1).toInt();
    for (int i = 0; i < NUMPIXELS; i++)
      pixels.setPixelColor(i, pixels.Color(r, g, b));
    pixels.show();
  } 

  else if (commas == 3) {
    int n[NUMPIXELS];
    int compteur = 0;
    int space = 0;
    for (char c : cmd) if (c == ';') space++;
    while (space >= 0) {
      if (cmd.indexOf('-') != -1) {
        int n1 = cmd.substring(0, cmd.indexOf('-')).toInt();
        cmd = cmd.substring(cmd.indexOf('-') + 1);
        int n2;
        if (space > 0)  {
          n2 = cmd.substring(0, cmd.indexOf(';')).toInt();
          cmd = cmd.substring(cmd.indexOf(';') + 1);
        }
        else n2 = cmd.substring(0, cmd.indexOf(',')).toInt();      
        for (int i = n1; i <= n2; i++) {n[compteur] = i; compteur++;}
      }
      else {
        if (space > 0)  {
          n[compteur] = cmd.substring(0, cmd.indexOf(';')).toInt();
          compteur++;
          cmd = cmd.substring(cmd.indexOf(';') + 1);
        } 
        else {
          n[compteur] = cmd.substring(0, cmd.indexOf(',')).toInt();
          compteur++;
        }
      }
      space--;
    }
    cmd = cmd.substring(cmd.indexOf(',') + 1);

    int r = cmd.substring(0, cmd.indexOf(',')).toInt();
    cmd = cmd.substring(cmd.indexOf(',') + 1);
    int g = cmd.substring(0, cmd.indexOf(',')).toInt();
    int b = cmd.substring(cmd.indexOf(',') + 1).toInt();

    for (int i = 0; i < compteur; i++) {
      if (n[i] < NUMPIXELS) pixels.setPixelColor(n[i], pixels.Color(r, g, b));
    } 
    pixels.show();
  }
}

class LEDCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    connected = true;
    pendingCommand = String(pCharacteristic->getValue().c_str());
    newCommand = true;
  }
};

class MyServerCallbacks : public BLEServerCallbacks {
  void onDisconnect(BLEServer* pServer) {
    pixels.clear();
    pixels.show();
    connected = false;
    BLEDevice::startAdvertising();
  }
  void onConnected(BLEServer* pServer) {
    connected = true;
  }
};

void setup() {
  Serial.begin(115200);

  pixels.begin();
  pixels.clear();
  pixels.show();
  
  BLEDevice::setPower(ESP_PWR_LVL_P9);
  BLEDevice::init("LED Controller");
  BLEDevice::init("LED Controller");
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV,     ESP_PWR_LVL_P9);  // ← advertising
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN,    ESP_PWR_LVL_P9);  // ← scan response
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);  // ← connexion

  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);
  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P9);

  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic =
    pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE
    );

  pCharacteristic->setCallbacks(new LEDCallbacks());
  pCharacteristic->setValue("OFF");
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("BLE LED Controller pret.");
}

void loop() {
  if (newCommand) {
    newCommand = false;
    applyCommand(pendingCommand);
  }
  delay(10);
}