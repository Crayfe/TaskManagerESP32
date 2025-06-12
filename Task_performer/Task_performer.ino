/*Sistema gestor de tareas programables. El cerebro del proyecto es una ESP32 conectada a una placa de desarrollo personalizada
 * La placa de desarrollo consta de un display OLED para mostrar información, un lector de tarjetas SD para leer y guardar información,
 * un endocer rotativo, un piezo para generar señales acústicas y unos pulsadores para enriquecer interacciones con un usuario. El pin 17 está preparado para manejar motores
 * a través de una electrónica externa para no dañar el ESP32 y que tenga potencia suficiente para funcionar, también está el pin 4
 * preparado para múltiples propósitos, pero está más pensado para usarlo para un sensor de temperatura y humedad DHT22.
 * 
 * Para obtener la hora actualizada se utiliza un servidor ntp, para eyo se hace uso de la funcionalidad wifi del ESP3, además de una
 * tarjeta ds para leer las credenciales de tal forma que se pueden cambiar sin la necesidad de volver a programar el ESP32
*/
// ==== INCLUDES ====
#include "DHT.h"
#define DHT_TYPE DHT22
#define PIEZO_CHANNEL 0
#include <WiFi.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "time.h"

// ==== OLED ====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// ==== PINS ====
#define PIEZO_PIN 26
#define DHT_PIN 4
#define PUMP_PIN 17
#define SD_CS 5

// ==== OBJECTS ====
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHT dht(DHT_PIN, DHT_TYPE);

// ==== NTP ====
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;
const int daylightOffset_sec = 3600;

// ==== WiFi ====
String ssid = "";
String password = "";

struct tm timeinfo;
time_t now;

void displayText(const String& text, bool clear = true) {
  if (clear) display.clearDisplay();
  display.setCursor(0, 0);
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.println(text);
  display.display();
}
void startTone(int frequency) {
  ledcAttachPin(PIEZO_PIN, PIEZO_CHANNEL);
  ledcSetup(PIEZO_CHANNEL, frequency, 8); // 8-bit resolution
  ledcWriteTone(PIEZO_CHANNEL, frequency);
}

void stopTone() {
  ledcWriteTone(PIEZO_CHANNEL, 0);
  ledcDetachPin(PIEZO_PIN);
}

// ==== Task system ====
class Task {
public:
  time_t scheduledTime;
  time_t repeatInterval;
  bool enabled;
  String label;

  Task(time_t scheduled, time_t repeatInt, bool isEnabled, String lbl)
    : scheduledTime(scheduled), repeatInterval(repeatInt), enabled(isEnabled), label(lbl) {}

  virtual void execute() = 0;

  virtual void updateNextExecution(time_t now) {
    if (repeatInterval > 0) {
      while (scheduledTime <= now) {
        scheduledTime += repeatInterval;
      }
    } else {
      enabled = false;
    }
  }
};

class MotorTask : public Task {
private:
  int durationSeconds;
public:
  MotorTask(time_t scheduled, time_t repeatInt, int duration, bool isEnabled, String lbl)
    : Task(scheduled, repeatInt, isEnabled, lbl), durationSeconds(duration) {}

  void execute() override {
    display.clearDisplay();
    display.setCursor(0,0);
    display.println("Running task:");
    display.println(label);
    display.display();

    digitalWrite(PUMP_PIN, HIGH);
    delay(durationSeconds * 1000);
    digitalWrite(PUMP_PIN, LOW);
  }
};

class AlarmTask : public Task {
private:
  int durationSeconds;
public:
  AlarmTask(time_t scheduled, time_t repeatInt, int duration, bool isEnabled, String lbl)
    : Task(scheduled, repeatInt, isEnabled, lbl), durationSeconds(duration) {}

  void execute() override {
    displayText("Alarm: " + label);
    startTone(1000);
    delay(durationSeconds * 1000);
    stopTone();
  }
};

class SensorTask : public Task {
public:
  SensorTask(time_t scheduled, time_t repeatInt, bool isEnabled, String lbl)
    : Task(scheduled, repeatInt, isEnabled, lbl) {}

  void execute() override {
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();

    display.clearDisplay();
    display.setCursor(0, 0);
    display.println("Sensor: " + label);
    if (isnan(temp) || isnan(hum)) {
      display.println("Read error");
    } else {
      display.printf("Temp: %.1f C\n", temp);
      display.printf("Hum : %.1f %%\n", hum);
    }
    display.display();
    delay(5000);
  }
};


std::vector<Task*> tasks;

void sortTasks() {
  std::sort(tasks.begin(), tasks.end(), [](Task* a, Task* b) {
    return a->scheduledTime < b->scheduledTime;
  });
}

void displayUpcomingTasks() {
  display.clearDisplay();
  display.setCursor(0, 0);
  char buf[30];
  strftime(buf, sizeof(buf), " %d/%m/%Y %H:%M:%S", &timeinfo);
  display.println(String(buf));
  display.println();
  display.println("Next tasks:");
  int lines = 1;
  for (Task* t : tasks) {
    if (t->enabled && lines < 4) {
      strftime(buf, sizeof(buf), "%d/%m/%Y %H:%M:%S", localtime(&t->scheduledTime));
      display.print("->");
      display.println(t->label);
      display.print(" ");
      display.println(buf);
      lines++;
    }
  }
  display.display();
}

// ==== Utilidades tarjeta SD ====
bool loadWiFiCredentials() {
  if (!SD.begin(SD_CS)) {
    displayText("SD init failed!");
    return false;
  }

  File file = SD.open("/wifi_config.txt");

  if (!file) {
    // Crear archivo plantilla
    file = SD.open("/wifi_config.txt", FILE_WRITE);
    if (file) {
      file.println("ssid=YOUR_SSID");
      file.println("password=YOUR_PASSWORD");
      file.close();
    }
    displayText("No WiFi config.\nEdit SD file.");
    return false;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.startsWith("ssid=")) {
      ssid = line.substring(5);
    } else if (line.startsWith("password=")) {
      password = line.substring(9);
    }
  }
  file.close();

  if (ssid == "" || password == "") {
    displayText("Invalid config.\nEdit SD file.");
    return false;
  }
  return true;
}

void loadTasksFromSD() {
  if (!SD.begin(SD_CS)) {
    displayText("SD init failed!");
    return;
  }

  File file = SD.open("/tasks.txt");
  if (!file) {
    displayText("tasks.txt not found");
    return;
  }

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0 || line.startsWith("#")) continue;

    // Divide toda la línea por ';'
    std::vector<String> tokens;
    int start = 0;
    while (true) {
      int idx = line.indexOf(';', start);
      if (idx == -1) {
        tokens.push_back(line.substring(start));
        break;
      }
      tokens.push_back(line.substring(start, idx));
      start = idx + 1;
    }

    // Verificar mínimo de campos comunes
    if (tokens.size() < 7) {
      displayText("Malformed line:\n" + line);
      delay(5000);
      continue;
    }

    String label = tokens[0];
    String type = tokens[1];
    bool enabled = (tokens[2] == "true");

    int day, month, year, hour, minute;
    if (sscanf(tokens[3].c_str(), "%d-%d-%d", &day, &month, &year) != 3 ||
        sscanf(tokens[4].c_str(), "%d:%d", &hour, &minute) != 2) {
      displayText("Bad date/time:\n" + tokens[3] + " " + tokens[4]);
      delay(5000);
      continue;
    }

    time_t scheduled = getTimeFromComponents(year, month, day, hour, minute);
    time_t repeat = tokens[5].toInt();

    // Crear tarea según tipo
    if (type == "motor" && tokens.size() >= 8) {
      int duration = tokens[6].toInt(); // parámetro adicional
      tasks.push_back(new MotorTask(scheduled, repeat, duration, enabled, label));
    } else if (type == "alarm" && tokens.size() >= 8) {
      int duration = tokens[6].toInt();
      tasks.push_back(new AlarmTask(scheduled, repeat, duration, enabled, label));
    } else if (type == "sensor") {
      tasks.push_back(new SensorTask(scheduled, repeat, enabled, label));
    } else {
      displayText("Unknown/invalid task:\n" + type);
      delay(5000);
    }
  }

  file.close();
}

void connectToWiFi() {
  WiFi.begin(ssid.c_str(), password.c_str());
  displayText("Connecting to:\n" + ssid);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    display.print(".");
    display.display();
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    displayText("WiFi connected!");
  } else {
    displayText("WiFi failed!");
  }
}

time_t getTimeFromComponents(int year, int month, int day, int hour, int minute) {
  struct tm t = {};
  t.tm_year = year - 1900;
  t.tm_mon = month - 1;
  t.tm_mday = day;
  t.tm_hour = hour;
  t.tm_min = minute;
  t.tm_sec = 0;
  t.tm_isdst = 0;
  time_t result = mktime(&t);
  return result;
}

void setup() {
  pinMode(PIEZO_PIN, OUTPUT);
  digitalWrite(PIEZO_PIN, LOW);
  pinMode(PUMP_PIN, OUTPUT);
  digitalWrite(PUMP_PIN, LOW);

  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
  dht.begin();
  display.clearDisplay();
  display.display();

  if (!loadWiFiCredentials()) return;
  connectToWiFi();
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  if (!getLocalTime(&timeinfo)) {
    displayText("Failed to get time");
    return;
  }
  loadTasksFromSD();
  // ==== Crear la tarea programada ====
  //tasks.push_back(new MotorTask(getTimeFromComponents(2025, 6, 5, 18, 5), 120, 10, true, "Water Pump"));
  //tasks.push_back(new MotorTask(getTimeFromComponents(2025, 6, 5, 18, 7), 180, 5, true, "Turbovieja"));
  sortTasks();
  displayUpcomingTasks();
  delay(10000);
}

void loop() {
  if (getLocalTime(&timeinfo)) {
    now = mktime(&timeinfo);
    if (!tasks.empty()) {
      Task* t = tasks.front();
      if (t->enabled && now >= t->scheduledTime) {
        t->execute();
        t->updateNextExecution(now);
        sortTasks();
      }
    }else{
      displayText("Any task has been scheduled");
    }
    displayUpcomingTasks();
  }
  delay(1000);
}
