#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BLE2902.h"
#include "BLEHIDDevice.h"
#include "HIDTypes.h"
#include "HIDKeyboardTypes.h"
#include <TFT_eSPI.h>
#include "TouchCal.h"

// Screen and keyboard layout
#define KEYBOARD_Y 40
#define KEY_WIDTH 21
#define KEY_HEIGHT 35
#define KEY_SPACING 3
#define KEY_RADIUS 4
#define STATUS_HEIGHT 25
bool shiftEnabled = false;
bool numberMode = false;

// BLE setup
BLEHIDDevice* hid;
BLECharacteristic* input;
BLECharacteristic* output;
bool deviceConnected = false;

// Screen setup
TFT_eSPI tft = TFT_eSPI();
#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33
SPIClass tsSPI = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);

// Keyboard layout
const char* keys[4][10] = {
    {"Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P"},
    {"A", "S", "D", "F", "G", "H", "J", "K", "L", "BS"},  // ⌫ is backspace
    {"Z", "X", "C", "V", "B", "N", "M", ",", ".", "EN"},  // ↵ is enter
    {"^", "<", ">", "^", "↓", "SPACE", "SPACE", "SPACE", ".", "?"}  // Added arrows
};

// HID Report Map
static const uint8_t hidReportMap[] = {
    0x05, 0x01,  // Usage Page (Generic Desktop)
    0x09, 0x06,  // Usage (Keyboard)
    0xA1, 0x01,  // Collection (Application)
    0x85, 0x01,  // Report ID (1)
    0x05, 0x07,  // Usage Page (Key Codes)
    0x19, 0x00,  // Usage Minimum (0)
    0x29, 0xFF,  // Usage Maximum (255)
    0x15, 0x00,  // Logical Minimum (0)
    0x25, 0xFF,  // Logical Maximum (255)
    0x75, 0x08,  // Report Size (8)
    0x95, 0x03,  // Report Count (3)
    0x81, 0x00,  // Input (Data, Array)
    0xC0         // End Collection
};

// HID Keycodes for letters
const uint8_t KEY_A = 0x04;
const uint8_t KEY_B = 0x05;
const uint8_t KEY_C = 0x06;
const uint8_t KEY_D = 0x07;
const uint8_t KEY_E = 0x08;
const uint8_t KEY_F = 0x09;
const uint8_t KEY_G = 0x0A;
const uint8_t KEY_H = 0x0B;
const uint8_t KEY_I = 0x0C;
const uint8_t KEY_J = 0x0D;
const uint8_t KEY_K = 0x0E;
const uint8_t KEY_L = 0x0F;
const uint8_t KEY_M = 0x10;
const uint8_t KEY_N = 0x11;
const uint8_t KEY_O = 0x12;
const uint8_t KEY_P = 0x13;
const uint8_t KEY_Q = 0x14;
const uint8_t KEY_R = 0x15;
const uint8_t KEY_S = 0x16;
const uint8_t KEY_T = 0x17;
const uint8_t KEY_U = 0x18;
const uint8_t KEY_V = 0x19;
const uint8_t KEY_W = 0x1A;
const uint8_t KEY_X = 0x1B;
const uint8_t KEY_Y = 0x1C;
const uint8_t KEY_Z = 0x1D;

// Additional HID keycodes for special keys
const uint8_t KEY_ENTER = 0x28;
const uint8_t KEY_BACKSPACE = 0x2A;
const uint8_t KEY_RIGHT_ARROW = 0x4F;
const uint8_t KEY_LEFT_ARROW = 0x50;
const uint8_t KEY_DOWN_ARROW = 0x51;
const uint8_t KEY_UP_ARROW = 0x52;

// Color definitions
#define KEY_COLOR 0x6B6D        // Soft gray
#define KEY_PRESS_COLOR 0x34B2  // Light blue
#define SPECIAL_KEY_COLOR 0x4208 // Darker gray
#define TEXT_COLOR TFT_WHITE
#define STATUS_BAR_COLOR 0x0926 // Dark blue

class MyServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
        deviceConnected = true;
        Serial.println("Connected");
    };

    void onDisconnect(BLEServer* pServer) {
        deviceConnected = false;
        Serial.println("Disconnected");
    }
};

void sendKey(char key) {
    if (!deviceConnected) return;
    
    uint8_t keycode;
    uint8_t modifier = 0;
    
    // Convert ASCII character to HID keycode
    if (key >= 'A' && key <= 'Z') {
        keycode = KEY_A + (key - 'A');
    } else if (key >= 'a' && key <= 'z') {
        keycode = KEY_A + (key - 'a');
    } else {
        // Handle special characters and keys
        switch(key) {
            case ' ':
                keycode = 0x2C;  // Spacebar
                break;
            case '.':
                keycode = 0x37;  // Period
                break;
            case ',':
                keycode = 0x36;  // Comma
                break;
            case ';':
                keycode = 0x33;  // Semicolon
                break;
            case '?':
                keycode = 0x38;  // Question mark
                modifier = 0x02;  // With shift
                break;
            case '⌫':
                keycode = KEY_BACKSPACE;
                break;
            case '↵':
                keycode = KEY_ENTER;
                break;
            case '←':
                keycode = KEY_LEFT_ARROW;
                break;
            case '→':
                keycode = KEY_RIGHT_ARROW;
                break;
            case '↑':
                keycode = KEY_UP_ARROW;
                break;
            case '↓':
                keycode = KEY_DOWN_ARROW;
                break;
            default:
                return;  // Unknown character
        }
    }
    
    // For uppercase letters
    if (key >= 'A' && key <= 'Z') {
        modifier = 0x02;  // Left shift
    }
    
    // Send keypress
    uint8_t msg[] = {modifier, 0x00, keycode};
    input->setValue(msg, sizeof(msg));
    input->notify();
    
    // Release key
    uint8_t release[] = {0x00, 0x00, 0x00};
    input->setValue(release, sizeof(release));
    input->notify();
    
    delay(10);
}

void drawKeyboard() {
    tft.fillScreen(TFT_BLACK);
    
    // Draw status bar with gradient
    for(int i = 0; i < STATUS_HEIGHT; i++) {
        tft.drawFastHLine(0, i, 240, STATUS_BAR_COLOR + (i * 4));
    }
    
    // Draw status text and indicator
    tft.setTextSize(1);
    tft.setTextColor(TEXT_COLOR);
    if(deviceConnected) {
        tft.drawString("BLE Connected", 5, 8);
        tft.fillCircle(220, STATUS_HEIGHT/2, 4, TFT_GREEN);
    } else {
        tft.drawString("Waiting for Connection...", 5, 8);
        tft.fillCircle(220, STATUS_HEIGHT/2, 4, TFT_RED);
    }
    
    // Draw keyboard keys
    for (int row = 0; row < 4; row++) {
        for (int col = 0; col < 10; col++) {
            int x = col * (KEY_WIDTH + KEY_SPACING) + 4;
            int y = KEYBOARD_Y + row * (KEY_HEIGHT + KEY_SPACING);
            
            // Special handling for space bar
            if (row == 3 && col >= 5 && col <= 7) {
                if (col == 5) {
                    tft.fillRoundRect(x, y, 
                        (KEY_WIDTH + KEY_SPACING) * 3 - KEY_SPACING, 
                        KEY_HEIGHT, KEY_RADIUS, SPECIAL_KEY_COLOR);
                    tft.setTextColor(TEXT_COLOR);
                    tft.drawString("SPACE", x + ((KEY_WIDTH + KEY_SPACING) * 1.5) - 15, y + 14);
                }
                continue;
            }
            
            // Determine key color
            uint16_t keyColor = KEY_COLOR;
            if (strcmp(keys[row][col], "⌫") == 0 || 
                strcmp(keys[row][col], "↵") == 0 || 
                strcmp(keys[row][col], "⇧") == 0) {
                keyColor = SPECIAL_KEY_COLOR;
            }
            
            // Draw key with rounded corners
            tft.fillRoundRect(x, y, KEY_WIDTH, KEY_HEIGHT, KEY_RADIUS, keyColor);
            
            // Center text
            String keyLabel = keys[row][col];
            int16_t textWidth = tft.textWidth(keyLabel);
            int16_t textX = x + (KEY_WIDTH - textWidth) / 2;
            int16_t textY = y + (KEY_HEIGHT - 8) / 2;
            
            tft.setTextColor(TEXT_COLOR);
            tft.drawString(keyLabel, textX, textY);
        }
    }
}

// Update the visual feedback in handleTouch
void showKeyPress(int x_pos, int y_pos, const char* keyStr, bool isSpecial = false) {
    uint16_t originalColor = isSpecial ? SPECIAL_KEY_COLOR : KEY_COLOR;
    
    // Press effect
    tft.fillRoundRect(x_pos, y_pos, KEY_WIDTH, KEY_HEIGHT, KEY_RADIUS, KEY_PRESS_COLOR);
    tft.setTextColor(TEXT_COLOR);
    
    // Center text
    int16_t textWidth = tft.textWidth(keyStr);
    int16_t textX = x_pos + (KEY_WIDTH - textWidth) / 2;
    int16_t textY = y_pos + (KEY_HEIGHT - 8) / 2;
    tft.drawString(keyStr, textX, textY);
    
    delay(100);
    
    // Release effect
    tft.fillRoundRect(x_pos, y_pos, KEY_WIDTH, KEY_HEIGHT, KEY_RADIUS, originalColor);
    tft.drawString(keyStr, textX, textY);
}

// Update handleTouch to use the new visual feedback
void handleTouch(int16_t x, int16_t y) {
    if (y < KEYBOARD_Y) return;
    
    x = x - 4;  // Adjust for left margin
    
    int row = (y - KEYBOARD_Y) / (KEY_HEIGHT + KEY_SPACING);
    int col = x / (KEY_WIDTH + KEY_SPACING);
    
    // Handle spacebar
    if (row == 3 && col >= 5 && col <= 7) {
        sendKey(' ');
        // Visual feedback
        int x_pos = col * (KEY_WIDTH + KEY_SPACING) + 4;
        int y_pos = KEYBOARD_Y + row * (KEY_HEIGHT + KEY_SPACING);
        tft.fillRoundRect(x_pos, y_pos, 
            (KEY_WIDTH + KEY_SPACING) * 3 - KEY_SPACING, 
            KEY_HEIGHT, KEY_RADIUS, KEY_PRESS_COLOR);
        tft.drawString("SPACE", x_pos + ((KEY_WIDTH + KEY_SPACING) * 1.5) - 15, y_pos + 14);
        delay(100);
        tft.fillRoundRect(x_pos, y_pos, 
            (KEY_WIDTH + KEY_SPACING) * 3 - KEY_SPACING, 
            KEY_HEIGHT, KEY_RADIUS, SPECIAL_KEY_COLOR);
        tft.drawString("SPACE", x_pos + ((KEY_WIDTH + KEY_SPACING) * 1.5) - 15, y_pos + 14);
        return;
    }
    
    if (row < 4 && col < 10) {
        const char* keyStr = keys[row][col];
        
        // Handle shift key
        if (strcmp(keyStr, "⇧") == 0) {
            shiftEnabled = !shiftEnabled;
            int x_pos = col * (KEY_WIDTH + KEY_SPACING) + 4;
            int y_pos = KEYBOARD_Y + row * (KEY_HEIGHT + KEY_SPACING);
            tft.fillRoundRect(x_pos, y_pos, KEY_WIDTH, KEY_HEIGHT, KEY_RADIUS, 
                            shiftEnabled ? KEY_PRESS_COLOR : SPECIAL_KEY_COLOR);
            tft.drawString("⇧", x_pos + (KEY_WIDTH - tft.textWidth("⇧")) / 2, 
                          y_pos + (KEY_HEIGHT - 8) / 2);
            return;
        }
        
        // Send key and show feedback
        if (shiftEnabled && strlen(keyStr) == 1) {
            sendKey(toupper(keyStr[0]));
        } else if (strlen(keyStr) == 1) {
            sendKey(keyStr[0]);
        }
        
        // Visual feedback
        int x_pos = col * (KEY_WIDTH + KEY_SPACING) + 4;
        int y_pos = KEYBOARD_Y + row * (KEY_HEIGHT + KEY_SPACING);
        tft.fillRoundRect(x_pos, y_pos, KEY_WIDTH, KEY_HEIGHT, KEY_RADIUS, KEY_PRESS_COLOR);
        tft.drawString(keyStr, x_pos + (KEY_WIDTH - tft.textWidth(keyStr)) / 2, 
                      y_pos + (KEY_HEIGHT - 8) / 2);
        delay(100);
        tft.fillRoundRect(x_pos, y_pos, KEY_WIDTH, KEY_HEIGHT, KEY_RADIUS, 
                         (strcmp(keyStr, "⌫") == 0 || strcmp(keyStr, "↵") == 0) ? 
                         SPECIAL_KEY_COLOR : KEY_COLOR);
        tft.drawString(keyStr, x_pos + (KEY_WIDTH - tft.textWidth(keyStr)) / 2, 
                      y_pos + (KEY_HEIGHT - 8) / 2);
    }
}

void setup() {
    Serial.begin(38400);
    while (!Serial) { delay(50); }
    Serial.println("Serial.OK");

    // Set rotation before initializing
    tc::TC_ROTATION = 3;
    
    delay(10);
    Serial.println("To touch..");
    
    // Initialize touch SPI and touchscreen
    tsSPI.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
    ts.begin(tsSPI);
    ts.setRotation(tc::TC_ROTATION);

    delay(100);
    Serial.println("To TFT..");
    
    // Initialize display
    tft.begin();
    tft.setRotation(tc::TC_ROTATION);
    
    Serial.println("Starting calibration...");
    tc::calibration(&ts, &tft);
    
    // Initialize BLE after calibration
    Serial.println("Initializing BLE...");
    BLEDevice::init("ESP32 Keyboard");
    BLEServer *pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());
    
    hid = new BLEHIDDevice(pServer);
    input = hid->inputReport(1);
    output = hid->outputReport(1);
    
    hid->manufacturer()->setValue("Espressif");
    hid->pnp(0x02, 0x05ac, 0x820a, 0x0001);
    hid->hidInfo(0x00, 0x01);
    
    hid->reportMap((uint8_t*)hidReportMap, sizeof(hidReportMap));
    hid->startServices();
    
    BLEAdvertising *pAdvertising = pServer->getAdvertising();
    pAdvertising->setAppearance(HID_KEYBOARD);
    pAdvertising->addServiceUUID(hid->hidService()->getUUID());
    pAdvertising->start();
    
    // Draw keyboard after everything is initialized
    drawKeyboard();
    Serial.println("Ready!");
}

void loop() {
    TS_Point p = TS_Point();
    bool pressed = tc::isTouch(&ts, &p);
    
    if (pressed) {
        handleTouch(p.x, p.y);
        delay(50);  // Debounce
    }
}