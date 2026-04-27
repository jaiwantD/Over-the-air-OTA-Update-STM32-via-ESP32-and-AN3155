<div align="center">

# 🚀 ESP32 → STM32 OTA Updater

[![Platform](https://img.shields.io/badge/Platform-ESP32-blue?style=for-the-badge&logo=espressif&logoColor=white)](https://www.espressif.com/)
[![Target](https://img.shields.io/badge/Target-STM32-blue?style=for-the-badge&logo=stmicroelectronics&logoColor=white)](https://www.st.com/)
[![Protocol](https://img.shields.io/badge/Protocol-UART%20AN3155-green?style=for-the-badge&logo=data:image/svg+xml;base64,PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHZpZXdCb3g9IjAgMCAyNCAyNCI+PHBhdGggZmlsbD0id2hpdGUiIGQ9Ik0xMiAyQzYuNDggMiAyIDYuNDggMiAxMnM0LjQ4IDEwIDEwIDEwIDEwLTQuNDggMTAtMTBTMTcuNTIgMiAxMiAyem0tMSAxNEg5VjhsMiAyVjE2em00IDBIMTFWOSAwaDFsMyA0LTMgM3oiLz48L3N2Zz4=)](https://www.st.com/resource/en/application_note/an3155-usart-protocol-used-in-the-stm32-bootloader-stmicroelectronics.pdf)
[![Storage](https://img.shields.io/badge/Storage-LittleFS-orange?style=for-the-badge&logo=files&logoColor=white)](https://github.com/lorol/LittleFS_esp32)
[![License](https://img.shields.io/badge/License-MIT-yellow?style=for-the-badge)](LICENSE)

<br/>

> **Wireless firmware flashing for STM32 microcontrollers — no USB cable, no programmer, no problem.**  
> Upload a `.bin` file from any browser and the ESP32 handles everything else.

<br/>

<img src="https://img.shields.io/badge/Status-Working-brightgreen?style=flat-square" />
<img src="https://img.shields.io/badge/UART-8E1%20Even%20Parity-blue?style=flat-square" />
<img src="https://img.shields.io/badge/WiFi-OTA%20via%20Browser-blueviolet?style=flat-square" />
<img src="https://img.shields.io/badge/TCP%20Bridge-Port%2023-lightgrey?style=flat-square" />

</div>

---

## 📖 Table of Contents

- [Overview](#-overview)
- [Features](#-features)
- [Architecture](#-architecture)
- [Hardware](#-hardware-setup)
- [Wiring Diagram](#-wiring-diagram)
- [Software Setup](#-software-setup)
- [How It Works](#-how-it-works)
- [Web Interface](#-web-interface)
- [TCP Serial Bridge](#-tcp-serial-bridge)
- [Bootloader Protocol](#-stm32-bootloader-protocol)
- [Configuration](#-configuration)
- [Troubleshooting](#-troubleshooting)
- [Contributing](#-contributing)

---

## 🌟 Overview

This project turns an **ESP32** into a wireless STM32 programmer. Instead of connecting a ST-Link or USB-UART adapter every time you want to flash your STM32, you simply:

1. Visit the ESP32's IP in a browser
2. Pick your `.bin` firmware file
3. Click **Upload and Flash**

The ESP32 saves the firmware to its internal filesystem (LittleFS), then bit-bangs the STM32 into bootloader mode and flashes it over UART using ST's official **AN3155 bootloader protocol** — completely hands-free.

---

## ✨ Features

| Feature | Description |
|---|---|
| 🌐 **Browser-based UI** | Upload firmware from any device on the same WiFi network |
| 💾 **LittleFS Storage** | Firmware stored safely on ESP32 flash before flashing |
| 🔄 **Hardware Reset Control** | ESP32 controls BOOT0 and NRST pins directly |
| 🔐 **AN3155 Compliant** | Full implementation of ST's UART bootloader spec |
| 🗑️ **Mass Erase** | Extended erase with 15-second timeout for large flash |
| 📡 **TCP Serial Bridge** | View STM32 UART logs wirelessly over Telnet (port 23) |
| ✅ **Word-aligned Writes** | Auto-pads final chunk to meet STM32 flash requirements |
| 🔁 **Auto-reboot** | STM32 is automatically restarted into normal mode after flashing |

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                        Your Laptop / Phone                      │
│                                                                 │
│    Browser (Upload .bin)          Telnet Client (Port 23)       │
└───────────────┬───────────────────────────┬─────────────────────┘
                │  WiFi                     │  WiFi
                ▼                           ▼
┌───────────────────────────────────────────────────────────────  ┐
│                          ESP32                                  │
│                                                                 │
│  ┌──────────────┐   ┌─────────────┐   ┌──────────────────────┐ │
│  │  Web Server  │   │   LittleFS  │   │  TCP Serial Bridge   │ │
│  │  (Port 80)   │──▶│firmware.bin │   │    (Port 23)         │ │
│  └──────┬───────┘   └──────┬──────┘   └──────────┬───────────┘ │
│         │                  │                       │            │
│         └──────────────────┴───────────────────────┘           │
│                             │                                   │
│                    ┌────────▼────────┐                          │
│                    │  Flash Engine   │                          │
│                    │  (AN3155 UART)  │                          │
│                    └────────┬────────┘                          │
└─────────────────────────────┼───────────────────────────────────┘
                              │  UART (8E1, 115200)
                              │  BOOT0 / NRST GPIO
                              ▼
              ┌───────────────────────────────┐
              │            STM32              │
              │   (Bootloader / Application)  │
              └───────────────────────────────┘
```

---

## 🔧 Hardware Setup

### Parts Required

| Component | Notes |
|---|---|
| ![ESP32](https://img.shields.io/badge/-ESP32%20Dev%20Board-red?style=flat-square) | Any standard ESP32 module |
| ![STM32](https://img.shields.io/badge/-STM32%20Target-blue?style=flat-square) | Any STM32 with UART bootloader support |
| Jumper Wires | 6 wires needed |
| 3.3V Logic Level | Both devices must be at 3.3V |

> ⚠️ **Voltage Warning:** STM32 and ESP32 both operate at 3.3V. Do **not** use 5V logic — it will damage your STM32.

---

## 🔌 Wiring Diagram

```
  ESP32 Pin          STM32 Pin
  ─────────          ─────────
  GPIO 16 (RX2) ───► PA9  (TX) 
  GPIO 17 (TX2) ◄─── PA10 (RX) 
  GPIO 4        ───► BOOT0
  GPIO 5        ───► NRST
  GND           ───► GND
  3.3V          ───► VCC  (if powering from ESP32)
```

| ESP32 Pin | Function | STM32 Pin |
|---|---|---|
| GPIO 16 (RX2) | Receive STM32 data | PA9 (USART TX) |
| GPIO 17 (TX2) | Send data to STM32 | PA10 (USART RX) |
| GPIO 4 | BOOT0 control | BOOT0 |
| GPIO 5 | Reset control | NRST |
| GND | Common ground | GND |

> 💡 **Tip:** On many STM32 boards, a 10kΩ pull-down resistor on BOOT0 ensures the device boots normally when BOOT0 is not actively driven HIGH.

---

## 💻 Software Setup

### Prerequisites

- [Arduino IDE](https://www.arduino.cc/en/software) or [PlatformIO](https://platformio.org/)
- **ESP32 Board Package** installed
- **LittleFS** library for ESP32

### Installation

**1. Clone this repository:**
```bash
git clone https://github.com/jaiwantD/Over-the-air-OTA-Update-STM32-via-ESP32-and-AN3155
cd esp32-stm32-ota-updater
```

**2. Configure your WiFi credentials** in the sketch:
```cpp
const char* ssid     = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
```

**3. Select your board and port in Arduino IDE:**
- Board: `ESP32 Dev Module`
- Partition Scheme: `Default 4MB with spiffs` (or any with LittleFS)

**4. Upload the sketch** to your ESP32.

**5. Open Serial Monitor** at 115200 baud and note the IP address printed:
```
Connected! ESP32 IP Address: 192.168.1.42
```

---

## ⚙️ How It Works

The flashing process follows 5 clean steps:

```
Step 1 ──► Assert BOOT0 HIGH + Pulse NRST  →  STM32 enters Bootloader Mode
Step 2 ──► Send 0x7F sync byte             →  Receive ACK (0x79)
Step 3 ──► Extended Erase (0xFFFF)         →  Mass erase (up to 15s timeout)
Step 4 ──► Write 256-byte chunks           →  Starting at 0x08000000
Step 5 ──► Assert BOOT0 LOW + Pulse NRST  →  STM32 boots into new firmware
```

### Flash Write Details

- Firmware is written in **256-byte pages** starting at `0x08000000`
- The final chunk is **padded with `0xFF`** to ensure 4-byte word alignment
- Each write command is **ACK/NACK verified** — flashing stops on the first error
- Progress is streamed to the Serial monitor as dots: `Writing new firmware........`

---

## 🌐 Web Interface

Once connected to WiFi, navigate to:

```
http://<ESP32_IP_ADDRESS>/
```

You'll see a minimal, clean upload form:

```
┌──────────────────────────────────────┐
│         STM32 OTA Updater            │
│                                      │
│  [Choose .bin file]                  │
│                                      │
│  [ Upload and Flash STM32 ]          │
└──────────────────────────────────────┘
```

After clicking upload:
- The firmware is saved to LittleFS as `firmware.bin`
- Flashing begins automatically
- Status messages stream to the **ESP32 Serial Monitor**

---

## 📡 TCP Serial Bridge

While not flashing, the ESP32 acts as a **wireless serial bridge** to the STM32.

Connect any Telnet client to port **23** to view live STM32 UART output:

```bash
# Linux / macOS
telnet 192.168.1.42 23

# Windows (PuTTY)
# Host: 192.168.1.42 | Port: 23 | Connection Type: Raw
```

> ℹ️ The TCP bridge is automatically suspended during flashing (`isFlashing` flag) and resumes when done.

---

## 📚 STM32 Bootloader Protocol

This project implements **ST Application Note AN3155** — the USART protocol used by the STM32 built-in ROM bootloader.

| Command | Byte | Checksum | Description |
|---|---|---|---|
| Sync | `0x7F` | — | Synchronise baud rate |
| Extended Erase | `0x44` | `0xBB` | Mass erase flash |
| Write Memory | `0x31` | `0xCE` | Write up to 256 bytes |

### Critical UART Setting

```cpp
// ⚠️ MUST use 8-bit Even Parity — STM32 bootloader requires it
Serial2.begin(115200, SERIAL_8E1, RX_PIN, TX_PIN);
//                    ^^^^^^^^^ Even parity, 1 stop bit
```

> Using `SERIAL_8N1` will cause sync failures every time. This is the #1 gotcha.

---

## 🛠️ Configuration

All configurable parameters are at the top of the sketch:

```cpp
// Pin Assignments
#define STM32_RX_PIN  16   // ESP32 RX2 → STM32 TX (PA9)
#define STM32_TX_PIN  17   // ESP32 TX2 → STM32 RX (PA10)
#define BOOT0_PIN      4   // Controls STM32 boot mode
#define RST_PIN        5   // Controls STM32 reset

// Network
const char* ssid     = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";

// Timeouts (in waitForAck)
// Normal commands: 2000ms
// Mass erase:     15000ms  ← may need increasing for large flash
```

---

## 🐛 Troubleshooting

| Symptom | Likely Cause | Fix |
|---|---|---|
| `FAILED! Check wiring (TX/RX).` | Sync byte not ACK'd | Verify TX/RX are not swapped; check BOOT0 is HIGH |
| `Erasing flash memory... FAILED!` | Erase timeout too short | Increase `waitForAck` timeout beyond 15s for large MCUs |
| `WRITE FAILED at address 0x...` | Parity mismatch or bad data | Ensure `SERIAL_8E1` is set; verify `.bin` is not corrupted |
| STM32 doesn't start after flash | BOOT0 stuck HIGH | Check GPIO 4 is driven LOW after flashing |
| Page not loading | Wrong IP or not on same network | Check Serial Monitor for correct IP address |
| Telnet shows garbage | Baud rate mismatch | Match STM32 app UART baud to 115200 |

---

## 📁 File Structure

```
esp32-stm32-ota-updater/
│
├── esp32_stm32_ota.ino     ← Main Arduino sketch
├── README.md               ← This file
└── LICENSE                 ← MIT License
```

---

## 🤝 Contributing

Contributions are welcome! Ideas for improvement:

- [ ] Add OTA progress bar in the browser UI
- [ ] Support for Go command (launch application directly)
- [ ] mDNS support (`stm32-flasher.local`)
- [ ] Flash verification (Read Memory + CRC check)
- [ ] Support multiple STM32 targets via dropdown

**To contribute:** Fork → Branch → PR. Please test with real hardware before submitting.

---

## 📄 License

This project is licensed under the **MIT License** — see the [LICENSE](LICENSE) file for details.

---

<div align="center">

Built with ❤️ using **ESP32** + **STM32** + **AN3155**

[![espressif](https://img.shields.io/badge/Espressif-ESP32-E7352C?style=for-the-badge&logo=espressif&logoColor=white)](https://www.espressif.com/)
[![ST](https://img.shields.io/badge/STMicroelectronics-STM32-03234B?style=for-the-badge&logo=stmicroelectronics&logoColor=white)](https://www.st.com/)
[![Arduino](https://img.shields.io/badge/Arduino-Framework-00878F?style=for-the-badge&logo=arduino&logoColor=white)](https://www.arduino.cc/)

*If this saved you from plugging in a USB cable, consider leaving a ⭐*

</div>
