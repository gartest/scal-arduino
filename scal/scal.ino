#include <SPI.h>
#include <MFRC522.h>
#include <Ethernet.h>

#define RST_PIN  5   //Pin 5 para el reset del RC522
#define SS_PIN   4   //Pin 4 para el SS (SDA) del RC522
MFRC522 mfrc522(SS_PIN, RST_PIN); //Creamos el objeto para el RC522

//Configuración de red
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED }; //MAC
byte ip[] = { 192, 168, 1, 180 }; // IP del servidor
EthernetServer server(80); //puerto del servidor
EthernetClient client; //para el server web de las luces
EthernetClient client2; //para guardar los datos de tarjeta a DB

//variables globales
String readString; // string donde se guarda la petición web (ej prender luces)
String card = ""; // string donde se guarda el id de la tarjeta
int ledPin = 3;
int doorPin = 2;
bool haypeticion = false;
bool accesoCorrecto = false;
String estadoLuces = "";
String tarjetasConPermiso = "";

//configuración inicial
void setup() {
  pinMode(ledPin, OUTPUT); //pin selected to control
  pinMode(doorPin, OUTPUT); //pin selected to control
  Serial.begin(9600); //Iniciamos la comunicación  serial
  SPI.begin();        //Iniciamos el Bus SPI
  mfrc522.PCD_Init(); // Iniciamos  el MFRC522
  Serial.println("Lector de tarjetas iniciado");
  //start Ethernet
  Ethernet.begin(mac, ip);
  server.begin();
}

//Bucle principal
void loop() {
  //guardamos datos de tarjeta a DB 
  if (card != "") {
    if (client.connect("192.168.1.136", 80)) {//IP del PC donde está la página
      Serial.println("guardando log tarjeta");
      client.print("GET /api/guardaridtarjeta/192.168.1.180/");//IP de este arduino
      client.print(card + (accesoCorrecto ? "/Acceso%20Correcto" : "/Acceso%20Denegado"));
      client.println(" HTTP/1.1");
      client.println("Host: scal.cl");
      client.println();
    } else {
      Serial.println("error al guardar log tarjeta");
    }
    while (client.connected() && !client.available()) delay(1); //waits for data
    int mloop = 0;
    while ((client.connected() || client.available()) && mloop < 100) { //connected or data available
      char c = client.read();
      mloop++;
    }
    Serial.println("listo.");
    client.stop();
    //limpiamos la variable tarjeta
    card = "";
    //devolvemos el acceso a false
    accesoCorrecto = false;
  }
  //Servidor web (para las luces)
  // Creamos la conexión
  EthernetClient client = server.available();
  client = server.available();
  if (client) {
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        //leemos los caracteres
        if (readString.length() < 100) {
          //guardamos los caracteres en la variable
          readString += c;
        }
        //si la petición terminó ('0x0D´) contestamos con un mensaje
        if (c == 0x0D) {
          client.println("HTTP/1.1 200 OK"); //cabecera estandar de respuesta correcta
          client.println("Content-Type: text/html");//tipo de contenido de la respuesta
          client.println();//salto de linea final obligatorio al final del tipo de contenido
          // casos
          if (readString.indexOf("?ledon") > -1) //si viene la palabra ledon en la petición encendemos la luz
          {
            digitalWrite(ledPin, HIGH); // encendemos luces
            estadoLuces = "Luces encendidas";
            client.println(estadoLuces);
            haypeticion = true;
          }
          if (readString.indexOf("?ledoff") > -1) //si viene ledoff apagamos
          {
            digitalWrite(ledPin, LOW); // apagamos luces
            estadoLuces = "Luces apagadas";
            client.println(estadoLuces);
            haypeticion = true;
          }
          if (readString.indexOf("?estadoluces") > -1) //consulta por el estado de las luces
          {
            client.println(estadoLuces);
            haypeticion = true;
          }
          if (readString.indexOf("?setpermitidos") > -1) //setea el valor de las tarjetas permitidas para este modulo
          {
            tarjetasConPermiso = readString.substring(readString.indexOf("?setpermitidos"));
            client.println("tarjetas con permiso: " + tarjetasConPermiso);
            Serial.println("tarjetas con permiso: " + tarjetasConPermiso);
            haypeticion = true;
          }
          if (!haypeticion) { // si no viene niguna de la anteriores solo mostramos un mensaje
            client.println("Bienvenido");
          }
          delay(10);//le damos un tiempo para procesar
          client.stop();//terminamos la petición
          //Limpiemos la variable para la siguiente petición
          readString = "";
        }
      }
    }
  }
  //RFID
  // Revisamos si hay nuevas tarjetas  presentes
  if ( mfrc522.PICC_IsNewCardPresent())
  {
    Serial.println("Nueva tarjeta detectada");
    //Seleccionamos una tarjeta
    if ( mfrc522.PICC_ReadCardSerial())
    {
      // Enviamos serialemente su UID
      Serial.print("Identificador de la tarjeta: ");
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        card.concat(String(mfrc522.uid.uidByte[i] < 0x10 ? "0" : ""));
        card.concat(String(mfrc522.uid.uidByte[i], HEX));
      }
      Serial.print(card);
      if(tarjetasConPermiso.indexOf(card) > -1){
        accesoCorrecto = true;
      }
      Serial.println();
      // Terminamos la lectura de la tarjeta  actual
      mfrc522.PICC_HaltA();
    }
  }
  //Fin RFID
  if (accesoCorrecto) {
    digitalWrite(doorPin, HIGH); // abrimos puerta
    delay(1500);
    digitalWrite(doorPin, LOW);    
  }
}
