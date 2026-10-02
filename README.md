# Mouse Jiggler para M5Stack Core2

Aplicación embebida para el M5Stack Core2 que actúa como dispositivo HID USB mouse, simulando movimiento periódico del cursor para evitar que el sistema detecte inactividad.

## Hardware requerido

- **M5Stack Core2** (ESP32-WROVER con pantalla TFT 320x240)
- **Cable USB-C** para alimentación y comunicación (también para el dispositivo HID mouse virtual)

## Estructura del proyecto

```
mouse/
├── platformio.ini          # Configuración PlatformIO
├── README.md               # Este archivo
├── src/
│   ├── main.cpp            # Punto de entrada (setup/loop)
│   ├── mouse_jiggler.h     # Declaración del módulo HID mouse
│   ├── mouse_jiggler.cpp   # Implementación del movimiento zigzag
│   ├── display_manager.h   # Declaración del módulo pantalla TFT
│   ├── display_manager.cpp # Implementación de gráficos en pantalla
│   ├── button_handler.h    # Declaración del módulo botón B
│   └── button_handler.cpp  # Implementación de detección de botón
```

## Requisitos previos

1. **Python 3.8+** instalado
2. **PlatformIO Core** instalado:

```bash
pip install platformio
```

## Instrucciones de compilación

```bash
# Navegar al directorio del proyecto
cd mouse

# Compilar el proyecto
pio run

# Subir al dispositivo
pio run --target upload

# Compilar y subir en un solo comando
pio run --target upload
```

## Instrucciones de uso

1. Conectar el M5Stack Core2 al PC mediante cable USB-C.
2. Esperar a que aparezca un nuevo dispositivo mouse en el sistema.
3. La pantalla mostrará el estado del jiggler.
4. **Botón B** (botón central del M5Stack Core2): alternar entre ON/OFF.

## Comportamiento

| Estado | Cursor | Pantalla | LED |
|--------|--------|----------|-----|
| OFF | Estático | "JIGGLER DESACTIVADO" + ○ Inactivo | Rojo |
| ON | Movimiento zigzag 5px cada 500ms | "JIGGLER ACTIVADO" + ● Activo | Verde |

## Personalización

- **Intervalo de movimiento**: `mouseJiggler.setMovementInterval(ms)` — por defecto 500ms
- **Distancia de movimiento**: `mouseJiggler.setMovementDistance(píxeles)` — por defecto 5px
- **Patrón de movimiento**: zigzag (derecha, abajo, izquierda, arriba) — configurable en `mouse_jiggler.cpp`

## Criterios de aceptación

1. ✅ Compilación exitosa con PlatformIO
2. ✅ Reconocido como dispositivo HID mouse por el sistema operativo
3. ✅ Movimiento visible del cursor en patrón zigzag
4. ✅ Estado ON/OFF mostrado en pantalla TFT
5. ✅ Control por botón B del M5Stack Core2
6. ✅ Código modular (3 módulos separados)
7. ✅ Configuración limpia en `platformio.ini`
8. ✅ Documentación completa
