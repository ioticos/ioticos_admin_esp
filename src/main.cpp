#include <Arduino.h>
#include <ArduinoJson.h>
#if defined(ESP8266)
#include <ESP8266WiFi.h>
#include <FS.h>
#else
#include <WiFi.h>
#include "SPIFFS.h"
#endif


#include <WiFiManager.h>
#include <Separador.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

//*********************************
//*********** CONFIG **************
//*********************************

#define WIFI_PIN 17
#define LED 2 //On Board LED

int brightness = 0;    // how bright the LED is
int fadeAmount = 5;    // how many points to fade the LED by

// setting PWM properties
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8; //Resolution 8, 10, 12, 15


//estas etiquetas se usan en el main, WifiManager.cpp y WifiManager.h para leer y escribir el archivo config.json
String etParametro1 = "Serial Number";
String etParametro2 = "Insert Password";
String etParametro3 = "Get Data Password";
String etParametro4 = "Server";
String etParametro5 = "MQTT Server";
String etParametro6 = "MQTT Port";

char serial_number[40];
char insert_password[40];
char get_data_password[40];
char server[40];
//MQTT
char mqtt_server[40];
char mqtt_port[40];


//estos datos deben estar configurados también en las constantes de tu panel
// NO USES ESTOS DATOS PON LOS TUYOS!!!!
/*
const String serial_number = "111";
const String insert_password = "121212";
const String get_data_password = "232323";
const char*  server = "ioticosadmin.ml";
//MQTT
const char *mqtt_server = "ioticos.org";
const int mqtt_port = 8883;
*/

//no completar, el dispositivo se encargará de averiguar qué usuario y qué contraseña mqtt debe usar.
char mqtt_user[20] = "";
char mqtt_pass[20] = "";

const int expected_topic_length = 26;

//variables de sistema de archivos
boolean SPIFFSinit = false; //bandera para verificar que el sistema de archivo se inicio correctamente


WiFiManager wifiManager;
WiFiClientSecure client;
PubSubClient mqttclient(client);
WiFiClientSecure client2;

Separador s;




//************************************
//***** DECLARACION FUNCIONES ********
//************************************
bool get_topic(int length);
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void send_mqtt_data();
void send_to_database();
void SPIFFSbegin();
void setupWifi();
void configValues();
void configPortal();


//*************************************
//********      GLOBALS         *******
//*************************************
bool topic_obteined = false;
char device_topic_subscribe [40];
char device_topic_publish [40];
char msg[25];
float temp = 0;
int hum = 0;
long milliseconds = 0;
byte sw1 = 0;
byte sw2 = 0;
byte slider = 0;



void setup() {
  randomSeed(analogRead(0));

  pinMode(LED,OUTPUT);

  // configure LED PWM functionalitites
  ledcSetup(ledChannel, freq, resolution);

  // attach the channel to the GPIO2 to be controlled
  ledcAttachPin(LED, ledChannel);

  Serial.begin(115200);


  pinMode(WIFI_PIN,INPUT_PULLUP);

  SPIFFSbegin();//funcion para iniciar el sistema de archivos y verifacar que el config.json no este corrupto
  setupWifi();//funcion que conecta a wifi via autoconnect. si no lo consigue llama a configPortal que abre el portal de configuracion
  configValues();//funcion que levanta los parametros del archivo configuracion.json y los guarda en las variables


  //client2.setCACert(web_cert);

  while(!topic_obteined){
    topic_obteined = get_topic(expected_topic_length);
    delay(3000);
  }


  //set mqtt cert
  //client.setCACert(mqtt_cert);
  int puertoMQTT = atoi(mqtt_port);
  mqttclient.setServer(mqtt_server, puertoMQTT);
	mqttclient.setCallback(callback);


}

void loop() {

  if (!client.connected()) {
		reconnect();
	}

//si el pulsador wifi esta en low, activamos el acces point de configuración
  if ( digitalRead(WIFI_PIN) == LOW ) {
    WiFiManager wifiManager;
    wifiManager.startConfigPortal("IoTicos Admin");
    Serial.println("Conectados a WiFi!!! :)");
  }

  //si estamos conectados a mqtt enviamos mensajes
  if (millis() - milliseconds > 3000){
    milliseconds = millis();


    if(mqttclient.connected()){
      //set mqtt cert
      temp = random(0,500) /10;
      hum = random(0,99);
      String to_send = String(temp) + "," + String(hum) + "," + String(sw1)+","+ String(sw2);
      to_send.toCharArray(msg,20);
      mqttclient.publish(device_topic_publish,msg);

      if (temp>47 || temp < 3){
        send_to_database();
      }
    }

  }

  mqttclient.loop();

}



//************************************
//*********** FUNCIONES **************
//************************************

void callback(char* topic, byte* payload, unsigned int length) {
  String incoming = "";
	Serial.print("Mensaje recibido desde tópico -> ");
	Serial.print(topic);
	Serial.println("");
	for (int i = 0; i < length; i++) {
		incoming += (char)payload[i];
	}
	incoming.trim();
	Serial.println("Mensaje -> " + incoming);

  String str_topic = String(topic);
  String command = s.separa(str_topic,'/',3);
  Serial.println(command);

  if(command=="sw1"){
    Serial.println("Sw1 pasa a estado " + incoming);
    sw1 = incoming.toInt();
  }

  if(command=="sw2"){
    Serial.println("Sw2 pasa a estado " + incoming);
    sw2 = incoming.toInt();
  }

  if(command=="slider"){
    Serial.println("Sw1 pasa a estado " + incoming);
    slider = incoming.toInt();
    ledcWrite(ledChannel,slider);
  }

}

void reconnect() {

	while (!mqttclient.connected()) {
		Serial.print("Intentando conexión MQTT SSL");
		// we create client id
		String clientId = "esp32_ia_";
		clientId += String(random(0xffff), HEX);
		// Trying SSL MQTT connection
		if (mqttclient.connect(clientId.c_str(),mqtt_user,mqtt_pass)) {
			Serial.println("Connected!");
			// We subscribe to topic

			mqttclient.subscribe(device_topic_subscribe);

		} else {
			Serial.print("falló :( con error -> ");
			Serial.print(mqttclient.state());
			Serial.println(" Intentamos de nuevo en 5 segundos");

			delay(5000);
		}
	}
}

//función para obtener el tópico perteneciente a este dispositivo
bool get_topic(int length){

  Serial.println("\nIniciando conexión segura para obtener tópico raíz...");

  if (!client2.connect(server, 443)) {
    Serial.println("Falló conexión!");
  }else {
    Serial.println("Conectados a servidor para obtener tópico - ok");
    // Make a HTTP request:
    String data = "gdp="+String(get_data_password)+"&sn="+ String(serial_number)+"\r\n";
    client2.print(String("POST ") + "/app/getdata/gettopics" + " HTTP/1.1\r\n" +\
                 "Host: " + String(server) + "\r\n" +\
                 "Content-Type: application/x-www-form-urlencoded"+ "\r\n" +\
                 "Content-Length: " + String (data.length()) + "\r\n\r\n" +\
                 data +\
                 "Connection: close\r\n\r\n");

    Serial.println("Solicitud enviada - ok");

    while (client2.connected()) {
      String line = client2.readStringUntil('\n');
      if (line == "\r") {
        Serial.println("Headers recibidos - ok");
        break;
      }
    }

    String line;
    while(client2.available()){
      line += client2.readStringUntil('\n');
    }
    Serial.println(line);
    String temporal_topic = s.separa(line,'#',1);
    String temporal_user = s.separa(line,'#',2);
    String temporal_password = s.separa(line,'#',3);



    Serial.println("El tópico es: " + temporal_topic);
    Serial.println("El user MQTT es: " + temporal_user);
    Serial.println("La pass MQTT es: " + temporal_password);
    Serial.println("La cuenta del tópico obtenido es: " + String(temporal_topic.length()));

    if (temporal_topic.length()==length){
      Serial.println("El largo del tópico es el esperado: " + String(temporal_topic.length()));

      String temporal_topic_subscribe = temporal_topic + "/actions/#";
      temporal_topic_subscribe.toCharArray(device_topic_subscribe,40);
      Serial.println(device_topic_subscribe);
      String temporal_topic_publish = temporal_topic + "/data";
      temporal_topic_publish.toCharArray(device_topic_publish,40);
      temporal_user.toCharArray(mqtt_user,20);
      temporal_password.toCharArray(mqtt_pass,20);

      client2.stop();
      return true;
    }else{
      client2.stop();
      return false;
    }

  }
}


void send_to_database(){

  Serial.println("\nIniciando conexión segura para enviar a base de datos...");

  if (!client2.connect(server, 443)) {
    Serial.println("Falló conexión!");
  }else {
    Serial.println("Conectados a servidor para insertar en db - ok");
    // Make a HTTP request:
    String data = "idp="+String(insert_password)+"&sn="+String(serial_number)+"&temp="+String(temp)+"&hum="+String(hum)+"\r\n";
    client2.print(String("POST ") + "/app/insertdata/insert" + " HTTP/1.1\r\n" +\
                 "Host: " + String(server) + "\r\n" +\
                 "Content-Type: application/x-www-form-urlencoded"+ "\r\n" +\
                 "Content-Length: " + String (data.length()) + "\r\n\r\n" +\
                 data +\
                 "Connection: close\r\n\r\n");

    Serial.println("Solicitud enviada - ok");

    while (client2.connected()) {
      String line = client2.readStringUntil('\n');
      if (line == "\r") {
        Serial.println("Headers recibidos - ok");
        break;
      }
    }


    String line;
    while(client2.available()){
      line += client2.readStringUntil('\n');
    }
    Serial.println(line);
    client2.stop();

    }

  }

  void SPIFFSbegin(){
    if (SPIFFS.begin(true)) {
      SPIFFSinit = true;
      Serial.println("SPIFFS iniciado");
      if (!SPIFFS.exists("/configuracion.json")) {
        Serial.println("Archivo de configuaración dañado, grabado parametros de base");

        DynamicJsonBuffer jsonBufferW;
        JsonObject& jsonWrite = jsonBufferW.createObject();

        jsonWrite[etParametro1] = "";
        jsonWrite[etParametro2] = "";
        jsonWrite[etParametro3] = "";
        jsonWrite[etParametro4] = "";
        jsonWrite[etParametro5] = "";
        jsonWrite[etParametro6] = "";

        File configFile = SPIFFS.open("/configuracion.json", "w");
        if (!configFile) {
          Serial.println("failed to open config file for writing");
        }
        jsonWrite.printTo(Serial);
        jsonWrite.printTo(configFile);
        configFile.close();
        Serial.println("Archivo grabado correctamente");
      }else{
        Serial.println("Ok exixtencia de archivo");
      }
    }else{
      Serial.println("no se puede iniciar el sistema de archivos");
    }
  }

  void setupWifi(){
    delay(10);

    WiFiManager wifiManager;
    #if defined(ESP8266)
    String deviceName = "IoTicos Admin ESP_" + String(ESP.getChipId());
    #else
    String deviceName = "IoTicos Admin ESP_" + String(ESP_getChipId());
    #endif
    char dName[50];
    deviceName.toCharArray(dName,50);
    if (!wifiManager.autoConnect(dName)) {
      configPortal(); //entra en modo AP para configuracion de parametros
      Serial.println("Entrando a config WiFi");
    }else{
      //if you get here you have connected to the WiFi
      Serial.println("Conectado. Usando los ultimos parametros guardados)");
    }

  }


  void configValues(){
    Serial.println("mounting FS...desde configValues");

    if (SPIFFSinit) {
      Serial.println("mounted file system");
      if (SPIFFS.exists("/configuracion.json")) {
        //file exists, reading and loading
        Serial.println("reading config file");
        File configFile = SPIFFS.open("/configuracion.json", "r");
        if (configFile) {
          Serial.println("opened config file");
          size_t size = configFile.size();
          // Allocate a buffer to store contents of the file.
          std::unique_ptr<char[]> buf(new char[size]);
          Serial.println(size);
          configFile.readBytes(buf.get(), size);
          DynamicJsonBuffer jsonBufferR;
          JsonObject& jsonR = jsonBufferR.parseObject(buf.get());
          jsonR.printTo(Serial);
          if (jsonR.success()) {
            Serial.println("\nparsed json");


            strcpy(serial_number, jsonR[etParametro1]);
            strcpy(insert_password, jsonR[etParametro2]);
            strcpy(get_data_password, jsonR[etParametro3]);
            strcpy(server, jsonR[etParametro4]);
            strcpy(mqtt_server, jsonR[etParametro5]);
            strcpy(mqtt_port, jsonR[etParametro6]);

            Serial.println(serial_number);
            Serial.println(insert_password);
            Serial.println(get_data_password);
            Serial.println(server);
            Serial.println(mqtt_server);
            Serial.println(mqtt_port);
          } else {
            Serial.println("failed to load json config");
          }
          configFile.close();
        }
      }
    } else {
      Serial.println("failed to mount FS");
    }
    //end read
  }

  void configPortal(){

    WiFiManager wifiManager;
    //String deviceName = "IoTicos Admin ESP_" + String(ESP.getChipId());
    String deviceName = "IoTicos Admin ESP_";
    char dName[50];
    deviceName.toCharArray(dName,50);

    Serial.println(deviceName);
    if (!wifiManager.startConfigPortal(dName) && SPIFFSinit) {
      Serial.println("failed to connect and hit timeout");
      delay(3000);
      //reset and try again, or maybe put it to deep sleep
      #if defined(ESP8266)
        ESP.reset();
      #else
        ESP.restart();
      #endif
      delay(5000);
    }

    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
    #if defined(ESP8266)
      ESP.reset();
    #else
      ESP.restart();
    #endif
  }
