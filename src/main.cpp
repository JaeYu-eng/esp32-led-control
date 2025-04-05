#include <Arduino.h>
#include <NimBLEDevice.h>

// LED 핀 설정
const int LED_PIN = 2;  // ESP32 내장 LED
const int PWM_CHANNEL = 0;  // PWM 채널
const int PWM_FREQ = 5000;  // PWM 주파수
const int PWM_RESOLUTION = 8;  // PWM 해상도 (8비트: 0-255)

// LED 상태 변수
bool isLedOn = false;
int currentBrightness = 50;  // 기본 밝기 50%

// BLE 서비스 및 특성 UUID
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

NimBLECharacteristic* pCharacteristic = nullptr;

// BLE 서버 클래스
class MyBLEServerCallbacks: public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer) {
        Serial.println("클라이언트 연결됨");
        // 연결 직후 현재 상태 전송
        if (pCharacteristic != nullptr) {
            char statusMsg[20];
            snprintf(statusMsg, sizeof(statusMsg), "STATUS:%d:%d", isLedOn ? 1 : 0, currentBrightness);
            pCharacteristic->setValue((uint8_t*)statusMsg, strlen(statusMsg));
            pCharacteristic->notify();
            Serial.printf("초기 상태 전송: LED=%s, 밝기=%d%%\n", isLedOn ? "ON" : "OFF", currentBrightness);
        }
    }

    void onDisconnect(NimBLEServer* pServer) {
        Serial.println("클라이언트 연결 해제됨");
    }
};

// BLE 특성 콜백 클래스
class MyCharacteristicCallbacks: public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pCharacteristic) {
        std::string value = pCharacteristic->getValue();
        if (value.length() > 0) {
            Serial.print("수신된 값: ");
            Serial.println(value.c_str());

            // LED 제어
            if (value == "ON") {
                isLedOn = true;
                ledcWrite(PWM_CHANNEL, map(currentBrightness, 0, 100, 0, 255));
                Serial.printf("LED 켜짐 (밝기: %d%%)\n", currentBrightness);
                sendStatus(pCharacteristic);
            } else if (value == "OFF") {
                isLedOn = false;
                ledcWrite(PWM_CHANNEL, 0);
                Serial.println("LED 꺼짐");
                sendStatus(pCharacteristic);
            } else if (value == "STATUS") {
                sendStatus(pCharacteristic);
            } else if (value.rfind("BRIGHT:", 0) == 0) {
                currentBrightness = atoi(value.substr(7).c_str());
                if (isLedOn) {
                    int pwmValue = map(currentBrightness, 0, 100, 0, 255);
                    ledcWrite(PWM_CHANNEL, pwmValue);
                }
                Serial.printf("밝기 설정: %d%%\n", currentBrightness);
                sendStatus(pCharacteristic);
            }
        }
    }

    void sendStatus(NimBLECharacteristic *pCharacteristic) {
        char statusMsg[20];
        snprintf(statusMsg, sizeof(statusMsg), "STATUS:%d:%d", isLedOn ? 1 : 0, currentBrightness);
        pCharacteristic->setValue((uint8_t*)statusMsg, strlen(statusMsg));
        pCharacteristic->notify();
        Serial.printf("상태 전송: LED=%s, 밝기=%d%%\n", isLedOn ? "ON" : "OFF", currentBrightness);
    }
};

void setup() {
    Serial.begin(115200);
    Serial.println("BLE LED 제어 서버 시작");

    // LED PWM 설정
    ledcSetup(PWM_CHANNEL, PWM_FREQ, PWM_RESOLUTION);
    ledcAttachPin(LED_PIN, PWM_CHANNEL);
    ledcWrite(PWM_CHANNEL, 0);  // LED 초기 상태: 꺼짐

    // BLE 초기화
    NimBLEDevice::init("ESP32_LED");
    NimBLEServer *pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new MyBLEServerCallbacks());

    // BLE 서비스 생성
    NimBLEService *pService = pServer->createService(SERVICE_UUID);
    pCharacteristic = pService->createCharacteristic(
        CHARACTERISTIC_UUID,
        NIMBLE_PROPERTY::READ |
        NIMBLE_PROPERTY::WRITE |
        NIMBLE_PROPERTY::NOTIFY
    );

    pCharacteristic->setCallbacks(new MyCharacteristicCallbacks());
    pService->start();
    NimBLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->start();

    Serial.println("BLE 서버 준비 완료");
}

void loop() {
    delay(2000);
} 