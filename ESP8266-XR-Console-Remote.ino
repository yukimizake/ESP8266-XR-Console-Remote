// 4-button WiFi remote for Behringer and Midas XR/MR series mixing consoles
// Designed for the Wemos D1 devboard, 4 push-buttons, 4 status LEDs

// OSC lib by Yotam Mann and Adrian Freed : https://github.com/CNMAT/OSC
// LEDs are common cathode (common negative)
// A and B switches have pull-down resistors
// C and D switches have pull-up resistors

#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <OSCData.h>

#define DEBOUNCE_DELAY_MS 40     // Button press debounce delay
#define REPEAT_MESSAGE 1         // Number of times an OSC message is sent
#define WAIT_AFTER_MESSAGE_MS 10 // Pause in ms after an OSC message is sent 
#define XREMOTE_REFRESH_MS 5000  // Delay between two /xremote messages

// Buttons and LEDs pins definition 
#define BUTTON_A_PIN D1
#define BUTTON_B_PIN D2
#define BUTTON_C_PIN D3
#define BUTTON_D_PIN D4
#define BUTTON_A_LED D5
#define BUTTON_B_LED D6
#define BUTTON_C_LED D7
#define BUTTON_D_LED D8

const char* ssid = ""; // Wifi SSID
const char* password = ""; // WiFi password

WiFiUDP Udp;
const IPAddress consoleIp(192, 168, 0, 6);  // XR/MR mixing console IP address
const unsigned int consolePort = 10024;     // Console OSC port (for outgoing OSC messages : 10023 X32/M32, 10024 for XR/MR series)
const unsigned int localUdpPort = 8888;     // Local OSC port (for incoming OSC messages)
OSCErrorCode error;

bool bMonoOn = false;
bool bCompOn = false;
bool bHpfOn = false;
bool bMuteOn = false;

unsigned long previousMillis;

void setup()
{
  pinMode(BUTTON_A_PIN, INPUT);
  pinMode(BUTTON_B_PIN, INPUT);
  pinMode(BUTTON_C_PIN, INPUT);
  pinMode(BUTTON_D_PIN, INPUT);
  pinMode(BUTTON_A_LED, OUTPUT);
  pinMode(BUTTON_B_LED, OUTPUT);
  pinMode(BUTTON_C_LED, OUTPUT);
  pinMode(BUTTON_D_LED, OUTPUT);
  digitalWrite(BUTTON_A_LED, LOW);
  digitalWrite(BUTTON_B_LED, LOW);
  digitalWrite(BUTTON_C_LED, LOW);
  digitalWrite(BUTTON_D_LED, LOW);
  
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.printf("\nConnecting to %s ", ssid);
  Serial.println();

  WiFi.begin(ssid, password);

  waitForWifi();

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Opening UDP port");
  Udp.begin(localUdpPort);

  flashALittle();
  
  loadSnapshot(1); // Initialize with snapshot 1
  Serial.println("Running");
}

void loop()
{
  if (digitalRead(BUTTON_A_PIN) == HIGH)
  {
    if (bHpfOn == false)
    {
      hpFilter(1);
    }
    else
    {
      hpFilter(0);
    }

    while (digitalRead(BUTTON_A_PIN) == HIGH) // Waits for key release
    {
      delay(1);
    }
    delay(DEBOUNCE_DELAY_MS);
  }

  if (digitalRead(BUTTON_B_PIN) == HIGH)
  {
    if (bCompOn == false)
    {
      loadSnapshot(2);
    }
    else
    {
      loadSnapshot(1);
    }

    while (digitalRead(BUTTON_B_PIN) == HIGH) // Waits for key release
    {
      delay(1);
    }
    delay(DEBOUNCE_DELAY_MS);
  }

  if (digitalRead(BUTTON_C_PIN) == LOW)
  {
    if (bMonoOn == false)
    {
      monoMonitoring(1);
    }
    else
    {
      monoMonitoring(0);
    }

    while (digitalRead(BUTTON_C_PIN) == LOW) // Waits for key release
    {
      delay(1);
    }
    delay(DEBOUNCE_DELAY_MS);
  }

  if (digitalRead(BUTTON_D_PIN) == LOW)
  {
    if (bMuteOn == false)
    {
      muteMonitoring(1);
    }
    else
    {
      muteMonitoring(0);
    }

    while (digitalRead(BUTTON_D_PIN) == LOW) // Waits for key release
    {
      delay(1);
    }
    delay(DEBOUNCE_DELAY_MS);
  }

  waitForWifi();
  keepSubscription();
  processIncomingMessages();
}

void loadSnapshot(int snapshotIdx)
{
  sendOscMessage("/-snap/load", snapshotIdx);
}

void monoMonitoring(int onOff)
{
  sendOscMessage("/config/solo/mono", onOff);
}

void hpFilter(int onOff)
{
  if (onOff == 1)
  {
    // Sets a low shelf EQ too
    sendOscMessage("/ch/01/eq/1/type", 1);      // Low-shelf
    processIncomingMessages();
    sendOscMessage("/ch/01/eq/1/f", 0.335f);    // 200 Hz
    processIncomingMessages();
    sendOscMessage("/ch/01/eq/1/g", 0.4f);      // -3 dB
    processIncomingMessages();
    sendOscMessage("/ch/01/eq/on", 1);          // EQ on
    processIncomingMessages();
    sendOscMessage("/ch/01/preamp/hpf", 0.57f); // 110 Hz
    processIncomingMessages();
  }
  else
  {
    // Disables the low shelf EQ
    sendOscMessage("/ch/01/eq/1/g", 0.5f);      // 0 dB
    processIncomingMessages();
  }
  sendOscMessage("/ch/01/preamp/hpon", onOff);
}

void muteMonitoring(int onOff)
{
  sendOscMessage("/config/solo/mute", onOff);
}

void processIncomingMessages()
{
  // Processes incoming OSC messages
  OSCMessage msg;
  int size = Udp.parsePacket();

  if (size > 0) {
    while (size--) {
      msg.fill(Udp.read());
    }
    if (!msg.hasError()) {
      msg.dispatch("/config/solo/mono", monoHandle);
      msg.dispatch("/ch/01/preamp/hpon", hpfHandle);
      msg.dispatch("/-snap/index", snapHandle);
      msg.dispatch("/config/solo/mute", muteHandle);
    } else {
      error = msg.getError();
      Serial.print("error: ");
      Serial.println(error);
    }
  }
}

void hpfHandle(OSCMessage &msg)
{
  int value = msg.getInt(0);
  if (value == 0)
  {
    digitalWrite(BUTTON_A_LED, LOW);
    bHpfOn = false;
  }
  else
  {
    digitalWrite(BUTTON_A_LED, HIGH);
    bHpfOn = true;
  }
}

void snapHandle(OSCMessage &msg)
{
  int value = msg.getInt(0);
  if (value == 2)
  {
    digitalWrite(BUTTON_B_LED, HIGH);
    bCompOn = true;
  }
  else
  {
    digitalWrite(BUTTON_B_LED, LOW);
    bCompOn = false;
  }
}

void monoHandle(OSCMessage &msg)
{
  int value = msg.getInt(0);
  if (value == 0)
  {
    digitalWrite(BUTTON_C_LED, LOW);
    bMonoOn = false;
  }
  else
  {
    digitalWrite(BUTTON_C_LED, HIGH);
    bMonoOn = true;
  }
}

void muteHandle(OSCMessage &msg)
{
  int value = msg.getInt(0);
  if (value == 0)
  {
    digitalWrite(BUTTON_D_LED, LOW);
    bMuteOn = false;
  }
  else
  {
    digitalWrite(BUTTON_D_LED, HIGH);
    bMuteOn = true;
  }
}

void keepSubscription()
{
  long currentMillis = millis();

  if (currentMillis - previousMillis > XREMOTE_REFRESH_MS)
  {
    // Subscribe to everything
    sendOscMessage("/xremote");

    // Polls possibly missed changes
    sendOscMessage("/config/solo/mono");
    sendOscMessage("/ch/01/preamp/hpon");
    sendOscMessage("/-snap/index");
    sendOscMessage("/config/solo/mute");
    previousMillis = currentMillis;
  }
}

void sendOscMessage(char command[])
{
  OSCMessage msg(command);
  for (int i = 0; i < REPEAT_MESSAGE; i++)
  {
    Udp.beginPacket(consoleIp, consolePort);
    msg.send(Udp);
    Udp.endPacket();
  }
  msg.empty();
  delay(WAIT_AFTER_MESSAGE_MS);
}

void sendOscMessage(char command[], int value)
{
  OSCMessage msg(command);
  msg.add(value);
  for (int i = 0; i < REPEAT_MESSAGE; i++)
  {
    Udp.beginPacket(consoleIp, consolePort);
    msg.send(Udp);
    Udp.endPacket();
  }
  msg.empty();
  delay(WAIT_AFTER_MESSAGE_MS);
}

void sendOscMessage(char command[], float value)
{
  OSCMessage msg(command);
  msg.add(value);
  for (int i = 0; i < REPEAT_MESSAGE; i++)
  {
    Udp.beginPacket(consoleIp, consolePort);
    msg.send(Udp);
    Udp.endPacket();
  }
  msg.empty();
  delay(WAIT_AFTER_MESSAGE_MS);
}

void waitForWifi()
{
  bool ledAState = digitalRead(BUTTON_A_LED);
  bool ledBState = digitalRead(BUTTON_B_LED);
  bool ledCState = digitalRead(BUTTON_C_LED);
  bool ledDState = digitalRead(BUTTON_D_LED);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(BUTTON_A_LED, LOW);
    digitalWrite(BUTTON_B_LED, LOW);
    digitalWrite(BUTTON_C_LED, LOW);
    digitalWrite(BUTTON_D_LED, LOW);
    delay(150);
    Serial.print(".");
    digitalWrite(BUTTON_A_LED, HIGH);
    digitalWrite(BUTTON_B_LED, HIGH);
    digitalWrite(BUTTON_C_LED, HIGH);
    digitalWrite(BUTTON_D_LED, HIGH);
    delay(150);
  }
  digitalWrite(BUTTON_A_LED, ledAState);
  digitalWrite(BUTTON_B_LED, ledBState);
  digitalWrite(BUTTON_C_LED, ledCState);
  digitalWrite(BUTTON_D_LED, ledDState);
}

void flashALittle()
{
  bool ledAState = digitalRead(BUTTON_A_LED);
  bool ledBState = digitalRead(BUTTON_B_LED);
  bool ledCState = digitalRead(BUTTON_C_LED);
  bool ledDState = digitalRead(BUTTON_D_LED);
  for (int i = 0; i < 5; i++)
  {
    digitalWrite(BUTTON_A_LED, LOW);
    digitalWrite(BUTTON_B_LED, LOW);
    digitalWrite(BUTTON_C_LED, LOW);
    digitalWrite(BUTTON_D_LED, LOW);
    delay(270);
    digitalWrite(BUTTON_A_LED, HIGH);
    digitalWrite(BUTTON_B_LED, HIGH);
    digitalWrite(BUTTON_C_LED, HIGH);
    digitalWrite(BUTTON_D_LED, HIGH);
    delay(30);
  }
  digitalWrite(BUTTON_A_LED, ledAState);
  digitalWrite(BUTTON_B_LED, ledBState);
  digitalWrite(BUTTON_C_LED, ledCState);
  digitalWrite(BUTTON_D_LED, ledDState);
}
