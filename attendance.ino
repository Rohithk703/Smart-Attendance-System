#if defined(ESP32)
#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define DEVICE "ESP32"
#define IR_SENSOR_ENTRY_PIN 13 // Entry IR sensor connected to GPIO 13
#define IR_SENSOR_EXIT_PIN 14  // Exit IR sensor connected to GPIO 14
#elif defined(ESP8266)
#include <ESP8266WiFiMulti.h>
ESP8266WiFiMulti wifiMulti;
#define DEVICE "ESP8266"
#define IR_SENSOR_ENTRY_PIN 2  // Entry IR sensor connected to GPIO 2 for ESP8266
#define IR_SENSOR_EXIT_PIN 3   // Exit IR sensor connected to GPIO 3 for ESP8266
#endif

#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <set>  // Include to use the set for storing roll numbers

// WiFi AP SSID
#define WIFI_SSID "Yesudas"
// WiFi password
#define WIFI_PASSWORD "heteam123"

// InfluxDB configuration
#define INFLUXDB_URL "https://us-east-1-1.aws.cloud2.influxdata.com"
#define INFLUXDB_TOKEN "NFdWzEBxBFZzurEU4eBUx2J46YKDD6Wj3RCvY9M0nX4MpdV1fI_Xog6IYUdV0Qtru3pqW8KyejVrqCe0gmQaGA=="
#define INFLUXDB_ORG "Amrita"
#define INFLUXDB_BUCKET "gg"  // Fixed bucket name

// Time zone info
#define TZ_INFO "UTC5.5"

// Declare InfluxDB client instance with preconfigured InfluxCloud certificate
InfluxDBClient client(INFLUXDB_URL, INFLUXDB_ORG, INFLUXDB_BUCKET, INFLUXDB_TOKEN, InfluxDbCloud2CACert);

// Declare Data point
Point sensor("attendance_records");

// Variables for cooldown timing
unsigned long lastEntryDetectionTime = 0;
unsigned long lastExitDetectionTime = 0;
#define DETECTION_COOLDOWN 5000 // 5 seconds between readings

// Set to store roll numbers of those who have entered
std::set<String> enteredRollNumbers;

void setup() {
  Serial.begin(115200);

  // Setup WiFi
  WiFi.mode(WIFI_STA);
  wifiMulti.addAP(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("Connecting to wifi");
  while (wifiMulti.run() != WL_CONNECTED) {
    Serial.print(".");
    delay(100);
  }
  Serial.println();

  // Accurate time is necessary for certificate validation and writing in batches
  timeSync(TZ_INFO, "pool.ntp.org", "time.nis.gov");

  // Check server connection
  if (client.validateConnection()) {
    Serial.print("Connected to InfluxDB: ");
    Serial.println(client.getServerUrl());
  } else {
    Serial.print("InfluxDB connection failed: ");
    Serial.println(client.getLastErrorMessage());
  }

  // Configure IR sensor pins
  pinMode(IR_SENSOR_ENTRY_PIN, INPUT);
  pinMode(IR_SENSOR_EXIT_PIN, INPUT);
}

void loop() {
  // Check WiFi connection and reconnect if needed
  if (wifiMulti.run() != WL_CONNECTED) {
    Serial.println("WiFi connection lost");
    return;
  }

  // Read IR sensor values
  int entryIrValue = digitalRead(IR_SENSOR_ENTRY_PIN);
  int exitIrValue = digitalRead(IR_SENSOR_EXIT_PIN);
  unsigned long currentMillis = millis();

  // Check entry sensor with cooldown
  if (entryIrValue == LOW && (currentMillis - lastEntryDetectionTime) > DETECTION_COOLDOWN) {
    String sectionName = getSectionName();  // Prompt for section name
    String rollNumber = getRollNumber();    // Prompt for roll number

    if (sectionName.length() > 0 && rollNumber.length() > 0) {
      enteredRollNumbers.insert(rollNumber);  // Record the roll number when entry is detected
      recordEvent("entry_detected", sectionName, rollNumber);
      lastEntryDetectionTime = currentMillis;
    }
  }

  // Check exit sensor with cooldown
  if (exitIrValue == LOW && (currentMillis - lastExitDetectionTime) > DETECTION_COOLDOWN) {
    String sectionName = getSectionName();  // Prompt for section name
    String rollNumber = getRollNumber();    // Prompt for roll number

    // Only allow exit if the roll number has been recorded from an entry
    if (enteredRollNumbers.count(rollNumber) > 0 && sectionName.length() > 0 && rollNumber.length() > 0) {
      enteredRollNumbers.erase(rollNumber);  // Remove roll number from the set after exit
      recordEvent("exit_detected", sectionName, rollNumber);
      lastExitDetectionTime = currentMillis;
    } else {
      Serial.println("Exit detected without valid entry.");
    }
  }

  // Check buffer size and write if it's getting full
  if (client.isBufferFull()) {
    Serial.println("Buffer full, forcing write");
    client.flushBuffer();
  }

  delay(100);
}

// Function to prompt user for section name
String getSectionName() {
  Serial.println("Enter section name:");
  while (!Serial.available()) {
    delay(100);
  }

  String sectionName = Serial.readStringUntil('\n');
  sectionName.trim(); // Remove any extra whitespace or newlines
  Serial.print("Using section name: ");
  Serial.println(sectionName);

  return sectionName;
}

// Function to prompt user for roll number
String getRollNumber() {
  Serial.println("Enter roll number:");
  while (!Serial.available()) {
    delay(100);
  }

  String rollNumber = Serial.readStringUntil('\n');
  rollNumber.trim(); // Remove any extra whitespace or newlines
  Serial.print("Using roll number: ");
  Serial.println(rollNumber);

  return rollNumber;
}

// Function to record an entry or exit event
void recordEvent(const String& status, const String& sectionName, const String& rollNumber) {
  sensor.clearFields();
  time_t now;
  time(&now);
  struct tm* localTime = localtime(&now);

  // Add fields for the event
  sensor.addField("status", status);
  sensor.addField("hour", localTime->tm_hour);
  sensor.addField("minute", localTime->tm_min);
  sensor.addField("section", sectionName);    // Include section name
  sensor.addField("roll_number", rollNumber); // Include roll number

  // Write point to InfluxDB
  if (!client.writePoint(sensor)) {
    Serial.print("InfluxDB write failed: ");
    Serial.println(client.getLastErrorMessage());
  } else {
    Serial.println(status + " detected and recorded successfully");
  }
}