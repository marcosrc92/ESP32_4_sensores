#include <Arduino.h>
#include <WiFi.h>
#include <ESP32_MailClient.h>
#include "WiFiClientSecure.h"
#include <UniversalTelegramBot.h>
#include <esp32-hal-timer.h>
#include <driver/adc.h>

/****************************************************************************************/
/*******************DEFINICIONES DE CONFIGURACION (NO TOCAR)*****************************/
/****************************************************************************************/
#define GMAIL_SMTP_SERVER "smtp.gmail.com"
#define GMAIL_SMTP_PORT 465

/* Credenciales e-mail */


/* ASIGNACIONES DE PINES DEL ESP32*/
#define P_LEDWIFI 22  //modo OUTPUT (Digital)

#define P_ACK_0 1      //modo INPUT_PULLUP con ISR a flanco descencente (Digital)
#define P_ACK_1 3      //modo INPUT_PULLUP con ISR a flanco descencente (Digital)
#define P_ACK_2 15     //modo INPUT_PULLUP con ISR a flanco descencente (Digital)
//#define P_ACK_3 8       //modo INPUT_PULLUP con ISR a flanco descencente (Digital)

//RGB0
#define P_SENSOR0 32  //Analog
#define P_L0VERDE 21  //modo OUTPUT (Digital)
#define P_L0ROJO 17   //modo OUTPUT (Digital)
//#define P_L0AZUL 33   //modo OUTPUT (Digital)

//RGB1
#define P_SENSOR1 35  //Analog
#define P_L1VERDE 16  //modo OUTPUT (Digital)
#define P_L1ROJO 4    //modo OUTPUT (Digital)
//#define P_L1AZUL 35   //modo OUTPUT (Digital)

//RGB2
#define P_SENSOR2 36  //Analog
#define P_L2VERDE 0  //modo OUTPUT (Digital)
#define P_L2ROJO 2   //modo OUTPUT (Digital)
//#define P_L2AZUL 34   //modo OUTPUT (Digital)

//RGB3
#define P_SENSOR3 39  //Analog
//#define P_L3VERDE 7  //modo OUTPUT (Digital)
//#define P_L3ROJO 6   //modo OUTPUT (Digital)
//#define P_L3AZUL 36   //modo OUTPUT (Digital)

#define P_BUZZER 13  //modo OUTPUT (Digital)

#define N_SENSORES 4
#define N_TIMERMAX 480  //tiempo en minutos para que salte la alarma
#define N_INTENTOS_WIFI_MAX 30

/****************************************************************************************/
/*****************VARIABLES DE CONFIGURACION (MODIFICABLE)*******************************/
/****************************************************************************************/
#define GMAIL_SMTP_USERNAME " "
#define GMAIL_SMTP_PASSWORD " "


#define WIFI_SSID "valmei"
#define WIFI_PASSWORD "valmeilab"

//#define WIFI_SSID "BV4900Pro"
//#define WIFI_PASSWORD "marcosrc92"


#define BOT_TOKEN " "

#define VALOR_CALIBRACION_menos20 -50.75
#define VALOR_CALIBRACION_menos80 -33.31

#define NUM_TEL_USERS 2
#define NUM_EMAIL_USERS 2

bool en_mails = 1; //permiso de mandar emails, se modifica solo aqui. 0 = deshabilita; 1 = habilita
bool en_sensor0 = 1;
bool en_sensor1 = 1;
bool en_sensor2 = 1;
bool en_sensor3 = 0;

//un array para comprobar si es usario autorizado se debe modificar NUM_TEL_USERS en los #define dependiendo del numero de usuarios autorizados
String CHAT_ID[NUM_TEL_USERS] = {"1769646176", "1395683047"};

//el tama??o de este vector es el numero de correos distintos que se van a meter, se debe modificar en los #define
char* EMAIL_LIST[NUM_EMAIL_USERS] = {"marcos.rodriguez@inycom.es", "valledorluis@uniovi.es"};

//temperatura control cada sensor           = {Sen0, Sen1,Sen2,Sen3}
float temperatura_alta[N_SENSORES]          = {  -5,  -5,  12,  -60};
float temperatura_media[N_SENSORES]         = { -12, -12,   9,  -65};
float temperatura_baja[N_SENSORES]          = { -17, -17,   5,  -70};
float temperatura_baja_extrema[N_SENSORES]  = { -25, -25,   2,  -85};

//comandos (cmd) y mensajes (msg) de telegram
String cmd_temp_sensor0 = "/temperatura0";
String cmd_estado_sensor0 = "/estado0";
String msg_arranque_sensor0 = "Iniciando arranque de la placa de control";
String msg_OK_sensor0 = "El estado del frigorifico es correcto";
String msg_tempalta_sensor0 = "La temperatura del frigorifico esta fuera de rango";
String msg_enRevision_sensor0 = "El frigorifico se encuentra en revision";
String msg_revisado_sensor0 = "El frigorifico esta revisado";

String cmd_temp_sensor1 = "/temperatura1";
String cmd_estado_sensor1 = "/estado1";
String msg_arranque_sensor1 = "Iniciando arranque de la placa de control";
String msg_OK_sensor1 = "El estado del frigorifico es correcto";
String msg_tempalta_sensor1 = "La temperatura del frigorifico esta fuera de rango";
String msg_enRevision_sensor1 = "El frigorifico se encuentra en revision";
String msg_revisado_sensor1 = "El frigorifico esta revisado";

String cmd_temp_sensor2 = "/temperatura2";
String cmd_estado_sensor2 = "/estado2";
String msg_arranque_sensor2 = "Iniciando arranque de la placa de control";
String msg_OK_sensor2 = "El estado del frigorifico es correcto";
String msg_tempalta_sensor2 = "La temperatura del frigorifico esta fuera de rango";
String msg_enRevision_sensor2 = "El frigorifico se encuentra en revision";
String msg_revisado_sensor2 = "El frigorifico esta revisado";

String cmd_temp_sensor3 = "/temperatura3";
String cmd_estado_sensor3 = "/estado3";
String msg_arranque_sensor3 = "Iniciando arranque de la placa de control";
String msg_OK_sensor3 = "El estado del frigorifico es correcto";
String msg_tempalta_sensor3 = "La temperatura del frigorifico es demasiado alta";
String msg_enRevision_sensor3 = "El frigorifico se encuentra en revision";
String msg_revisado_sensor3 = "El frigorifico esta revisado";

/****************************************************************************************/
/****************************************************************************************/
/****************************************************************************************/

/* Declaracion del objeto de sesion SMTP */
SMTPData data;

//Declaracion de funcion de interrupci??n de pulsacion
void IRAM_ATTR ISR_ACK0();
void IRAM_ATTR ISR_ACK1();
void IRAM_ATTR ISR_ACK2();
void IRAM_ATTR ISR_ACK3();

bool flag_ACK0 = 0;
bool flag_ACK1 = 0;
bool flag_ACK2 = 0;
bool flag_ACK3 = 0;

//Declaraci??n la de funcion de estados
void maquina_estados(int, int);
void estados_automaticos();
int estado[N_SENSORES] = {0, 0, 0, 0};

// Analog incializacion

float calculo_temp(float, int);
float Val_sensor = 0;
float temp_actual[N_SENSORES] = {0, 0, 0, 0};


//variables de estado
bool W_conexion = 1;
bool estado_arranque[N_SENSORES] = {0, 0, 0, 0};
bool estado_OK[N_SENSORES] = {0, 0, 0, 0};
bool estado_ACK[N_SENSORES] = {0, 0, 0, 0};
bool estado_revision[N_SENSORES] = {0, 0, 0, 0};
bool estado_revisado[N_SENSORES] = {0, 0, 0, 0};

bool estadoLEDWIFI = LOW; //LOW = 0x0
volatile bool parp1Hz = LOW; //LOW = 0x0

//nombre del remitente
char* remitente = "ESP32";
char* asunto = "Actualizacion estado frigorifico";

//configuracion del timer
hw_timer_t * timer = NULL;
void IRAM_ATTR onTimer(); //Declaracion de funcion de interrupci??n de timer
volatile long interruptCounter[N_SENSORES] = {0, 0, 0, 0};

hw_timer_t * timer_parp = NULL;
void IRAM_ATTR parp_mantenimiento(); //Declaracion de funcion de interrupci??n de timer

//hw_timer_t * timer_wifi = NULL;
//void IRAM_ATTR check_wifi(); //Declaracion de funcion de interrupci??n de timer

volatile bool flag_alarma[N_SENSORES] = {0, 0, 0, 0};
volatile bool flag_8horas[N_SENSORES] = {0, 0, 0, 0};
volatile bool flag_timer = 0;

//Funcion de manejo de mensajes de telegram
void handleNewMessages(int);
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOT_TOKEN, secured_client);
// Checks for new messages every 1 second.
int botRequestDelay = 1000;
unsigned long lastTimeBotRan;

unsigned long W_previousMillis = 0;
unsigned long W_interval = 30000;


/****************************************************************************************/
/******************************FUNCION ENVIO E-MAILS*************************************/
/****************************************************************************************/

String sendEmail(char *subject, char *sender, String body, char *recipient, boolean htmlFormat) {
  data.setLogin(GMAIL_SMTP_SERVER, GMAIL_SMTP_PORT, GMAIL_SMTP_USERNAME, GMAIL_SMTP_PASSWORD);
  data.setSender(sender, GMAIL_SMTP_USERNAME);
  data.setSubject(subject);
  data.setMessage(body, htmlFormat);
  data.addRecipient(recipient);
  if (!MailClient.sendMail(data))
    return MailClient.smtpErrorReason();
  data.empty(); //tras enviar el mail limpio los datos de envio, evitando que se envien m??ltiples emails (espero)
  return "";
}

/****************************************************************************************/
/****************************************************************************************/
/****************************************************************************************/

void setup() {

  //Serial.begin(115200);
  Serial.end();

  pinMode(P_LEDWIFI, OUTPUT);

  pinMode(P_ACK_0, INPUT_PULLUP);
  pinMode(P_ACK_1, INPUT_PULLUP);
  pinMode(P_ACK_2, INPUT_PULLUP);
  //pinMode(P_ACK_3, INPUT_PULLUP);

  pinMode(P_L0VERDE, OUTPUT);
  pinMode(P_L0ROJO, OUTPUT);
  //pinMode(P_L0AZUL, OUTPUT);
  //digitalWrite(P_L0AZUL, LOW); //no se utiliza por ahora, asi se mantiene el color azul apagado

  pinMode(P_L1VERDE, OUTPUT);
  pinMode(P_L1ROJO, OUTPUT);
  //pinMode(P_L1AZUL, OUTPUT);
  //digitalWrite(P_L1AZUL, LOW); //no se utiliza por ahora, asi se mantiene el color azul apagado

  pinMode(P_L2VERDE, OUTPUT);
  pinMode(P_L2ROJO, OUTPUT);
  //pinMode(P_L2AZUL, OUTPUT);
  //digitalWrite(P_L2AZUL, LOW); //no se utiliza por ahora, asi se mantiene el color azul apagado

  //pinMode(P_L3VERDE, OUTPUT);
  //pinMode(P_L3ROJO, OUTPUT);
  //pinMode(P_L3AZUL, OUTPUT);
  //digitalWrite(P_L3AZUL, LOW); //no se utiliza por ahora, asi se mantiene el color azul apagado

  pinMode(P_BUZZER, OUTPUT);

  /****************GPIO sin utilizar******************/
  pinMode(9, OUTPUT);
  digitalWrite(9, LOW);

  pinMode(10, OUTPUT);
  digitalWrite(10, LOW);

  pinMode(13, OUTPUT);
  digitalWrite(13, LOW);

  pinMode(14, OUTPUT);
  digitalWrite(14, LOW);

  pinMode(25, OUTPUT);
  digitalWrite(25, LOW);

  pinMode(27, OUTPUT);
  digitalWrite(27, LOW);

  /***************************************************/


  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED)
  {
    //parpadeo en LED WiFi
    digitalWrite(P_LEDWIFI, estadoLEDWIFI);
    if (estadoLEDWIFI == LOW)
      estadoLEDWIFI = HIGH;
    else
      estadoLEDWIFI = LOW;
    Serial.print(".");
    delay(140);
  }


  //fijar parpadeo del LED
  digitalWrite(P_LEDWIFI, HIGH);
  //Serial.println("WiFi connected.");

  secured_client.setCACert(TELEGRAM_CERTIFICATE_ROOT);

  //setup del timer 0 interrupcion lectura ADC
  timer = timerBegin(0, 65536, true);       //timer 0 PS = 65536 f=1220Hz countUp=true
  timerAttachInterrupt(timer, &onTimer, true);
  //timerAlarmWrite(timer, 1220, true);     //cuenta 1 segundo
  timerAlarmWrite(timer, 73200, true);      //cuenta 1 minuto
  //timerAlarmWrite(timer, 2196000, true);  //cuenta 30 min
  //timerAlarmWrite(timer, 4392000, true);  //cuenta 1 hora
  timerAlarmEnable(timer);

  //setup del timer 1 para parpadeos de 1 Hz
  timer_parp = timerBegin(1, 65536, true);  //timer 0 PS = 65536 f=1220Hz countUp=true
  timerAttachInterrupt(timer_parp, &parp_mantenimiento, true);
  timerAlarmWrite(timer_parp, 1220, true);  //cuenta 1 segundo
  timerAlarmEnable(timer_parp);

  //digitalWrite(P_L3VERDE, HIGH);
  digitalWrite(P_L1VERDE, HIGH);
  digitalWrite(P_L2VERDE, HIGH);
  digitalWrite(P_L0VERDE, HIGH);

  //inicializacion de variable temp_actual
  if (en_sensor0){
    float Aread0 = analogRead(P_SENSOR0);
    temp_actual[0] = calculo_temp(Aread0, 0);
  }

  if (en_sensor1){
    float Aread1 = analogRead(P_SENSOR1);
    temp_actual[1] = calculo_temp(Aread1, 1);  
  }

  if (en_sensor2){
    float Aread2 = analogRead(P_SENSOR2);
    temp_actual[2] = calculo_temp(Aread2, 2);
  }

  if (en_sensor3){
    float Aread3 = analogRead(P_SENSOR3);
    temp_actual[3] = calculo_temp(Aread3, 3);
  }

  estados_automaticos();
  if (en_sensor0)
    maquina_estados(estado[0], 0);

  if (en_sensor1)
    maquina_estados(estado[1], 1);

  if (en_sensor2)
    maquina_estados(estado[2], 2);

  if (en_sensor3)
    maquina_estados(estado[3],3);


  //config del pin de interrupcion de ACK
  attachInterrupt(P_ACK_0, ISR_ACK0, RISING);
  attachInterrupt(P_ACK_1, ISR_ACK1, RISING);
  attachInterrupt(P_ACK_2, ISR_ACK2, RISING);
  //attachInterrupt(P_ACK_3, ISR_ACK3, RISING);

  //digitalWrite(P_L3VERDE, LOW);
  digitalWrite(P_L1VERDE, LOW);
  digitalWrite(P_L2VERDE, LOW);
  digitalWrite(P_L0VERDE, LOW);
}

/******************************************************************************************/

void loop() {

  unsigned long W_currentMillis = millis();
  float Aread0 = 0;

  if (flag_timer) { //entrada periodica con timer 0 o asincrona con pulsacion de ACK
    if (en_sensor0){
      float Aread0 = analogRead(P_SENSOR0);
      temp_actual[0] = calculo_temp(Aread0, 0);
    }

    if (en_sensor1){
      float Aread1 = analogRead(P_SENSOR1);
      temp_actual[1] = calculo_temp(Aread1, 1);  
    }

    if (en_sensor2){
      float Aread2 = analogRead(P_SENSOR2);
      temp_actual[2] = calculo_temp(Aread2 ,2);
    }
  
    if (en_sensor3){
      float Aread3 = analogRead(P_SENSOR3);
      temp_actual[3] = calculo_temp(Aread3, 3);
    }


    estados_automaticos();
    
    if (en_sensor0)
      maquina_estados(estado[0], 0);

    if (en_sensor1)
      maquina_estados(estado[1], 1);

    if (en_sensor2)
      maquina_estados(estado[2], 2);

    if (en_sensor3)
      maquina_estados(estado[3],3);

    flag_timer = 0;
  }

    if(flag_ACK0 && en_sensor0){
      maquina_estados(estado[0],0);
      flag_ACK0 = 0;
    }
    
    if(flag_ACK1 && en_sensor1){
      maquina_estados(estado[1],1);
      flag_ACK1 = 0;
    }

    if(flag_ACK2 && en_sensor2){
      maquina_estados(estado[2],2);
      flag_ACK2 = 0;
    }

    if(flag_ACK3 && en_sensor3){
      maquina_estados(estado[3],3);
      flag_ACK3 = 0;
    }


  if (!flag_alarma[0] && !flag_alarma[1] && !flag_alarma[2] && !flag_alarma[3]) {
    digitalWrite(P_BUZZER, LOW);
  }
  else digitalWrite(P_BUZZER, HIGH);

    
  //reconexion
  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(P_LEDWIFI, HIGH);
    W_conexion = 1;
  }
  else {
    W_conexion = 0;
  }

  if ((WiFi.status() != WL_CONNECTED) && (W_currentMillis - W_previousMillis >= W_interval)) {
    //Serial.println("Reconectando al WiFi");
    WiFi.disconnect();
    WiFi.reconnect();
    W_previousMillis = W_currentMillis;
  }
  

  if (millis() > lastTimeBotRan + botRequestDelay)  {
    int numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    while (numNewMessages) {
      //Serial.println("got response");
      handleNewMessages(numNewMessages);
      numNewMessages = bot.getUpdates(bot.last_message_received + 1);
    }
    lastTimeBotRan = millis();
  }

  delay(100);
}



/******************************************************************************************/

void maquina_estados(int estado_f, int sensor) {
  unsigned int tel_user = 0;
  unsigned int email_user = 0;
  String result, mensaje_estado;

  switch (estado_f) {
    case 0: //arranque inicial

      if (!estado_arranque[sensor]) {
        mensaje_estado = "Proceso de arranque de la m??quina asociada al sensor: ";
        mensaje_estado += String(sensor);
        mensaje_estado += ", esperando hasta 8 horas a que baje a una temperatura de control";

        for (email_user = 0; email_user < NUM_EMAIL_USERS; email_user++) {
          result = sendEmail(asunto, remitente, mensaje_estado, EMAIL_LIST[email_user], false);
        }

        for (tel_user = 0; tel_user < NUM_TEL_USERS; tel_user++) {
          bot.sendMessage(CHAT_ID[tel_user], mensaje_estado, "");
        }
      }

      estado_arranque[sensor] = 1;
      estado_OK[sensor] = 0;
      estado_ACK[sensor] = 0;
      estado_revision[sensor] = 0;
      estado_revisado[sensor] = 0;
      flag_alarma[sensor] = 0;
      flag_8horas[sensor] = 0;

      if (sensor == 0) {
        digitalWrite(P_L0VERDE, LOW);
        digitalWrite(P_L0ROJO, LOW);
      }
      else if (sensor == 1) {
        digitalWrite(P_L1VERDE, LOW);
        digitalWrite(P_L1ROJO, LOW);
      }
      else if (sensor == 2) {
        digitalWrite(P_L2VERDE, LOW);
        digitalWrite(P_L2ROJO, LOW);
      }
      else if (sensor == 3) {
        //digitalWrite(P_L3VERDE, LOW);
        //digitalWrite(P_L3ROJO, LOW);
      }

      break;

    case 1: //todo OK
      if (estado_OK[sensor] == 0 && en_mails) {
        mensaje_estado = "El estado de la maquina asociada al sensor: ";
        mensaje_estado += String(sensor);
        mensaje_estado += " es correcto";

        for (email_user = 0; email_user < NUM_EMAIL_USERS; email_user++) {
          result = sendEmail(asunto, remitente, mensaje_estado, EMAIL_LIST[email_user], false);
        }

        for (tel_user = 0; tel_user < NUM_TEL_USERS; tel_user++) {
          bot.sendMessage(CHAT_ID[tel_user], mensaje_estado, "");
        }
      }

      estado_arranque[sensor] = 1;
      estado_OK[sensor] = 1;
      estado_ACK[sensor] = 0;
      estado_revision[sensor] = 0;
      estado_revisado[sensor] = 0;
      flag_alarma[sensor] = 0;
      flag_8horas[sensor] = 0;

      //color Verde
      if (sensor == 0) {
        digitalWrite(P_L0VERDE, HIGH);
        digitalWrite(P_L0ROJO, LOW);
      }
      else if (sensor == 1) {
        digitalWrite(P_L1VERDE, HIGH);
        digitalWrite(P_L1ROJO, LOW);
      }
      else if (sensor == 2) {
        digitalWrite(P_L2VERDE, HIGH);
        digitalWrite(P_L2ROJO, LOW);
      }
      else if (sensor == 3) {
        //digitalWrite(P_L3VERDE, HIGH);
        //digitalWrite(P_L3ROJO, LOW);
      }

      break;

    case 2: //alarma SALIDA BUZZER
      if (estado_ACK[sensor] == 0 && en_mails) {
        mensaje_estado = "La temperatura de la maquina asociada al sensor: ";
        mensaje_estado += String(sensor);
        mensaje_estado += " esta fuera de rango. Temperatura actual: ";
        mensaje_estado += String(temp_actual[sensor]);

        for (email_user = 0; email_user < NUM_EMAIL_USERS; email_user++) {
          result = sendEmail(asunto, remitente, mensaje_estado, EMAIL_LIST[email_user], false);
        }

        for (tel_user = 0; tel_user < NUM_TEL_USERS; tel_user++) {
          bot.sendMessage(CHAT_ID[tel_user], mensaje_estado, "");
        }
      }
      estado_arranque[sensor] = 1;
      estado_OK[sensor] = 0;
      estado_ACK[sensor] = 1;
      estado_revision[sensor] = 0;
      estado_revisado[sensor] = 0;

      //color rojo
      if (sensor == 0) {
        digitalWrite(P_L0VERDE, LOW);
        digitalWrite(P_L0ROJO, HIGH);
        flag_alarma[0] = 1;
      }
      else if (sensor == 1) {
        digitalWrite(P_L1VERDE, LOW);
        digitalWrite(P_L1ROJO, HIGH);
        flag_alarma[1] = 1;
      }
      else if (sensor == 2) {
        digitalWrite(P_L2VERDE, LOW);
        digitalWrite(P_L2ROJO, HIGH);
        flag_alarma[2] = 1;
      }
      /*
      else if (sensor == 3) {
        //digitalWrite(P_L3VERDE, LOW);
        // digitalWrite(P_L3ROJO, HIGH);
        flag_alarma[3] = 0;
      }
*/
      //digitalWrite(P_BUZZER, HIGH);
      break;

    case 3: //en revision
      if (estado_revision[sensor] == 0 && en_mails) {
        mensaje_estado = "La maquina asociada al sensor: ";
        mensaje_estado += String(sensor);
        mensaje_estado += " se encuentra en revision";

        for (email_user = 0; email_user < NUM_EMAIL_USERS; email_user++) {
          result = sendEmail(asunto, remitente, mensaje_estado, EMAIL_LIST[email_user], false);
        }

        for (tel_user = 0; tel_user < NUM_TEL_USERS; tel_user++) {
          bot.sendMessage(CHAT_ID[tel_user], mensaje_estado, "");
        }
      }

      estado_arranque[sensor] = 1;
      estado_OK[sensor] = 0;
      estado_ACK[sensor] = 0;
      estado_revision[sensor] = 1;
      estado_revisado[sensor] = 0;
      flag_alarma[sensor] = 0;
      flag_8horas[sensor] = 0;

      /*naranja parpadeando mediante interrupcion de 1 Hz*/

      break;

    case 4: //revisado
      if (estado_revisado[sensor] == 0 && en_mails) {
        mensaje_estado = "La maquina asociada al sensor: ";
        mensaje_estado += String(sensor);
        mensaje_estado += " est?? revisada";

        for (email_user = 0; email_user < NUM_EMAIL_USERS; email_user++) {
          result = sendEmail(asunto, remitente, mensaje_estado, EMAIL_LIST[email_user], false);
        }

        for (tel_user = 0; tel_user < NUM_TEL_USERS; tel_user++) {
          bot.sendMessage(CHAT_ID[tel_user], mensaje_estado, "");
        }
      }

      estado_arranque[sensor] = 1;
      estado_OK[sensor] = 0;
      estado_ACK[sensor] = 0;
      estado_revision[sensor] = 0;
      estado_revisado[sensor] = 1;
      flag_alarma[sensor] = 0;
      flag_8horas[sensor] = 0;


      //color naranja
      if (sensor == 0) {
        digitalWrite(P_L0VERDE, HIGH);
        digitalWrite(P_L0ROJO, HIGH);
      }
      else if (sensor == 1) {
        digitalWrite(P_L1VERDE, HIGH);
        digitalWrite(P_L1ROJO, HIGH);
      }
      else if (sensor == 2) {
        digitalWrite(P_L2VERDE, HIGH);
        digitalWrite(P_L2ROJO, HIGH);
      }
      else if (sensor == 3) {
        //digitalWrite(P_L3VERDE, HIGH);
        // digitalWrite(P_L3ROJO, HIGH);
      }

      break;
  } //fin switch

  

  return;
}

/******************************************************************************************/

void estados_automaticos() {
  for (int sens = 0; sens < N_SENSORES; sens++) {
    if (estado[sens] == 0 && (temp_actual[sens] <= temperatura_media[sens]/* && temp_actual[sens] >= temperatura_baja_extrema[sens]*/))
      estado[sens] = 1;

    else if (estado[sens] == 4 && temp_actual[sens] <= temperatura_baja[sens]/* && temp_actual[sens] >= temperatura_baja_extrema[sens]*/)
      estado[sens] = 1;
      
    else if ((estado[sens] == 0 || estado[sens] == 4) && flag_8horas[sens]) //alarma por tiempo al haber entrado N veces en el timer 0 sin haber reiniciado el contador de entrada
      estado[sens] = 2;

    else if (estado[sens] == 1 && (temp_actual[sens] >= temperatura_alta[sens] || temp_actual[sens] < temperatura_baja_extrema[sens]))
      estado[sens] = 2;

    else if (estado[sens] == 2 && temp_actual[sens] <= temperatura_baja[sens] && temp_actual[sens] >= temperatura_baja_extrema[sens])
      estado[sens] = 1;
  }
  return;
}

/******************************************************************************************/

/*
  Al marcar un fragmento de c??digo con el atributo IRAM_ATTR, estamos declarando que el c??digo compilado se colocar?? en la RAM interna (IRAM) del ESP32.
  De lo contrario, el c??digo se coloca en Flash. Y el flash en el ESP32 es mucho m??s lento que la RAM interna.
*/

void IRAM_ATTR ISR_ACK0() {
  //cambiar estado cuando se pulsa acuse de recibo (ESTADOS MANUALES)
  delay(150); // evita rebotes del pulsador


  flag_ACK0 = 1;

  if (estado[0] == 4) //falso error
    estado[0] = 1;

  else if (estado[0] == 3) //fin mantenimiento
    estado[0] = 4;

  else if (estado[0] == 2) //inicio mantenimiento
    estado[0] = 3;

  else if (estado[0] == 0 && (temp_actual[0] >= temperatura_alta[0] || temp_actual[0] < temperatura_baja_extrema[0])) //bypass arranque
    estado[0] = 2;

  else if (estado[0] == 0 && temp_actual[0] <= temperatura_alta[0] && temp_actual [0] > temperatura_baja_extrema[0]) //bypass arranque
    estado[0] = 1;

  return;
}

void IRAM_ATTR ISR_ACK1() {
  //cambiar estado cuando se pulsa acuse de recibo (ESTADOS MANUALES)
  delay(150); // evita rebotes del pulsador
  Serial.println("ISR_ACK1");

  flag_ACK1 = 1;

  if (estado[1] == 4) //falso error
    estado[1] = 1;

  else if (estado[1] == 3) //fin mantenimiento
    estado[1] = 4;

  else if (estado[1] == 2) //inicio mantenimiento
    estado[1] = 3;

  else if (estado[0] == 0 && (temp_actual[1] >= temperatura_alta[1] || temp_actual[1] < temperatura_baja_extrema[1])) //bypass arranque
    estado[0] = 2;

  else if (estado[0] == 0 && temp_actual[1] <= temperatura_alta[1] && temp_actual[1] > temperatura_baja_extrema[1]) //bypass arranque
    estado[0] = 1;

  return;
}

void IRAM_ATTR ISR_ACK2() {
  //cambiar estado cuando se pulsa acuse de recibo (ESTADOS MANUALES)
  delay(150); // evita rebotes del pulsador

  flag_ACK2 = 1;

  if (estado[2] == 4) //falso error
    estado[2] = 1;

  else if (estado[2] == 3) //fin mantenimiento
    estado[2] = 4;

  else if (estado[2] == 2) //inicio mantenimiento
    estado[2] = 3;

  else if (estado[0] == 0 && (temp_actual[2] >= temperatura_alta[2] || temp_actual[2] < temperatura_baja_extrema[2])) //bypass arranque
    estado[0] = 2;

  else if (estado[0] == 0 && temp_actual[2] <= temperatura_alta[2] && temp_actual[2] > temperatura_baja_extrema[2]) //bypass arranque
    estado[0] = 1;

  return;
}
/*
void IRAM_ATTR ISR_ACK3() {
  //cambiar estado cuando se pulsa acuse de recibo (ESTADOS MANUALES)
  delay(150); // evita rebotes del pulsador

  flag_ACK3 = 1;

  if (estado[3] == 4) //falso error
    estado[3] = 1;

  else if (estado[3] == 3) //fin mantenimiento
    estado[3] = 4;

  else if (estado[3] == 2) //inicio mantenimiento
    estado[3] = 3;

  else if (estado[0] == 0 && (temp_actual[3] >= temperatura_alta[3] || temp_actual[3] < temperatura_baja_extrema[3])) //bypass arranque
    estado[0] = 2;

  else if (estado[0] == 0 && temp_actual[3] <= temperatura_alta[3] && temp_actual[3] > temperatura_baja_extrema[3]) //bypass arranque
    estado[0] = 1;

  return;
  }
*/
/******************************************************************************************/

//Rutina de interrupcion del timer
void IRAM_ATTR onTimer() {

  flag_timer = 1;
  
    for(int k=0; k < N_SENSORES; k++){
      interruptCounter[k]++;
      if(estado[k] == 0 || estado[k] == 4){

        if(flag_8horas[k])
          interruptCounter[k] = 0;

        if(interruptCounter[k]>=N_TIMERMAX && !flag_alarma[k])
          flag_8horas[k] = 1;
      }
      else interruptCounter[k] = 0;
    }
}

/******************************************************************************************/

void IRAM_ATTR parp_mantenimiento() {

  if (parp1Hz == LOW)
    parp1Hz = HIGH;
  else
    parp1Hz = LOW;

  if (estado[0] == 3) { //color naranja
    digitalWrite(P_L0VERDE, parp1Hz);
    digitalWrite(P_L0ROJO, parp1Hz);
  }
  else if (estado[1] == 3) { //color naranja
    digitalWrite(P_L1VERDE, parp1Hz);
    digitalWrite(P_L1ROJO, parp1Hz);
  }
  else if (estado[2] == 3) { //color naranja
    digitalWrite(P_L2VERDE, parp1Hz);
    digitalWrite(P_L2ROJO, parp1Hz);
  }
  else if (estado[3] == 3) { //color naranja
    //digitalWrite(P_L3VERDE, parp1Hz);
    // digitalWrite(P_L3ROJO, parp1Hz);
  }

  if (!W_conexion)
    digitalWrite(P_LEDWIFI, parp1Hz);
}

/******************************************************************************************/

float calculo_temp(float Val_sensor, int sens) {
  float temp_preop = 0;
  float temperatura = 0;

  if (temperatura_alta[sens] <= -50.0f)  
    temp_preop = VALOR_CALIBRACION_menos80 * Val_sensor;

  else if (temperatura_alta[sens] > -50.0f && temperatura_alta[sens] <= 0)
    temp_preop = VALOR_CALIBRACION_menos20 * Val_sensor;

  else if (temperatura_alta[sens] > 0)
    temp_preop = -25.6684 + 14.6296 * Val_sensor;
    
  temperatura = temp_preop / 4096.0f;

  return temperatura;
}
/******************************************************************************************/

void handleNewMessages(int numNewMessages) {
  //Serial.println("handleNewMessages");
  int user = 0;
  String msg_alarma;
  //Serial.println(String(numNewMessages));
  for (int i = 0; i < numNewMessages; i++) {
    // Chat id of the requester
    String chat_id = String(bot.messages[i].chat_id);

    int j = 0;
    while (chat_id != CHAT_ID[j] && j <= NUM_TEL_USERS) {
      j++;
    }
    if (j >= NUM_TEL_USERS) {
      j = 0;
      bot.sendMessage(chat_id, "Usuario no autorizado", "");
      continue;
    }

    // Print the received message
    String text = bot.messages[i].text;
    String from_name = bot.messages[i].from_name;


    if (text == "/comandos") {
      String welcome = "Bienvenido, " + from_name + ".\n";
      welcome += "Utilice los siguientes comandos para controlar el frigorifico\n\n";
      welcome += "/comandos - Lista comandos \n";
      welcome += "/temperaturaX - Temperatura actual \n";
      welcome += "/estadoX - Estado de la m??quina \n";
      bot.sendMessage(chat_id, welcome, "");
    }

    if (text == cmd_temp_sensor0 && en_sensor0) {
      bot.sendMessage(chat_id, String(temp_actual[0]), "");
    }

    if (text == cmd_temp_sensor1 && en_sensor1) {
      bot.sendMessage(chat_id, String(temp_actual[1]), "");
    }

    if (text == cmd_temp_sensor2 && en_sensor2) {
      bot.sendMessage(chat_id, String(temp_actual[2]), "");
    }
    
    if (text == cmd_temp_sensor3 && en_sensor3) {
      bot.sendMessage(chat_id, String(temp_actual[3]), "");
    }
    
    if (text == cmd_estado_sensor0 && en_sensor0) {
      switch (estado[0]) {
        case 0:
          bot.sendMessage(chat_id, msg_arranque_sensor0, "");
          break;

        case 1:
          bot.sendMessage(chat_id, msg_OK_sensor0, "");
          break;

        case 2:
          bot.sendMessage(chat_id, msg_tempalta_sensor0, "");
          msg_alarma = "Temperatura actual: ";
          msg_alarma += String(temp_actual[0]);
          bot.sendMessage(chat_id, msg_alarma, "");
          break;

        case 3:
          bot.sendMessage(chat_id, msg_enRevision_sensor0, "");
          break;

        case 4:
          bot.sendMessage(chat_id, msg_revisado_sensor0, "");
          break;
      }
    }

    if (text == cmd_estado_sensor1 && en_sensor1) {
      switch (estado[1]) {
        case 0:
          bot.sendMessage(chat_id, msg_arranque_sensor1, "");
          break;

        case 1:
          bot.sendMessage(chat_id, msg_OK_sensor1, "");
          break;

        case 2:
          bot.sendMessage(chat_id, msg_tempalta_sensor1, "");
          msg_alarma = "Temperatura actual: ";
          msg_alarma += String(temp_actual[1]);
          bot.sendMessage(chat_id, msg_alarma, "");
          break;

        case 3:
          bot.sendMessage(chat_id, msg_enRevision_sensor1, "");
          break;

        case 4:
          bot.sendMessage(chat_id, msg_revisado_sensor1, "");
          break;
      }
    }

    if (text == cmd_estado_sensor2 && en_sensor2) {
      switch (estado[2]) {
        case 0:
          bot.sendMessage(chat_id, msg_arranque_sensor2, "");
          break;

        case 1:
          bot.sendMessage(chat_id, msg_OK_sensor2, "");
          break;

        case 2:
          bot.sendMessage(chat_id, msg_tempalta_sensor2, "");
          msg_alarma = "Temperatura actual: ";
          msg_alarma += String(temp_actual[2]);
          bot.sendMessage(chat_id, msg_alarma, "");
          break;

        case 3:
          bot.sendMessage(chat_id, msg_enRevision_sensor2, "");          
          break;

        case 4:
          bot.sendMessage(chat_id, msg_revisado_sensor2, "");
          break;
      }
    }
    
    if (text == cmd_estado_sensor3 && en_sensor3) {
        switch(estado[3]){
          case 0:
              bot.sendMessage(chat_id, msg_arranque_sensor3, "");
          break;

          case 1:
              bot.sendMessage(chat_id, msg_OK_sensor3, "");
          break;

          case 2:
              bot.sendMessage(chat_id, msg_tempalta_sensor3, "");
              msg_alarma = "Temperatura actual: ";
              msg_alarma += String(temp_actual[3]);
              bot.sendMessage(chat_id, msg_alarma, "");
          break;

          case 3:
              bot.sendMessage(chat_id, msg_enRevision_sensor3, "");
          break;

          case 4:
              bot.sendMessage(chat_id, msg_revisado_sensor3, "");
          break;
        }
      }

  }
}

/******************************************************************************************/
