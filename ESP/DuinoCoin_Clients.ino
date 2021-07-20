/*
  DuinoCoin_Clients.ino
  created 10/05/2021
  by Luiz H. Cassettari
  Update by AzagraMac
  Update: 20/07/2021
*/

#if ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#endif

#if ESP32
#include <WiFi.h>
#include <WiFiClient.h>
#endif

#define CLIENTS 5                                                   // Define max clients, limit 10, optimal 5

#define END_TOKEN  '\n'
#define SEP_TOKEN  ','

const char * host = "51.15.127.80";                                 // Static server IP
const int ports[] = {2812, 2817, 2816, 2815, 2814, 2813, 2811};     // Static server PORT

WiFiClient clients[CLIENTS];
byte clientsWaitJob[CLIENTS];
int clientsShares[CLIENTS];
String clientsBuffer[CLIENTS];
unsigned long clientsTimes[CLIENTS];
unsigned long clientsTimeOut[CLIENTS];
byte clientsBadJob[CLIENTS];

unsigned long clientsConnectTime = 0;

bool clients_connected(byte i)
{
  return clients[i].connected();
}

bool clients_connect(byte i)
{
  int port_size = sizeof(ports) / sizeof(ports[0]);
  int port_number = random(0, port_size - 1);

  if (clients[i].connected())
  {
    return true;
  }

  wire_readLine(i);

  Serial.print("[AVR " + String(i) + "]");
  Serial.println(" Connecting to Duino-Coin server with port " + String(ports[port_number]) + "...");

  if (!clients[i].connect(host, ports[port_number]))
  {
    Serial.print("[AVR " + String(i) + "]");
    Serial.println(F(" Connection failed"));
    return false;
  }
  clientsShares[i] = 0;
  clientsWaitJob[i] = 4;
  clientsBadJob[i] = 0;
  clientsTimes[i] = millis();
  clientsTimeOut[i] = millis();
  clientsBuffer[i] = "";
  return true;
}

bool clients_stop(byte i)
{
  clients[i].stop();
  return true;
}

int client_i = 0;

void clients_loop()
{
  if (clients_runEvery(clientsConnectTime))
  {
    clientsConnectTime = 55000;
    for (client_i = 0; client_i < CLIENTS; client_i++)
    {
      int i = client_i;
      if (wire_exists(i + 1) && !clients_connect(i))
      {
        break;
      }
    }
  }

  for (client_i = 0; client_i < CLIENTS; client_i++)
  {
    int i = client_i;
    if (wire_exists(i + 1) && clients_connected(i))
    {

      if (clientsWaitJob[i] == 4)
      {
        clients_waitRequestVersion(i);
      }

      if (clientsWaitJob[i] == 0)
      {
        clients_requestJob(i);
      }

      if (clientsWaitJob[i] == 1)
      {
        clients_waitRequestJob(i);
      }

      if (clientsWaitJob[i] == 2)
      {
        clients_sendJobDone(i);
      }

      if (clientsWaitJob[i] == 3)
      {
        clients_waitFeedbackJobDone(i);
      }

      if (millis() - clientsTimeOut[i] > 55000)
      {
        Serial.println("[AVR " + String(i) + "]" + " --------------- TIMEOUT ------------- ");
        clients_stop(i);
      }

    }
  }
}

void clients_waitRequestVersion(byte i)
{
  if (clients[i].available()) {
    String buffer = clients[i].readStringUntil(END_TOKEN);
    Serial.println("[AVR " + String(i) + "] Server version " + buffer);
    clientsWaitJob[i] = 0;
    clientsTimeOut[i] = millis();
  }
}

void clients_requestJob(byte i)
{
  Serial.print("[AVR " + String(i) + "]");
  Serial.println(" Requesting job for: " + String(ducouser));
  clients[i].print("JOB," + String(ducouser) + "," + JOB);
  clientsWaitJob[i] = 1;
  clientsTimeOut[i] = millis();
}

void clients_waitRequestJob(byte i)
{
  String clientBuffer = clients_readData(i);
  if (clientBuffer.length() > 0)
  {
    String hash = getValue(clientBuffer, SEP_TOKEN, 0);
    String job = getValue(clientBuffer, SEP_TOKEN, 1);
    unsigned int diff = getValue(clientBuffer, SEP_TOKEN, 2).toInt();

    Serial.print("[AVR " + String(i) + "]");
    Serial.println(" Difficulty: " + String(diff));

    wire_sendJob(i + 1, hash, job, diff);
    clientsWaitJob[i] = 2;
    clientsTimeOut[i] = millis();
  }
}

void clients_sendJobDone(byte i)
{
  String responseJob = wire_readLine(i + 1);
  if (responseJob.length() > 0)
  {
    StreamString response;
    response.print(responseJob);

    int job = response.readStringUntil(',').toInt();
    int time = response.readStringUntil(',').toInt();
    String id = response.readStringUntil('\n');
    float HashRate = job / (time * .000001f);

    if (id.length() > 0) id = "," + id;

    //String identifier = String(rigIdentifier) + "AVR " + String(i);
    String identifier = "AVR " + String(i);

    clients[i].print(String(job) + "," + String(HashRate, 2) + "," + MINER + "," + String(identifier) + id);
    Serial.print("[AVR " + String(i) + "]");
    Serial.println(" Job Done: (" + String(job) + ")" + " Hashrate: " + String(HashRate));
    clientsWaitJob[i] = 3;
    clientsTimeOut[i] = millis();
  }
}

void clients_waitFeedbackJobDone(byte i)
{
  String clientBuffer = clients_readData(i);
  if (clientBuffer.length() > 0)
  {
    unsigned long time = (millis() - clientsTimes[i]);
    clientsShares[i]++;
    int Shares = clientsShares[i];
    Serial.print("[AVR " + String(i) + "]");
    Serial.println(" Job " + clientBuffer  + ": Share #" + String(Shares) + " " + timeString(time));
    clientsWaitJob[i] = 0;

    if (clientBuffer == "BAD")
    {
      Serial.println("[AVR " + String(i) + "]" + "BAD BAD BAD BAD");
      if (clientsBadJob[i]++ > 3)
        clients_stop(i);
    }
    else
    {
      clientsBadJob[i] = 0;
    }
    clientsTimeOut[i] = millis();
  }
}

String clients_string()
{
  int i = 0;
  String str;
  str += "Connected AVR ";
  str += "[";
  str += " ";
  for (i = 0; i < CLIENTS; i++)
  {
    if (wire_exists(i + 1))
    {
      str += (i);
      str += " ";
    }
  }
  str += "]";
  return str;
}

String timeString(unsigned long t) {
  String str = "";
  t /= 1000;
  int s = t % 60;
  int m = (t / 60) % 60;
  int h = (t / 3600);
  str += h;
  str += ":";
  if (m < 10) str += "0";
  str += m;
  str += ":";
  if (s < 10) str += "0";
  str += s;
  return str;
}


String clients_readData(byte i)
{
  String str = "";
  while (clients[i].available()) {
    char c = clients[i].read();
    if (c == END_TOKEN)
    {
      str = clientsBuffer[i];
      clientsBuffer[i] = "";
    }
    else
      clientsBuffer[i] += c;
  }
  return str;
}

// https://stackoverflow.com/questions/9072320/split-string-into-string-array
String getValue(String data, char separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex && found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

String waitForClientData(int i) {
  unsigned long previousMillis = millis();
  unsigned long interval = 10000;

  String buffer;
  while (clients[i].connected()) {
    if (clients[i].available()) {
      Serial.println(clients[i].available());
      buffer = clients[i].readStringUntil(END_TOKEN);
      if (buffer.length() == 1 && buffer[0] == END_TOKEN)
        buffer = "???\n"; // NOTE: Should never happen...
      if (buffer.length() > 0)
        break;
    }
    if (millis() - previousMillis >= interval) break;
    handleSystemEvents();
  }
  return buffer;
}

boolean clients_runEvery(unsigned long interval)
{
  static unsigned long previousMillis = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval)
  {
    previousMillis = currentMillis;
    return true;
  }
  return false;
}
