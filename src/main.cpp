#include <M5Core2.h>
#include <BleMouse.h>

// Nombre que verá el PC al buscar dispositivos Bluetooth
BleMouse bleMouse("Dell BT Laser Mouse", "Dell Inc.", 100);

bool isRunning = true;
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

void updateScreen() {
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.drawCentreString("BT MOUSE JIGGLER", 160, 20, 1);

    // Estado Bluetooth
    M5.Lcd.setTextSize(2);
    if (bleMouse.isConnected()) {
        M5.Lcd.setTextColor(GREEN);
        M5.Lcd.drawCentreString("BLE: Conectado", 160, 70, 1);
    } else {
        M5.Lcd.setTextColor(ORANGE);
        M5.Lcd.drawCentreString("BLE: Emparejando...", 160, 70, 1);
    }

    // Estado del jiggler (pausa / movimiento / desactivado)
    M5.Lcd.setTextSize(2);
    if (isRunning) {
        M5.Lcd.setTextColor(currentState == STATE_MOVE ? GREEN : YELLOW);
        M5.Lcd.drawCentreString(currentState == STATE_MOVE ? "MOVIENDO" : "EN PAUSA", 160, 120, 1);
    } else {
        M5.Lcd.setTextColor(RED);
        M5.Lcd.drawCentreString("DESACTIVADO", 160, 120, 1);
    }

    // Botón único de ancho completo: Activar / Desactivar
    M5.Lcd.fillRect(15, 170, 290, 50, isRunning ? DARKGREEN : RED);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.drawCentreString(isRunning ? "ACTIVO - Toca para pausar" : "PAUSA - Toca para activar", 160, 185, 1);
}

void turnScreenOn() {
    if (!screenOn) {
        screenOn = true;
        M5.Axp.SetLcdVoltage(2800);
        updateScreen();
    }
    screenOnTime = millis();
}

void turnScreenOff() {
    if (screenOn) {
        screenOn = false;
        M5.Axp.SetLcdVoltage(0);
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
    if (screenOn) updateScreen();
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

    // Estela visual en el LCD del M5
    if (screenOn) {
        int lcdX = map(currentX, 0, SCREEN_W, 0, 320);
        int lcdY = map(currentY, 0, SCREEN_H, 0, 240);
        M5.Lcd.fillCircle(lcdX, lcdY, 5, YELLOW);
        M5.Lcd.drawCircle(lcdX, lcdY, 8, ORANGE);
        delay(STEP_DELAY_MS);
        M5.Lcd.fillCircle(lcdX, lcdY, 5, BLACK);
        M5.Lcd.drawCircle(lcdX, lcdY, 8, BLACK);
    } else {
        delay(STEP_DELAY_MS);
    }
}

void setup() {
    M5.begin();
    bleMouse.begin();
    randomSeed(esp_random());
    stateStartTime = millis();
    currentStateDuration = random(1000, 10001); // 1-10 segundos
    screenOnTime = millis();
    updateScreen();
}

void loop() {
    M5.update();

    // Detección de cualquier botón físico (A, B o C)
    bool buttonPressed = M5.BtnA.wasPressed() || M5.BtnB.wasPressed() || M5.BtnC.wasPressed();

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

    // Cualquier entrada: encender pantalla y alternar jiggler
    if (buttonPressed || touchDetected) {
        turnScreenOn();
        toggleJiggler();
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