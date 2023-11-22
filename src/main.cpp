/**
 * Created by K. Suwatchai (Mobizt)
 *
 * Email: k_suwatchai@hotmail.com
 *
 * Github: https://github.com/mobizt/Firebase-ESP8266
 *
 * Copyright (c) 2023 mobizt
 *
 */

/** This example will show how to authenticate using
 * the legacy token or database secret with the new APIs (using config and auth data).
 */

#include <Arduino.h>
#include <WiFi.h>
#include <FirebaseESP32.h>

// Provide the RTDB payload printing info and other helper functions.
#include <addons/RTDBHelper.h>

/* 2. If work with RTDB, define the RTDB URL and database secret */
#define DATABASE_URL "https://projeto-healthcare-808d1-default-rtdb.firebaseio.com/" //<databaseName>.firebaseio.com or <databaseName>.<region>.firebasedatabase.app
#define DATABASE_SECRET "osNnJHAzSlqnasdZYrsblWAJsJ189OFclTrXWsJ0"

#define WIFI_SSID "LAPTOP77 8270"
#define WIFI_PASSWORD "1412&Jf3"
// #define FIREBASE_HOST "https://projeto-healthcare-808d1-default-rtdb.firebaseio.com/"
// #define FIREBASE_AUTH "osNnJHAzSlqnasdZYrsblWAJsJ189OFclTrXWsJ0"

/* 3. Define the Firebase Data object */
FirebaseData fbdoOxy;
FirebaseData fbdoHeart;
FirebaseData fbdoOxyLevel;
FirebaseData fbdoCalling;

/* 4, Define the FirebaseAuth data for authentication data */
FirebaseAuth auth;

/* Define the FirebaseConfig data for config data */
FirebaseConfig config;

// Pins Potenciometros
#define PWM33_Pin 33
int oxygenADC = 0;
int oxygenSatLvl = 0;
const int warningSatLvl = 89;

#define PWM34_Pin 34
int heartRateADC = 0;
int heartRate = 0;
const int warningHrUpper = 200;
const int warningHrLower = 30;

#define PWM35_Pin 35
int oxygenLevelADC = 0;
int oxygenLevel = 0;

// Pins Leds
#define Led12_Pin 12
#define Led14_Pin 14

unsigned long dataMillis = 0;
int count = 0;

// Funcs
void readOxygenSaturation(void *param);
void blinkLed(void *param);
void readHeartRate(void *param);
void monitorVitalSigns(void *param);
void commandLine(void *param);
void setOxygenLevel(void *param);
void callAttendant(void *param);

void pwmSetvalue(int value);

int cliInput;

void setup()
{
	Serial.begin(9600);

	WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
	Serial.print("Connecting to Wi-Fi");
	while (WiFi.status() != WL_CONNECTED)
	{
		Serial.print(".");
		delay(300);
	}
	Serial.println();
	Serial.print("Connected with IP: ");
	Serial.println(WiFi.localIP());
	Serial.println();

	Serial.printf("Firebase Client v%s\n\n", FIREBASE_CLIENT_VERSION);

	/* Assign the certificate file (optional) */
	// config.cert.file = "/cert.cer";
	// config.cert.file_storage = StorageType::FLASH;

	/* Assign the database URL and database secret(required) */
	config.database_url = DATABASE_URL;
	config.signer.tokens.legacy_token = DATABASE_SECRET;

	// Comment or pass false value when WiFi reconnection will control by your code or third party library e.g. WiFiManager
	Firebase.reconnectNetwork(true);

	// Since v4.4.x, BearSSL engine was used, the SSL buffer need to be set.
	// Large data transmission may require larger RX buffer, otherwise connection issue or data read time out can be occurred.
	fbdoOxy.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
	fbdoHeart.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
	fbdoOxyLevel.setBSSLBufferSize(4096 /* Rx buffer size in bytes from 512 - 16384 */, 1024 /* Tx buffer size in bytes from 512 - 16384 */);
	/* Initialize the library with the Firebase authen and config */
	Firebase.begin(&config, &auth);

	// Or use legacy authenticate method
	// Firebase.begin(DATABASE_URL, DATABASE_SECRET);

	pinMode(Led14_Pin, OUTPUT);
	pinMode(Led12_Pin, OUTPUT);

	Serial.printf("Escolha o modo de monitoramento a ser utilizado\r\n a -> Resumo de todos os sinais vitais\r\n b -> batimento cardaco c/ detalhes\r\n o -> saturacao de oxigenio c/ detalhes\r\n n -> nivel de oxigenio\r\n");
	xTaskCreate(commandLine, "commandLine", 8192, NULL, 0, NULL);
	xTaskCreate(readOxygenSaturation, "readOxygenSaturation", 8192, NULL, 0, NULL);
	xTaskCreate(blinkLed, "blinkLed", 8192, NULL, 0, NULL);
	xTaskCreate(readHeartRate, "readHeartRate", 8192, NULL, 0, NULL);
	xTaskCreate(monitorVitalSigns, "monitorVitalSigns", 8192, NULL, 0, NULL);
	xTaskCreate(setOxygenLevel, "setOxygenLevel", 8192, NULL, 0, NULL);
	xTaskCreate(callAttendant, "callAttendant", 8192, NULL, 10, NULL);
}

void sendString(char *path, char *string)
{	
	if (path == "/project/oxygen")
	{
		Firebase.setString(fbdoOxy, path, string);
	}
	else if (path == "/project/heartRate")
	{
		Firebase.setString(fbdoHeart, path, string);
	}
	else if (path == "/project/oxygenLevel")
	{
		Firebase.setString(fbdoOxyLevel, path, string);
	}
}

void callAttendant(void *param)
{
	for(;;)
	{
		Firebase.getInt(fbdoCalling,"/attendant/callAttendant");
		bool calling = fbdoCalling.boolData();

		if (calling)
		{
			digitalWrite(Led14_Pin, HIGH);
			vTaskDelay(200);
			digitalWrite(Led14_Pin, LOW);
			vTaskDelay(100);
			digitalWrite(Led14_Pin, HIGH);
			vTaskDelay(1000);
			digitalWrite(Led14_Pin, LOW);
		}
		else
		{
			digitalWrite(Led14_Pin, LOW);
		}
	}
}

void printLogs(char type, char *data)
{
	if (cliInput == type)
	{
		Serial.printf(data);
	}
}

void blinkLed(void *param)
{
	for (;;)
	{
		Serial.printf("");

		if (heartRate <= 0)
		{
			digitalWrite(Led14_Pin, HIGH);
		}
		else if ((heartRate >= warningHrUpper || heartRate <= warningHrLower) || (oxygenSatLvl <= warningSatLvl))
		{
			digitalWrite(Led14_Pin, HIGH);
			vTaskDelay(200);
			digitalWrite(Led14_Pin, LOW);
			vTaskDelay(200);
		}
		else
		{
			digitalWrite(Led14_Pin, LOW);
		}
	}
}

void readHeartRate(void *param)
{
	for (;;)
	{
		char str[75] = "";
		char details[25] = "";

		const int adcRange = 4000;
		const int heartRateRange = 220;
		const int critHeartRate = 0;

		char fireBaseData[20];
		
		heartRateADC = analogRead(PWM34_Pin);
		heartRate = (int)(((heartRateADC * heartRateRange) / adcRange) + critHeartRate);

		sprintf(fireBaseData, "%d", heartRate);
		sendString("/project/heartRate", fireBaseData);

		if (heartRate >= 150)
		{
			snprintf(details, 25, "Taquicardia/Estresse");
		}
		else if (heartRate > 100)
		{
			snprintf(details, 25, "Acelerado");
		}
		else if (heartRate >= 60 && heartRate <= 100)
		{
			snprintf(details, 25, "Em repouso");
		}
		else if (heartRate < 60 && heartRate > 30)
		{
			snprintf(details, 25, "Dormindo");
		}
		else if (heartRate < 30 && heartRate > 0)
		{
			snprintf(details, 25, "Crtico");
		}
		else if (heartRate <= 0)
		{
			snprintf(details, 25, "Parada cardaca");
		}

		snprintf(str, 75, "Batimento cardiaco: %d bpm [%s]\r\n", heartRate, details);
		printLogs('b', str);
	}
}

void monitorVitalSigns(void *param)
{
	for (;;)
	{
		char str[100] = "";
		snprintf(str, 100, "Saturação de oxigênio: %d%% | Batimento cardíaco: %d bpm | Nível Oxigênio: %d%%\r\n", oxygenSatLvl, heartRate, oxygenLevel);
		printLogs('a', str);
		vTaskDelay(500);
	}
}

void readOxygenSaturation(void *param)
{
	for (;;)
	{
		char str[75] = "";
		char details[25] = "";

		char fireBaseData[20] = "";

		const int adcRange = 4000;
		const int oxygenSatRange = 15;
		const int oxygenCritSat = 85;
		const int warningSatLvl = 89;

		oxygenADC = analogRead(PWM33_Pin);
		oxygenSatLvl = (int)(((oxygenADC * oxygenSatRange) / adcRange) + oxygenCritSat);
		sprintf(fireBaseData, "%d", oxygenSatLvl);
		sendString("/project/oxygen", fireBaseData);

		if (oxygenSatLvl < 90)
		{
			snprintf(details, 25, "Vermelho");
		}
		else if (oxygenSatLvl >= 90 && oxygenSatLvl < 96)
		{
			snprintf(details, 25, "Amarelo");
		}
		else if (oxygenSatLvl >= 96)
		{
			snprintf(details, 25, "Verde");
		}

		snprintf(str, 75, "Saturacao de oxigenio: %d%% [%s]\r\n", oxygenSatLvl, details);
		printLogs('o', str);
	}
}

void setOxygenLevel(void *param)
{
	const int adcRange = 4096;
	const int oxygenLvlRange = 100;
	const int lowestOxygenLvl = 0;

	for(;;)
	{
		char fireBaseData[20] = "";
		char str[75] = "";

		vTaskDelay(100);

		oxygenLevelADC = analogRead(PWM35_Pin);
		
		oxygenLevel = (int)(((oxygenLevelADC * oxygenLvlRange) / adcRange) + lowestOxygenLvl);

		pwmSetvalue(oxygenLevel);

		sprintf(fireBaseData, "%d", oxygenLevel);
		sendString("/project/oxygenLevel", fireBaseData);

		snprintf(str, 75, "Nivel de oxigenio: %d\r\n", oxygenLevel);
		printLogs('n', str);
	}
}

void pwmSetvalue(int value)
{
	analogWrite(Led12_Pin, value);
}

void commandLine(void *param)
{
	for (;;)
	{
		int temp = Serial.read();
		if (temp == 'a')
		{
			cliInput = temp;
		}
		else if (temp == 'b')
		{
			cliInput = temp;
		}
		else if (temp == 'n')
		{
			cliInput = temp;
		}
		else if (temp == 'o')
		{
			cliInput = temp;
		}
		
		
		vTaskDelay(500);
	}
}

void loop()
{
}
