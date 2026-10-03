# Ping Point

Firmware para un sistema interactivo de entrenamiento de puntería en ping-pong. El proyecto utiliza una placa ESP32 para detectar impactos, indicar objetivos mediante una tira de LEDs y mostrar el estado de la partida en una pantalla LCD. Al finalizar, registra el puntaje y el modo de juego en Firebase Realtime Database.

Los datos almacenados en Firebase serán leídos por una aplicación móvil desarrollada en Flutter. Esta aplicación no forma parte de este repositorio.

## Funcionalidades

- Selección de modo por tiempo (60 segundos) o por intentos (10 intentos iniciales).
- Dos niveles de juego: objetivos en la mesa y una secuencia que incluye sensores posteriores.
- Detección de impactos mediante 12 sensores digitales.
- Señalización de objetivos y resultados con una tira de 360 LEDs WS2812/WS2812B.
- Visualización del modo, nivel, puntaje y tiempo o intentos restantes en una LCD I2C de 20 × 4.
- Conexión Wi-Fi con credenciales guardadas en EEPROM y configuración de red mediante un punto de acceso del ESP32 cuando no hay una red guardada disponible.
- Validación del usuario activo y registro de resultados en Firebase Realtime Database.

## Hardware

| Componente | Conexión en el ESP32 | Detalle |
| --- | --- | --- |
| Tira LED WS2812/WS2812B | GPIO 12 | 360 LEDs, organizados en 12 secciones de 30 |
| Sensores de mesa | GPIO 14, 27, 26, 25, 33, 32, 23, 19 | 8 entradas con `INPUT_PULLDOWN` |
| Sensores posteriores | GPIO 18, 5, 4, 13 | 4 entradas con `INPUT_PULLDOWN` |
| Selector de modo | GPIO 34 | Entrada digital |
| Selector de nivel | GPIO 35 | Entrada digital |
| Botón de selección | GPIO 15 | Confirma las selecciones |
| LCD I2C | Bus I2C | Dirección `0x27`, formato 20 × 4 |

El cableado, la fuente de alimentación y los niveles eléctricos deben ser compatibles con la placa y los componentes utilizados. En particular, la tira LED requiere una alimentación dimensionada para su consumo y una referencia de tierra común con el ESP32.

## Requisitos

- PlatformIO (por ejemplo, la extensión de PlatformIO IDE para VS Code).
- Placa compatible con el entorno `featheresp32` definido en `platformio.ini`.
- Framework Arduino para ESP32.
- Cuenta y proyecto de Firebase con Realtime Database habilitada.
- Aplicación móvil Flutter que gestione el usuario activo y lea los resultados de Firebase.

Las dependencias de firmware se declaran en `platformio.ini` y PlatformIO las instala al compilar:

- FastLED
- LiquidCrystal_I2C
- Firebase ESP32 Client
- ArduinoJson

## Compilar y cargar

Desde la raíz del proyecto, con PlatformIO CLI instalado:

```bash
pio run
pio run --target upload
pio device monitor --baud 115200
```

También se puede abrir la carpeta del proyecto en VS Code y ejecutar las tareas equivalentes desde PlatformIO. Ajusta el puerto de carga según el sistema y la placa conectada.

## Configuración

Antes de conectar el firmware a Firebase, configura en `src/main.cpp` los valores reales de `API_KEY`, `DATABASE_URL`, `USER_EMAIL` y `USER_PASSWORD`. Actualmente contienen valores de ejemplo, por lo que deben sustituirse para que la autenticación funcione.

No publiques credenciales reales en el repositorio. Para un despliegue compartido, utiliza un mecanismo de configuración que mantenga los secretos fuera del control de versiones y aplica reglas de seguridad de Firebase con acceso mínimo necesario.

Al iniciar, el ESP32 intenta conectarse a las redes almacenadas en EEPROM. Si no lo consigue, crea un punto de acceso llamado `PING_POINT`; conéctate a esa red y abre `192.168.4.1` para enviar las credenciales de Wi-Fi. El nombre y la contraseña del punto de acceso están definidos en `src/main.cpp`; cámbialos antes de desplegar el dispositivo. Evita introducir contraseñas reales mientras el firmware las imprime por el puerto serie.

## Integración con Firebase y Flutter

El firmware busca usuarios en `/users` y espera encontrar un registro cuyo campo `jugando` sea `true`. La aplicación Flutter debe marcar al usuario seleccionado como activo antes de iniciar una partida. El firmware toma su identificador y nombre, y al terminar escribe el resultado en su registro.

Estructura de datos utilizada por el firmware:

```text
/users/{userId}
  nombre: "Nombre del usuario"
  jugando: false
  puntajes/
    0: 8
    1: 12
  modos/
    0: "intentos"
    1: "tiempo"
```

Cada partida agrega el puntaje en `puntajes/{indice}` y el modo correspondiente en `modos/{indice}`. Ambos índices se relacionan: la aplicación Flutter puede consultar esas rutas para mostrar el historial de partidas del usuario. Al guardar el resultado, el firmware establece `jugando` en `false`.

Este proyecto solo contiene el firmware del dispositivo. La autenticación móvil, la interfaz Flutter, las reglas de acceso y la lectura de estos datos deben implementarse y configurarse en sus respectivos componentes.

## Estructura del proyecto

```text
Ping_Point/
├── include/       # Headers del proyecto
├── lib/           # Librerías locales
├── src/
│   └── main.cpp   # Firmware principal
├── test/          # Pruebas de PlatformIO
└── platformio.ini # Placa, framework y dependencias
```

## Estado de las pruebas

El directorio `test/` está preparado para pruebas de PlatformIO, pero actualmente no contiene pruebas automatizadas del firmware.