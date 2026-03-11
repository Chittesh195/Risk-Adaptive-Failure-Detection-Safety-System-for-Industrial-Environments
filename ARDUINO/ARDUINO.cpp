#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>
#include <Servo.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define DHTPIN 2
#define PIRPIN 3
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);
Servo doorServo;

unsigned long previousMillis = 0;
const long interval = 3000;

int screenState = 0;
bool doorRunning = false;
unsigned long doorTimer = 0;

// --------- ICONS ----------

// Human 👤
void drawHuman() {
  display.fillCircle(15, 25, 6, WHITE);
  display.fillRect(12, 32, 6, 15, WHITE);
}

// Thermometer 🌡
void drawTemp() {
  display.drawRect(10, 20, 6, 25, WHITE);
  display.fillCircle(13, 48, 6, WHITE);
}

// Water 💧
void drawHumidity() {
  display.fillTriangle(13, 20, 5, 40, 21, 40, WHITE);
  display.fillCircle(13, 40, 6, WHITE);
}

// Door Closed 🚪
void drawDoorClosed() {
  display.drawRect(8, 20, 20, 30, WHITE);
  display.fillCircle(24, 35, 2, WHITE);
}

// Door Open 🚪➡
void drawDoorOpen() {
  display.drawRect(8, 20, 20, 30, WHITE);
  display.drawLine(8, 20, 30, 15, WHITE);
  display.fillCircle(28, 30, 2, WHITE);
}

// ---------- STARTUP ANIMATION ----------
void startupAnimation() {

  unsigned long start = millis();

  while (millis() - start < 5000) {

    display.clearDisplay();

    display.setTextSize(2);
    display.setCursor(10, 10);
    display.println("SMART");

    display.setCursor(10, 30);
    display.println("INDUSTRY");

    display.drawRect(10, 55, 108, 6, WHITE);

    int width = map(millis() - start, 0, 5000, 0, 104);
    display.fillRect(12, 57, width, 2, WHITE);

    display.display();
  }
}

void setup() {

  Wire.begin();
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.setTextColor(WHITE);

  startupAnimation();

  pinMode(PIRPIN, INPUT);

  dht.begin();
  doorServo.attach(9);
  doorServo.write(0);

  display.clearDisplay();
  display.setCursor(20, 25);
  display.println("Warming Sensor...");
  display.display();
  delay(1000);
}

void loop() {

  unsigned long currentMillis = millis();
  int motion = digitalRead(PIRPIN);

  // ===== MOTION DETECTED =====
  if (motion == HIGH && !doorRunning) {

    doorRunning = true;
    doorTimer = currentMillis;

    display.clearDisplay();
    drawHuman();
    display.setTextSize(1);
    display.setCursor(50, 25);
    display.println("Motion Detected");
    display.display();

    doorServo.write(90);   // OPEN DOOR
  }

  // ===== DOOR CONTROL =====
  if (doorRunning) {

    if (currentMillis - doorTimer < 3000) {

      display.clearDisplay();
      drawDoorOpen();
      display.setCursor(50, 25);
      display.println("Door Opening");
      display.display();
    }

    else if (currentMillis - doorTimer < 6000) {

      display.clearDisplay();
      drawDoorClosed();
      display.setCursor(50, 25);
      display.println("Door Closing");
      display.display();

      doorServo.write(0);
    }

    else {
      doorRunning = false;
      screenState = 0;
      previousMillis = currentMillis;
    }

    return;
  }

  // ===== NORMAL DISPLAY =====
  if (currentMillis - previousMillis >= interval) {

    previousMillis = currentMillis;

    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();

    display.clearDisplay();
    display.setTextSize(1);

    if (screenState == 0) {

      drawTemp();
      display.setCursor(50, 20);
      display.println("Temperature");
      display.setTextSize(2);
      display.setCursor(50, 35);
      display.print(temp);
      display.print(" C");
      screenState = 1;
    }

    else if (screenState == 1) {

      drawHumidity();
      display.setCursor(50, 20);
      display.println("Humidity");
      display.setTextSize(2);
      display.setCursor(50, 35);
      display.print(hum);
      display.print(" %");
      screenState = 2;
    }

    else {

      drawHuman();
      display.setCursor(50, 25);
      display.println("No Motion");
      screenState = 0;
    }

    display.display();
  }
}
