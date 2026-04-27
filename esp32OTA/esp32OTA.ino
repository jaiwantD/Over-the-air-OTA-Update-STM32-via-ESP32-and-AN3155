#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>


#define STM32_RX_PIN 16 //  (PA9)
#define STM32_TX_PIN 17 // (PA10)
#define BOOT0_PIN 4     //  BOOT0
#define RST_PIN 5       // NRST

const char* ssid = "XXXXXXX-GIT-HIDE---XXXXXXX";
const char* password = "XXXXXX-GIT-HIDE----XXXXXXXX";


WebServer server(80);
WiFiServer serialServer(23);
WiFiClient serialClient;

bool isFlashing = false; 
File uploadFile;

// --- HTML Web Page ---
const char* htmlForm = 
"<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>"
"<style>body{font-family: Arial; text-align: center; margin-top: 50px;}</style></head><body>"
"<h2>STM32 OTA Updater</h2>"
"<form method='POST' action='/upload' enctype='multipart/form-data'>"
"<input type='file' name='update' accept='.bin'><br><br>"
"<input type='submit' value='Upload and Flash STM32'>"
"</form></body></html>";


// ==========================================
// STM32 BOOTLOADER PROTOCOL (AN3155)
// ==========================================

// Added a timeout variable (defaults to 2000ms for normal commands)
bool waitForAck(unsigned long timeout = 2000) {
  unsigned long start = millis();
  while(millis() - start < timeout) { 
    if (Serial2.available()) {
      uint8_t b = Serial2.read();
      if (b == 0x79) return true;  // ACK
      if (b == 0x1F) return false; // NACK
    }
  }
  return false; // Timeout
}

bool stm32Sync() {
  while(Serial2.available()) Serial2.read(); // Clear buffer
  Serial2.write(0x7F); // Sync byte
  return waitForAck();
}

bool stm32Erase() {
  Serial2.write(0x44); // Extended Erase Command
  Serial2.write(0xBB); // Checksum
  if (!waitForAck()) return false;
  
  // Global Erase (0xFFFF) + Checksum (0x00)
  Serial2.write(0xFF);
  Serial2.write(0xFF);
  Serial2.write(0x00);
  
  // WAIT UP TO 15 SECONDS FOR MASS ERASE TO FINISH
  return waitForAck(15000); 
}

bool stm32Write(uint32_t address, uint8_t* data, uint16_t length) {
  Serial2.write(0x31); // Write Memory Command
  Serial2.write(0xCE); // Checksum
  if(!waitForAck()) return false;

  // Send Address + Checksum
  uint8_t a3 = (address >> 24) & 0xFF;
  uint8_t a2 = (address >> 16) & 0xFF;
  uint8_t a1 = (address >> 8) & 0xFF;
  uint8_t a0 = address & 0xFF;
  uint8_t checksum = a3 ^ a2 ^ a1 ^ a0;
  Serial2.write(a3); Serial2.write(a2); Serial2.write(a1); Serial2.write(a0); Serial2.write(checksum);
  if(!waitForAck()) return false;

  // Send Data Length (N-1) + Data + Checksum
  uint8_t n = length - 1;
  checksum = n;
  Serial2.write(n);
  for(int i = 0; i < length; i++) {
    Serial2.write(data[i]);
    checksum ^= data[i];
  }
  Serial2.write(checksum);
  return waitForAck();
}

// ==========================================

void handleFileUpload() {
  HTTPUpload& upload = server.upload();
  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("\nReceiving Firmware: %s\n", upload.filename.c_str());
    uploadFile = LittleFS.open("/firmware.bin", "w");
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) uploadFile.write(upload.buf, upload.currentSize);
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {
      uploadFile.close();
      Serial.printf("File saved! Size: %u bytes\n", upload.totalSize);
    }
  }
}

void setup() {
  Serial.begin(115200); 
  // CRITICAL: STM32 Bootloader requires Even Parity (SERIAL_8E1)
  Serial2.begin(115200, SERIAL_8E1, STM32_RX_PIN, STM32_TX_PIN); 

  pinMode(BOOT0_PIN, OUTPUT);
  pinMode(RST_PIN, OUTPUT);
  digitalWrite(BOOT0_PIN, LOW);
  digitalWrite(RST_PIN, HIGH); 

  if(!LittleFS.begin(true)){
    Serial.println("Error mounting LittleFS");
    return;
  }

  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.print("\nConnected! ESP32 IP Address: ");
  Serial.println(WiFi.localIP());

  serialServer.begin();
  serialServer.setNoDelay(true);

  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html", htmlForm);
  });

  server.on("/upload", HTTP_POST, []() {
    server.send(200, "text/plain", "Upload Complete! Check ESP32 Serial Monitor for flashing status.");
    flashSTM32(); 
  }, handleFileUpload);

  server.begin();
}

void loop() {
  server.handleClient(); 

  if (isFlashing) return; 

  // --- TCP Serial Bridge (Laptop -> STM32 Logs) ---
  if (serialServer.hasClient()) {
    if (!serialClient || !serialClient.connected()) {
      if (serialClient) serialClient.stop();
      serialClient = serialServer.available();
      Serial.println("Laptop connected to view STM32 logs!");
    } else {
      serialServer.available().stop();
    }
  }

  if (Serial2.available() && serialClient && serialClient.connected()) {
    size_t len = Serial2.available();
    uint8_t sbuf[len];
    Serial2.readBytes(sbuf, len);
    serialClient.write(sbuf, len);
  }

  if (serialClient && serialClient.available()) {
    size_t len = serialClient.available();
    uint8_t sbuf[len];
    serialClient.readBytes(sbuf, len);
    Serial2.write(sbuf, len);
  }
}

void flashSTM32() {
  isFlashing = true;
  Serial.println("\n--- Starting STM32 Flash Process ---");

  // 1. Enter Bootloader Mode
  digitalWrite(BOOT0_PIN, HIGH);
  delay(10);
  digitalWrite(RST_PIN, LOW);
  delay(50);
  digitalWrite(RST_PIN, HIGH);
  delay(100); 

  // 2. Sync with STM32
  Serial.print("Syncing with STM32... ");
  if (!stm32Sync()) {
    Serial.println("FAILED! Check wiring (TX/RX).");
    isFlashing = false;
    return;
  }
  Serial.println("OK");

  // 3. Erase Memory
  Serial.print("Erasing flash memory (this takes a moment)... ");
  if (!stm32Erase()) {
    Serial.println("FAILED!");
    isFlashing = false;
    return;
  }
  Serial.println("OK");

  // 4. Open and Write Firmware
  File file = LittleFS.open("/firmware.bin", "r");
  if(!file){
    Serial.println("Error: Could not open firmware.bin");
    isFlashing = false;
    return;
  }

  Serial.print("Writing new firmware...");
  uint32_t address = 0x08000000; // STM32 Main Flash Start Address
  uint8_t buffer[256];
  
  while (file.available()) {
    size_t bytesRead = file.read(buffer, 256);
    
    // STM32 flash writes must be word-aligned (multiples of 4 bytes)
    // If the final chunk is not a multiple of 4, pad it with 0xFF
    while (bytesRead % 4 != 0) {
      buffer[bytesRead] = 0xFF;
      bytesRead++;
    }

    if (!stm32Write(address, buffer, bytesRead)) {
      Serial.println("\nWRITE FAILED at address " + String(address, HEX));
      file.close();
      isFlashing = false;
      return;
    }
    address += bytesRead;
    Serial.print("."); // Print progress dots
  }
  
  file.close();
  Serial.println("\nFlashing finished successfully!");

  // 5. Restart STM32 in Normal Mode
  Serial.println("Rebooting STM32 to run new code...");
  digitalWrite(BOOT0_PIN, LOW);
  delay(10);
  digitalWrite(RST_PIN, LOW);
  delay(50);
  digitalWrite(RST_PIN, HIGH);
  
  isFlashing = false;
  Serial.println("--- Done! ---");
}