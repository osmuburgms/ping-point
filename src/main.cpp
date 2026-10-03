#include <Arduino.h>
#include <FastLED.h>                 // Librería para la tira LED WS2812b
#include <Wire.h>
#include <LiquidCrystal_I2C.h>      // Librería para pantalla LCD I2C
#include <FirebaseESP32.h>          // Librería para Firebase en ESP32
#include <addons/TokenHelper.h>     // Librería para manejar tokens de Firebase
#include <addons/RTDBHelper.h>      // Librería para manejar la base de datos en tiempo real de Firebase
#include <ArduinoJson.h>            // Librería para manejar JSON en Arduino
#include <WiFi.h>
#include <WebServer.h>
#include <EEPROM.h>

#ifdef ARDUINO_ARCH_ESP32
#include <WiFi.h>
#else
#include <ESP8266WiFi.h>
#endif
#include <Wire.h>
#include "WiFi.h"

WebServer server(80);

// Configuración de LEDs
#define PIN_LED          12         // Pin de datos para la tira LED en la ESP32
#define NUMERO_LEDS      360
#define LEDS_POR_SECCION 30         // LEDs por sección
CRGB leds[NUMERO_LEDS];             // Arreglo de LEDs para FastLED

// Configuración de Sensores
#define NUMERO_SENSORES_MESA 8
#define NUMERO_SENSORES_POSTERIOR 4

// Pines de los sensores del nivel 1 y nivel 2 en la ESP32 (evitando GPIO 21 y 22)
const int sensoresMesa[NUMERO_SENSORES_MESA] = {14, 27, 26, 25, 33, 32, 23, 19};
const int sensoresPosterior[NUMERO_SENSORES_POSTERIOR] = {18, 5, 4, 13};

// Pines para los switches de modo y nivel en la ESP32
#define PIN_SWITCH_MODO_JUEGO 34     // Switch para elegir entre modo de tiempo e intentos
#define PIN_SWITCH_NIVEL      35     // Switch para seleccionar nivel

// Pin para el botón de confirmación de selección en la ESP32
#define PIN_BOTON_SELECCION 15       // Botón para confirmar elección

// Configuración de LCD
LiquidCrystal_I2C lcd(0x27, 20, 4); // Dirección I2C de la LCD, 20x4

// Variables del juego
int puntaje = 0;
bool aciertoNivel1 = false;          // Estado para verificar si el nivel 1 ha sido acertado
int objetivoActivoMesa = -1;
int objetivoActivoPosterior = -1;
enum ModoJuego { MODO_TIEMPO, MODO_INTENTOS };
enum NivelJuego { NIVEL_1, NIVEL_2 };
ModoJuego modoActual = MODO_INTENTOS; // Modo predeterminado es por intentos
NivelJuego nivelActual = NIVEL_1;
int intentosRestantes = 10;          // Inicial para modo de intentos
unsigned long tiempoInicio;
const unsigned long LIMITE_TIEMPO = 60000; // 60 segundos
unsigned long ultimaActualizacionTiempo = 0; // Variable para controlar la actualización de tiempo en la pantalla
int validarSensor = 0; //Variable para validar que se presione una ves cada sensor
bool impacto1 = false;
bool impacto2 = false;
String userIdWithTrue = "";  // Variable para guardar el ID del usuario con 'jugando' en true
String userName_WithTrue = "";  // Variable para almacenar el nombre de usuario con 'jugando' en true

// Definición de constantes para tiempos de debounce
const unsigned long DEBOUNCE_DELAY = 50;

// Variables para el control de antirebote
unsigned long lastDebounceTimeModoJuego = 0;
unsigned long lastDebounceTimeNivel = 0;

bool lastButtonStateModoJuego = LOW;
bool lastButtonStateNivel = LOW;

// Define Firebase Data object
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
#define API_KEY "api_key_firebase"
#define DATABASE_URL "database_url_firebase"
#define USER_EMAIL "email_firebase"
#define USER_PASSWORD "password_firebase"

bool validarJugandoEnFirebase();
void guardarPuntajeEnFirebase();
void iniciarJuego();
void seleccionarNuevosObjetivos();
void actualizarLEDs();
void verificarImpactosSensores();
void configurarColorSeccion(int seccion, CRGB color);
void actualizarLCD();
void finalizarJuego();

String leerStringDeEEPROM(int direccion)
{
    String cadena = "";
    char caracter = EEPROM.read(direccion);
    int i = 0;
    while (caracter != '\0' && i < 100)
    {
        cadena += caracter;
        i++;
        caracter = EEPROM.read(direccion + i);
    }
    return cadena;
}

void escribirStringEnEEPROM(int direccion, String cadena)
{
    int longitudCadena = cadena.length();
    for (int i = 0; i < longitudCadena; i++)
    {
        EEPROM.write(direccion + i, cadena[i]);
    }
    EEPROM.write(direccion + longitudCadena, '\0'); // Null-terminated string
    EEPROM.commit();                                // Guardamos los cambios en la memoria EEPROM
}

void handleRoot()
{
    String html = "<html><body>";
    html += "<form method='POST' action='/wifi'>";
    html += "Red Wi-Fi: <input type='text' name='ssid'><br>";
    html += "Contraseña: <input type='password' name='password'><br>";
    html += "<input type='submit' value='Conectar'>";
    html += "</form></body></html>";
    server.send(200, "text/html", html);
}

int posW = 50;
void handleWifi()
{
    String ssid = server.arg("ssid");
    String password = server.arg("password");
    Serial.print("Conectando a la red Wi-Fi ");
    Serial.println(ssid);
    Serial.print("Clave Wi-Fi ");
    Serial.println(password);
    Serial.print("...");
    WiFi.disconnect(); // Desconectar la red Wi-Fi anterior, si se estaba conectado
    WiFi.begin(ssid.c_str(), password.c_str(), 6);

    int cnt = 0;
    while (WiFi.status() != WL_CONNECTED and cnt < 8)
    {
        delay(1000);
        Serial.print(".");
        cnt++;
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        // guardar en memoria eeprom la ultima red conectada

        Serial.print("Guardando en memoria eeprom...");
/*        if (posW == 0)
            posW = 50;
        else
            posW = 0;*/
        String varsave = leerStringDeEEPROM(300);
        if (varsave == "a") {
            posW = 0;
            escribirStringEnEEPROM(300, "b");
        }
        else{
            posW=50;
            escribirStringEnEEPROM(300, "a");
        }
        escribirStringEnEEPROM(0 + posW, ssid);
        escribirStringEnEEPROM(100 + posW, password);
        // guardar en memoria eeprom la ultima red conectada

        Serial.println("Conexión establecida");
        server.send(200, "text/plain", "Conexión establecida");
    }
    else
    {
        Serial.println("Conexión no establecida");
        server.send(200, "text/plain", "Conexión no establecida");
    }
}

bool lastRed()
{ // verifica si una de las 2 redes guardadas en la memoria eeprom se encuentra disponible
    // para conectarse en ese momento
    for (int psW = 0; psW <= 50; psW += 50)
    {
        String usu = leerStringDeEEPROM(0 + psW);
        String cla = leerStringDeEEPROM(100 + psW);
        lcd.setCursor(0, 1);
        lcd.print(usu);
        Serial.println(usu);
        Serial.println(cla);
        WiFi.disconnect();
        WiFi.begin(usu.c_str(), cla.c_str(), 6);
        int cnt = 0;
        while (WiFi.status() != WL_CONNECTED and cnt < 5)
        {
            delay(1000);
            Serial.print(".");
            cnt++;
        }
        if (WiFi.status() == WL_CONNECTED){
            Serial.println("Conectado a Red Wifi");
            Serial.println(WiFi.localIP());
            break;
        }
    }
    if (WiFi.status() == WL_CONNECTED)
        return true;
    else
        return false;
}

void initAP(const char *apSsid, const char *apPassword)
{ // Nombre de la red Wi-Fi y  Contraseña creada por el ESP32
    Serial.begin(115200);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(apSsid, apPassword);

    server.on("/", handleRoot);
    server.on("/wifi", handleWifi);

    server.begin();
    Serial.println("Servidor web iniciado");
}

void loopAP()
{

    server.handleClient();
}

void intentoconexion(const char *apname, const char *appassword)
{
    Serial.begin(115200);
    EEPROM.begin(512);
    Serial.println("ingreso a intentoconexion");

    // Mostrar mensaje de conexión en la LCD
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Conectando a WiFi...");

    if (!lastRed()) {  // Si no se conecta a ninguna red guardada
        Serial.println("Conectarse desde su celular a la red creada");
        Serial.println("en el navegador colocar la ip:");
        Serial.println("192.168.4.1");

        // Mostrar mensaje si no se conectó
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("WiFi no conectado");
        lcd.setCursor(0, 1);
        lcd.print("IP: 192.168.4.1");

        // Crear el AP
        initAP(apname, appassword);
    }

    while (WiFi.status() != WL_CONNECTED) { // Mientras no se conecte
        loopAP(); // Mantiene activo el servidor web
    }
}


void setup() {
  Serial.begin(115200);

  // Configuración de pines para los sensores como entradas
  for (int i = 0; i < NUMERO_SENSORES_MESA; i++) {
    pinMode(sensoresMesa[i], INPUT_PULLDOWN);
  }
  for (int i = 0; i < NUMERO_SENSORES_POSTERIOR; i++) {
    pinMode(sensoresPosterior[i], INPUT_PULLDOWN);
  }

  // Configuración de pines de switches y botón de confirmación
  pinMode(PIN_SWITCH_MODO_JUEGO, INPUT);
  pinMode(PIN_SWITCH_NIVEL, INPUT);
  pinMode(PIN_BOTON_SELECCION, INPUT);

  // Configuración de LEDs con FastLED
  FastLED.addLeds<WS2812, PIN_LED, GRB>(leds, NUMERO_LEDS);
  FastLED.clear();  // Inicializa todos los LEDs apagados
  FastLED.show();

  // Inicialización de la LCD
  lcd.init();
  lcd.backlight();
  // Conectar a WiFi usando la librería "apwifieeprom.h"
  intentoconexion("PING_POINT", "12345678");  // Pasar LCD a la función
  lcd.setCursor(0, 0);
  lcd.print("Sistema entrenamiento");
  lcd.setCursor(0, 1);
  lcd.print(" Punteria Ping Pong ");
  delay(2000);
  lcd.clear();

  config.api_key = API_KEY;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  Firebase.reconnectWiFi(true);
  Firebase.begin(&config, &auth);

  // Validar estado 'jugando' en Firebase
  while (validarJugandoEnFirebase() == false) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Nadie jugando...");
    lcd.setCursor(0, 1);
    lcd.print("Intente luego");
  }

  // Inicialización del juego
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Juego iniciado...");
  delay(2000);
  iniciarJuego();
}

void loop() {
  unsigned long tiempoActual = millis();

  // Verificar condiciones de fin de juego
  if (modoActual == MODO_TIEMPO && (tiempoActual - tiempoInicio >= LIMITE_TIEMPO)) {
    finalizarJuego();
    return;
  }

  if (modoActual == MODO_INTENTOS && intentosRestantes <= 0) {
    finalizarJuego();
    return;
  }

  // Actualizar el tiempo restante en la pantalla LCD cada segundo en modo de tiempo
  if (modoActual == MODO_TIEMPO && (tiempoActual - ultimaActualizacionTiempo >= 1000)) {
    ultimaActualizacionTiempo = tiempoActual; // Actualiza el último tiempo de actualización
    actualizarLCD();                         // Llamada para actualizar el tiempo en pantalla
  }

  // Chequeo de impactos en los sensores
  verificarImpactosSensores();
}

bool validarJugandoEnFirebase() {
  bool juegoPermitido = false;      // Variable para controlar si se permite jugar
  String path = "/users";           // Ruta donde están almacenados los usuarios en Firebase

  // Obtener los datos de todos los usuarios
  if (Firebase.getJSON(fbdo, path)) {
    FirebaseJson& json = fbdo.jsonObject();
    size_t len = json.iteratorBegin();

    for (size_t i = 0; i < len; i++) {
      String key, value;
      int type;
      json.iteratorGet(i, type, key, value);

      FirebaseJsonData data;
      FirebaseJsonData username;

      // Accede a la variable 'jugando' y 'nombre' del usuario
      json.get(data, key + "/jugando");
      json.get(username, key + "/nombre");

      if (data.success && data.boolValue) {
        juegoPermitido = true;              // Al menos un usuario tiene 'jugando' en true
        userIdWithTrue = key;               // Guarda el ID del usuario
        if (username.success) {
          userName_WithTrue = username.stringValue; // Guarda el nombre del usuario
        }
        break; // Detener la búsqueda después de encontrar el primer usuario con 'jugando' en true
      }
    }

    json.iteratorEnd();
  }

  // Si no se encontró un usuario con 'jugando' en true
  if (!juegoPermitido) {
    userIdWithTrue = "";       // Establece userIdWithTrue en vacío
    userName_WithTrue = "";    // Establece userName_WithTrue en vacío
  }

  return juegoPermitido;
}



void guardarPuntajeEnFirebase() {
  String userPath = "/users";  // Ruta principal donde están los usuarios

  // Si se encontró un usuario con 'jugando' == true
  if (userIdWithTrue != "") {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Guardando.......");
    lcd.setCursor(0, 1);
    lcd.print("Puntaje y modo");
    lcd.setCursor(0, 2);
    lcd.print("Usuario:");
    lcd.setCursor(0, 3);
    lcd.print(userName_WithTrue);

    String pathBase = "/users/" + userIdWithTrue;  // Ruta del usuario
    int siguienteClave = 0;

    // Obtener la cantidad actual de puntajes
    if (Firebase.getInt(fbdo, pathBase + "/puntajes")) {
      FirebaseJsonArray array = fbdo.jsonArray();
      siguienteClave = array.size();
    }

    // Guardar el puntaje y modo
    Firebase.setInt(fbdo, pathBase + "/puntajes/" + String(siguienteClave), puntaje);
    String modoJuego = (modoActual == MODO_TIEMPO) ? "tiempo" : "intentos";
    Firebase.setString(fbdo, pathBase + "/modos/" + String(siguienteClave), modoJuego);
    Firebase.setBool(fbdo, pathBase + "/jugando", false);
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Puntaje y modo");
    lcd.setCursor(0, 2);
    lcd.print("guardados en la APP");
  } else {
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("No se encontró ningún");
    lcd.setCursor(0, 2);
    lcd.print("usuario jugando");
  }
  delay(2000);
}

void iniciarJuego() {
  lcd.setCursor(0, 1);
  lcd.print(" Intentos<  Tiempo   ");
  // Selección de modo de juego y nivel antes de iniciar
  while (digitalRead(PIN_BOTON_SELECCION) == LOW) {
    lcd.setCursor(0, 0);
    lcd.print("Elija modo de juego:");
    lcd.setCursor(0, 1);
    bool currentButtonStateModoJuego = digitalRead(PIN_SWITCH_MODO_JUEGO);
    if (currentButtonStateModoJuego != lastButtonStateModoJuego) {
      lastDebounceTimeModoJuego = millis(); // Se detectó un cambio, reiniciamos el temporizador
    }

    if ((millis() - lastDebounceTimeModoJuego) > DEBOUNCE_DELAY) {
      // Si el estado es estable después del retraso, procesamos el cambio
      if (currentButtonStateModoJuego == HIGH) {
        if (modoActual == MODO_TIEMPO) {
          modoActual = MODO_INTENTOS;
        } else {
          modoActual = MODO_TIEMPO;
        }
        lcd.print(modoActual == MODO_TIEMPO ? " Intentos   Tiempo<  " : " Intentos<  Tiempo   ");
      }
    }
    lastButtonStateModoJuego = currentButtonStateModoJuego; // Guardar el estado del botón
  }

  while (digitalRead(PIN_BOTON_SELECCION) == HIGH); // Espera a que se libere el botón
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print(" NIVEL 1<  NIVEL 2  ");
  while (digitalRead(PIN_BOTON_SELECCION) == LOW) {
    lcd.setCursor(0, 0);
    lcd.print("Elija nivel(s):");
    lcd.setCursor(0, 1);
    bool currentButtonStateNivel = digitalRead(PIN_SWITCH_NIVEL);
    if (currentButtonStateNivel != lastButtonStateNivel) {
      lastDebounceTimeNivel = millis(); // Se detectó un cambio, reiniciamos el temporizador
    }

    if ((millis() - lastDebounceTimeNivel) > DEBOUNCE_DELAY) {
      // Si el estado es estable después del retraso, procesamos el cambio
      if (currentButtonStateNivel == HIGH) {
        if (nivelActual == NIVEL_2) {
          nivelActual = NIVEL_1;
        } else {
          nivelActual = NIVEL_2;
        }
        lcd.print(nivelActual == NIVEL_1 ? " NIVEL 1<  NIVEL 2  " : " NIVEL 1   NIVEL 2< ");
      }
    }
    lastButtonStateNivel = currentButtonStateNivel; // Guardar el estado del botón
  }

  lcd.clear();

  puntaje = 0;
  intentosRestantes = 10;
  aciertoNivel1 = false;  // Reiniciar estado del nivel 1
  tiempoInicio = millis();
  ultimaActualizacionTiempo = millis(); // Reiniciar la variable de actualización de tiempo
  seleccionarNuevosObjetivos();
  actualizarLCD();  // Mostrar puntaje inicial en LCD
}

void seleccionarNuevosObjetivos() {
  int vector0[4] = {0, 1, 6, 7};
  int vector1[6] = {0, 1, 2, 5, 6, 7};
  int vector2[6] = {1, 2, 3, 4, 5, 6};
  int vector3[4] = {2, 3, 4, 5};

  unsigned long tiempoInicio = millis();

  // Mostrar secciones aleatorias durante 5 segundos
  while (millis() - tiempoInicio < 5000) {
    int seccionAleatoriaMesa = random(0, NUMERO_SENSORES_MESA);
    int seccionAleatoriaPosterior = random(0, NUMERO_SENSORES_POSTERIOR);

    // Iluminar aleatoriamente las secciones de la mesa y posteriores
    FastLED.clear();
    configurarColorSeccion(seccionAleatoriaMesa, CRGB::Blue);
    if (nivelActual == NIVEL_2) {
      configurarColorSeccion(NUMERO_SENSORES_MESA + seccionAleatoriaPosterior, CRGB::Blue);
    }
    FastLED.show();
    delay(200);  // Breve pausa para dar el efecto de cambio
  }

  // Seleccionar objetivos finales
  if (nivelActual == NIVEL_1) {
    // Generar objetivo aleatorio para el Nivel 1
    objetivoActivoMesa = random(0, NUMERO_SENSORES_MESA);
    objetivoActivoPosterior = -1;  // No usar Nivel 2
  }

  if (nivelActual == NIVEL_2) {
    // Generar objetivo aleatorio para el Nivel 2
    objetivoActivoPosterior = random(0, NUMERO_SENSORES_POSTERIOR);
    // Generar objetivo aleatorio para el Nivel 1
    if (objetivoActivoPosterior == 0) objetivoActivoMesa = vector0[random(0, NUMERO_SENSORES_POSTERIOR)];
    if (objetivoActivoPosterior == 1) objetivoActivoMesa = vector1[random(0, NUMERO_SENSORES_POSTERIOR + 2)];
    if (objetivoActivoPosterior == 2) objetivoActivoMesa = vector2[random(0, NUMERO_SENSORES_POSTERIOR + 2)];
    if (objetivoActivoPosterior == 3) objetivoActivoMesa = vector3[random(0, NUMERO_SENSORES_POSTERIOR)];
  }

  // Actualizar LEDs con los objetivos finales
  actualizarLEDs();
}


void actualizarLEDs() {
  FastLED.clear();
  // Ilumina el sensor objetivo en azul para Nivel 1
  configurarColorSeccion(objetivoActivoMesa, CRGB::Blue);
  
  // Ilumina el sensor objetivo en azul para Nivel 2 si está en Nivel 2
  if (nivelActual == NIVEL_2 && objetivoActivoPosterior != -1) {
    configurarColorSeccion(NUMERO_SENSORES_MESA + objetivoActivoPosterior, CRGB::Blue);
  }
  
  FastLED.show();
  validarSensor = 0;
}

void verificarImpactosSensores() {
  // Nivel 1: Verificación simple
  for (int i = 0; i < NUMERO_SENSORES_MESA; i++) {
    if (digitalRead(sensoresMesa[i]) == HIGH) {
      impacto1 = true;
    }
    if (impacto1 == true) {
      if (++validarSensor == 1) {
        if (i == objetivoActivoMesa) {
          if (nivelActual == NIVEL_1) puntaje++;
          if (nivelActual == NIVEL_2) aciertoNivel1 = true;  // Marca el objetivo del nivel 1 como acertado
          configurarColorSeccion(i, CRGB::Lime);  // Verde para acierto
        } else {
          puntaje--;
          if(puntaje < 0) puntaje = 0;
          if (nivelActual == NIVEL_2) aciertoNivel1 = false;  // Marca el objetivo del nivel 1 como errado
          configurarColorSeccion(i, CRGB::Red);  // Rojo para error
        }
        if ((modoActual == MODO_INTENTOS && nivelActual == NIVEL_1) || (nivelActual == NIVEL_2 && !aciertoNivel1)) intentosRestantes--;
        actualizarLCD();  // Actualizar puntaje en LCD inmediatamente
        FastLED.show();
        delay(300);  // Muestra el color por un corto tiempo
        impacto1 = false;
        /* Ilumina un nuevo objetivo en azul en caso de estar en el nivel 1
            o si está en el nivel 2 si no ha acertado al sensor de la mesa*/
        if (nivelActual == NIVEL_1 || (nivelActual == NIVEL_2 && !aciertoNivel1)) seleccionarNuevosObjetivos();
        return;
      } else {
        puntaje--;
        if(puntaje < 0) puntaje = 0;
        if (nivelActual == NIVEL_2) aciertoNivel1 = false;  // Marca el objetivo del nivel 1 como errado
        configurarColorSeccion(i, CRGB::Red);  // Rojo para error
        if (modoActual == MODO_INTENTOS) intentosRestantes--;
        actualizarLCD();  // Actualizar puntaje en LCD inmediatamente
        FastLED.show();
        delay(300);  // Muestra el color por un corto tiempo
        impacto1 = false;
        // Ilumina un nuevo objetivo en azul
        seleccionarNuevosObjetivos(); 
        return;
      }
    }
  }
  // Nivel 2: Primero verifica el objetivo del Nivel 1 y luego verifica los sensores de atrás
  if (nivelActual == NIVEL_2) {
    // Verificar si el sensor de la parte posterior fue presionado antes del objetivo de nivel 1
    for (int i = 0; i < NUMERO_SENSORES_POSTERIOR; i++) {
      if (digitalRead(sensoresPosterior[i]) == HIGH) {
        impacto2 = true;
      }
      if (impacto2 == true) {
        if (!aciertoNivel1) {
          // Penalizar si se presiona el sensor de atrás sin haber acertado el de nivel 1 primero
          puntaje--;
          if(puntaje < 0) puntaje = 0;
          configurarColorSeccion(NUMERO_SENSORES_MESA + i, CRGB::Red);  // Rojo para error
          if (modoActual == MODO_INTENTOS) intentosRestantes--;
          actualizarLCD();  // Actualizar puntaje en LCD inmediatamente
          FastLED.show();
          delay(300);  // Muestra el error en rojo
          impacto2 = false;
          seleccionarNuevosObjetivos();  // Generar nuevos objetivos
          return;
        } else {
          // Si el objetivo del Nivel 1 fue acertado, verifica el objetivo del Nivel 2
          if (i == objetivoActivoPosterior) {
            puntaje++;
            configurarColorSeccion(NUMERO_SENSORES_MESA + i, CRGB::Lime);  // Verde para acierto
          } else {
            puntaje--;
            if(puntaje < 0) puntaje = 0;
            configurarColorSeccion(NUMERO_SENSORES_MESA + i, CRGB::Red);  // Rojo para error
          }
          if (modoActual == MODO_INTENTOS) intentosRestantes--;
          actualizarLCD();  // Actualizar puntaje en LCD
          FastLED.show();
          delay(300);  // Muestra el color por un corto tiempo
          impacto2 = false;
          seleccionarNuevosObjetivos();  // Ilumina un nuevo objetivo en azul
          aciertoNivel1 = false;  // Reinicia el estado para el siguiente intento
          return;
        }
      }
      
    }
  }
}

void configurarColorSeccion(int seccion, CRGB color) {
  for (int i = 0; i < LEDS_POR_SECCION; i++) {
    leds[seccion * LEDS_POR_SECCION + i] = color;
  }
}

void actualizarLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Modo: ");
  lcd.print(modoActual == MODO_TIEMPO ? "Tiempo" : "Intentos");

  lcd.setCursor(0, 1);
  lcd.print("Nivel: ");
  lcd.print(nivelActual == NIVEL_1 ? "1" : "1 y 2");

  lcd.setCursor(0, 2);
  lcd.print("Puntaje: ");
  lcd.print(puntaje);

  lcd.setCursor(0, 3);
  if (modoActual == MODO_INTENTOS) {
    lcd.print("Intentos Rest: ");
    lcd.print(intentosRestantes);
  } else {
    lcd.print("Tiempo: ");
    lcd.print((LIMITE_TIEMPO - (millis() - tiempoInicio)) / 1000);
    lcd.print("s");
  }
}

void finalizarJuego() {
  FastLED.clear();
  FastLED.show();
  lcd.clear();
  lcd.setCursor(0, 1);
  lcd.print("Juego Terminado!");
  lcd.setCursor(0, 2);
  lcd.print("Puntaje Final: ");
  lcd.print(puntaje);
  delay(3000);
  guardarPuntajeEnFirebase();  // Guarda el puntaje y modo en Firebase
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Reiniciando juego...");
  delay(2000);
  // Validar estado 'jugando' en Firebase
  while (validarJugandoEnFirebase() == false) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Nadie jugando...");
    lcd.setCursor(0, 1);
    lcd.print("Intente luego");
  }
  iniciarJuego();  // Reiniciar juego
}
