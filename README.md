# IoTicos Admin Esp

Este firmware se encuentra en desarrollo.<br>
IoTicos Admin Esp, nos permite enviar datos en tiempo real mediante MQTT así como datos para que sean acumulados en base de datos. <br>
Utilizamos como contraparte, La plataforma IoT IoTicos Admin que encontrarás en:https://github.com/ioticos/ioticos_admin <br>
De momento, funciona para esp32, próximamente en esp8266. <br>

Más información en <a href="https://ioticos.org">IoTicos.org</a>

# IoTicos Admin Esp - Rama Santiago

Con el fin de mejorar la libreria WiFiManager provista y poder trabajar con parametros configurables mas alla de los de conexión WiFi que trae incorporada la misma libreria, se le agrego el manejo de archivos SPIFFS.
La idea es que se genere un archivo de configuración (en este caso un archivo .json) que contenga los parametros de conexón tanto al broker mqtt que se utilice como asi tambien al servidor, puerto, clave de escritura, clave de lectura, etc.
