#include <M5Core2.h>
#include <cstring>
#include <stdint.h>
#include "ble_mouse_keyboard.h"
#include "math.h"


// Nombre que verá el PC al buscar dispositivos Bluetooth
BleMouseKeyboard bleMouse("HID-MAGNET keyboard", "WasxAlpha Inc.", 100);

String TEXT_TO_TYPE;
String COLA_BOTONES = "";
bool awaitingPromptRelease = false;
unsigned long promptOpenedAt = 0;
unsigned long acComboStartTime = 0;
bool acComboTiming = false;
bool regenerationPrompt = false;
const unsigned long AC_COMBO_HOLD_MS = 2000;
const unsigned long PROMPT_INPUT_DELAY_MS = 5000;

const uint8_t AXP192_IRQ_ENABLE3 = 0x42;
const uint8_t AXP192_IRQ_STATUS3 = 0x46;
const uint8_t AXP192_IRQ_PEK_SHORT_PRESS = 0x02;

int16_t beepSamples[4410];  // 0,1 s a 44.100 muestras/s


bool isRunning = false;
bool screenOn = true;

// Resolución típica de un monitor Full HD (ajústala a tu pantalla)
const int SCREEN_W = 1920;
const int SCREEN_H = 1080;

// Estado del "vuelo" tipo abeja
int targetX = 0, targetY = 0;

int currentX = 0, currentY = 0;
const int STEP_MIN = 6;
const int STEP_MAX = 14;
const int STEP_DELAY_MS = 25;
bool arrived = true;

// Máquina de estados para ciclos aleatorios de pausa/movimiento (1-10 s)
enum JigglerState { STATE_PAUSE, STATE_MOVE };
JigglerState currentState = STATE_PAUSE;
unsigned long stateStartTime = 0;
unsigned long currentStateDuration = 0;

// Apagado automático de pantalla tras 10 s sin interacción
unsigned long screenOnTime = 0;
const unsigned long SCREEN_AUTO_OFF_MS = 10000;

// Debounce del toque táctil para evitar rebotes
bool touchActive = false;
unsigned long lastTouchTime = 0;
const unsigned long TOUCH_DEBOUNCE_MS = 300;

void beep() {
    for (int i = 0; i < 4410; ++i) {
        float phase = 6.2831853f * 1000.0f * i / 44100.0f;
        beepSamples[i] = (int16_t)(6000.0f * sinf(phase));
    }

    M5.Spk.PlaySound(
        reinterpret_cast<const unsigned char*>(beepSamples),
        sizeof(beepSamples)
    );
}

void updateScreen() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);

    M5.Lcd.drawCentreString("HID-MAGNET", 160, 20, 1);
    M5.Lcd.drawCentreString(COLA_BOTONES, 160, 45, 1);

    // Estado Bluetooth
    M5.Lcd.setTextSize(2);
    if (bleMouse.isConnected()) {
        M5.Lcd.setTextColor(GREEN);
        M5.Lcd.drawCentreString("BLE: Conectado", 160, 70, 1);
    } else {
        M5.Lcd.setTextColor(ORANGE);
        M5.Lcd.drawCentreString("BLE: Desconectado", 160, 70, 1);
    }

    // Botones táctiles: controlar el jiggler y escribir el texto configurado.
    M5.Lcd.fillRect(0, 105, 320, 50, isRunning ? DARKGREEN : RED);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.drawCentreString(isRunning ? "PAUSAR" : "ACTIVAR", 160, 121, 1);
    M5.Lcd.fillRect(0, 165, 320, 55, BLUE);
    M5.Lcd.drawCentreString("ESCRIBIR", 160, 183, 1);
}

void turnScreenOn() {
    if (!screenOn) {
        M5.Axp.SetDCDC3(true);
        screenOn = true;
        updateScreen();
    }
    screenOnTime = millis();
}

void turnScreenOff() {
    if (screenOn) {
        screenOn = false;
        M5.Axp.SetDCDC3(false);
    }
}

void toggleJiggler() {
    isRunning = !isRunning;
    if (isRunning) {
        // Reiniciar máquina de estados al activar
        currentState = STATE_PAUSE;
        stateStartTime = millis();
        currentStateDuration = random(1000, 10001); // 1-10 segundos
    }
}

void flyLikeABee() {
    if (arrived) {
        // Nuevo objetivo aleatorio lejos de la posición actual
        do {
            targetX = random(50, SCREEN_W - 50);
            targetY = random(50, SCREEN_H - 50);
        } while (abs(targetX - currentX) < 200 && abs(targetY - currentY) < 200);
        arrived = false;
    }

    // Paso aleatorio dentro del rango
    int step = random(STEP_MIN, STEP_MAX + 1);

    int dx = targetX - currentX;
    int dy = targetY - currentY;
    float dist = sqrt((float)dx * dx + (float)dy * dy);

    if (dist <= step) {
        // Llegamos: saltar al objetivo exacto
        bleMouse.move(targetX - currentX, targetY - currentY);
        currentX = targetX;
        currentY = targetY;
        arrived = true;
    } else {
        // Mover una fracción del camino (proporcional al paso)
        int moveX = (int)((dx / dist) * step);
        int moveY = (int)((dy / dist) * step);
        bleMouse.move(moveX, moveY);
        currentX += moveX;
        currentY += moveY;
    }

}

bool findLatestTextFile(String& latestPath, uint32_t& latestNumber) {
    File root = SD.open("/");
    if (!root || !root.isDirectory()) {
        if (root) root.close();
        Serial.println("No se pudo listar la microSD");
        return false;
    }

    latestPath = "";
    latestNumber = 0;
    File entry = root.openNextFile();
    while (entry) {
        if (!entry.isDirectory()) {
            const char* name = entry.name();
            const char* baseName = strrchr(name, '/');
            baseName = baseName ? baseName + 1 : name;
            const size_t nameLength = strlen(baseName);

            if (nameLength > 10 &&
                strncmp(baseName, "texto_", 6) == 0 &&
                strcmp(baseName + nameLength - 4, ".txt") == 0) {
                uint32_t number = 0;
                bool valid = true;
                for (size_t i = 6; i < nameLength - 4; ++i) {
                    const char digit = baseName[i];
                    if (digit < '0' || digit > '9') {
                        valid = false;
                        break;
                    }
                    const uint32_t value = digit - '0';
                    if (number > (UINT32_MAX - value) / 10) {
                        valid = false;
                        break;
                    }
                    number = number * 10 + value;
                }

                if (valid && number > 0 &&
                    (latestPath.isEmpty() || number > latestNumber)) {
                    latestNumber = number;
                    latestPath = "/";
                    latestPath += baseName;
                }
            }
        }
        entry.close();
        entry = root.openNextFile();
    }
    root.close();
    return true;
}

bool loadTextToType() {
    String textPath;
    uint32_t textNumber = 0;
    if (!findLatestTextFile(textPath, textNumber)) return false;

    if (textPath.isEmpty()) textPath = "/texto.txt";
    File textFile = SD.open(textPath.c_str(), FILE_READ);
    if (!textFile) {
        Serial.printf("No se pudo abrir %s en la microSD\n", textPath.c_str());
        return false;
    }

    TEXT_TO_TYPE = "";
    while (textFile.available()) {
        char character = static_cast<char>(textFile.read());
        if (character != '\r') TEXT_TO_TYPE += character;
    }
    textFile.close();
    return true;
}

size_t secureRandomIndex(size_t upperBound) {
    return esp_random() % upperBound;
}

bool generateAndSaveText() {
    const char* uppercase = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char* lowercase = "abcdefghijklmnopqrstuvwxyz";
    const char* digits = "0123456789";
    const char* symbols = "!@#$%&*?";
    const char* alphanumeric = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    char password[10];

    for (int i = 0; i < 4; ++i) password[i] = digits[secureRandomIndex(strlen(digits))];
    password[4] = symbols[secureRandomIndex(strlen(symbols))];
    password[5] = uppercase[secureRandomIndex(strlen(uppercase))];
    password[6] = lowercase[secureRandomIndex(strlen(lowercase))];
    for (int i = 7; i < 10; ++i) {
        password[i] = alphanumeric[secureRandomIndex(strlen(alphanumeric))];
    }

    for (int i = 9; i > 0; --i) {
        int other = secureRandomIndex(i + 1);
        char character = password[i];
        password[i] = password[other];
        password[other] = character;
    }

    String generatedText;
    for (char character : password) generatedText += character;

    String latestPath;
    uint32_t latestNumber = 0;
    if (!findLatestTextFile(latestPath, latestNumber)) return false;
    if (latestNumber == UINT32_MAX) {
        Serial.println("Se alcanzo el limite de archivos de texto secuenciales");
        return false;
    }

    char textPath[32];
    snprintf(textPath, sizeof(textPath), "/texto_%04lu.txt",
             static_cast<unsigned long>(latestNumber + 1));
    File textFile = SD.open(textPath, FILE_WRITE);
    if (!textFile) {
        Serial.printf("No se pudo crear %s en la microSD\n", textPath);
        return false;
    }
    size_t written = textFile.print(generatedText);
    textFile.close();
    if (written != generatedText.length()) {
        SD.remove(textPath);
        Serial.printf("No se pudo guardar la cadena completa en %s\n", textPath);
        return false;
    }

    TEXT_TO_TYPE = generatedText;
    Serial.printf("Cadena generada y guardada en %s\n", textPath);
    return true;
}

void setup() {
    M5.begin(true, true, true, false, kMBusModeOutput, true);
    randomSeed(esp_random());
    if (!loadTextToType()) generateAndSaveText();
    Write1Byte(AXP192_IRQ_STATUS3, 0xFF);
    Write1Byte(AXP192_IRQ_ENABLE3, Read8bit(AXP192_IRQ_ENABLE3) | AXP192_IRQ_PEK_SHORT_PRESS);
    bleMouse.begin();
    stateStartTime = millis();
    currentStateDuration = random(1000, 10001); // 1-10 segundos
    screenOnTime = millis();
    updateScreen();
}

void loop() {
    M5.update();

    if (Read8bit(AXP192_IRQ_STATUS3) & AXP192_IRQ_PEK_SHORT_PRESS) {
        Write1Byte(AXP192_IRQ_STATUS3, AXP192_IRQ_PEK_SHORT_PRESS);
        M5.shutdown();
        while (true) delay(1000);
    }

    bool buttonAPressed = M5.BtnA.wasPressed();
    bool buttonBPressed = M5.BtnB.wasPressed();
    bool buttonCPressed = M5.BtnC.wasPressed();

    // Detección de toque en pantalla con debounce
    bool touchDetected = false;
    TouchPoint tp = M5.Touch.getPressPoint();
    if (tp.x >= 0 && tp.y >= 0) {
        if (!touchActive && (millis() - lastTouchTime > TOUCH_DEBOUNCE_MS)) {
            touchDetected = true;
            touchActive = true;
            lastTouchTime = millis();
        }
    } else {
        touchActive = false;
    }

    if (touchDetected || buttonAPressed || buttonBPressed || buttonCPressed) {
        turnScreenOn();
    }

    bool boton_mouse = touchDetected && tp.y >= 105 && tp.y < 155 && tp.x >= 0 && tp.x < 320;
    bool boton_teclado = touchDetected && tp.y >= 165 && tp.y < 220 && tp.x >= 0 && tp.x < 320;

    if (boton_mouse) {
        toggleJiggler();
        updateScreen();beep();
        COLA_BOTONES = "";
    }

    if (boton_teclado && COLA_BOTONES == "CBA") {
        COLA_BOTONES = TEXT_TO_TYPE;
        bleMouse.typeText(TEXT_TO_TYPE.c_str());
        updateScreen();beep();
        COLA_BOTONES = "";
    }
    
    if (buttonAPressed ) { COLA_BOTONES += "A"; updateScreen();beep(); }
    if (buttonBPressed ) { COLA_BOTONES += "B"; updateScreen();beep(); }
    if (buttonCPressed ) { COLA_BOTONES += "C"; updateScreen();beep(); }
    
    if (COLA_BOTONES.length() > 4) {
        if (COLA_BOTONES == "ABCBA") {
            // Acción a realizar cuando se ingresa la secuencia correcta
            beep();
            if (generateAndSaveText()) {
                regenerationPrompt = true;
                updateScreen();
            }
        } else {
            // Secuencia incorrecta, reiniciar la cola
            beep();beep();
        }
        COLA_BOTONES = "";
    }

    // Apagado automático de pantalla tras 10 s sin interacción
    if (screenOn && (millis() - screenOnTime > SCREEN_AUTO_OFF_MS)) {
        turnScreenOff();
    }

    // Máquina de estados: alternar pausa/movimiento con duraciones aleatorias
    if (isRunning && bleMouse.isConnected()) {
        unsigned long now = millis();
        if (now - stateStartTime >= currentStateDuration) {
            // Cambiar de estado
            if (currentState == STATE_PAUSE) {
                currentState = STATE_MOVE;
            } else {
                currentState = STATE_PAUSE;
                arrived = true; // Nuevo objetivo al volver a moverse
            }
            stateStartTime = now;
            currentStateDuration = random(1000, 10001); // 1-10 segundos
            if (screenOn) updateScreen();
        }

        if (currentState == STATE_MOVE) {
            flyLikeABee();
        }
    }

    // Refrescar estado si cambia la conexión Bluetooth
    static bool lastConnState = false;
    if (bleMouse.isConnected() != lastConnState) {
        lastConnState = bleMouse.isConnected();
        if (screenOn) updateScreen();
    }

    delay(20);
}