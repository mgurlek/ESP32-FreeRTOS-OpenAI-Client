# ESP32 FreeRTOS OpenAI Client

An ESP32-based AI Assistant that uses FreeRTOS and the OpenAI API to answer questions. It features a local web server for submitting questions and an ST7735 display to show status and responses.

## Developers
**Mert Gurlek, Yigit Ozdemir**

## Key Features
- **OpenAI Integration**: Direct communication with OpenAI API (GPT-3.5-turbo).
- **FreeRTOS**: Multitasking with dedicated tasks for GUI, System Monitor, and WiFi/Web Server.
- **Web Interface**: Submit questions via a simple HTTP GET request.
- **Display Output**: ST7735 screen support for visual feedback.
- **Turkish Character Support**: Automatic conversion of UTF-8 Turkish characters for the display.

## Requirements

### Hardware
- **ESP32 Development Board**
- **ST7735 TFT Display** (128x160)
- **Push Button** (GPIO 2)
- Connections (Default):
    - **MOSI**: 11
    - **CLK**: 12
    - **CS**: 10
    - **DC**: 9
    - **RST**: 8
    - **BCKL**: 7

### Software
- **ESP-IDF** (Espressif IoT Development Framework)
- **FreeRTOS** (Included in ESP-IDF)
- **OpenAI API Key**

## Bill of Materials (BOM)

| Component | Quantity | Description | Note |
| :--- | :---: | :--- | :--- |
| **ESP32 N16R8** | 1 | Microcontroller (Wi-Fi + BLE) | ESP32-WROOM-32 or similar |
| **ST7735 TFT Display** | 1 | 1.8" Color Display (128x160) | SPI Interface |
| **Push Button** | 1 | Momentary Switch | For System Monitor toggle |
| **Jumper Wires** | ~10 | Male-to-Male / Male-to-Female | For connections |
| **Breadboard** | 1 | Prototyping Board | Optional, for easy wiring |
| **USB Cable** | 1 | Micro USB | Data capable for programming |

## SDK Configuration & Setup

### 1. Install ESP-IDF
Follow the official [Espressif ESP-IDF Installation Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/) to set up your development environment.

### 2. Clone the Repository
```bash
git clone <repository-url>
cd ESP32-FreeRTOS-OpenAI-Client
```

### 3. Initialize the Project
Set the target to ESP32:
```bash
idf.py set-target esp32
```

### 4. Configure SDK (Important!)
You need to configure the project to use the custom partition table provided in `partitions.csv`.

1. Run the configuration tool:
   ```bash
   idf.py menuconfig
   ```
2. Navigate to **Partition Table**.
3. Select **Partition Table**.
4. Choose **Custom partition table CSV**.
5. Ensure **Custom partition CSV file** is set to `partitions.csv`.
6. Save (Press `S`) and Exit (Press `Esc`).

### 5. Configure the Project
You must configure your WiFi credentials and OpenAI API Key before building.

Open the file `main/main.c` and modify the following macros:

```c
// main/main.c

#define WIFI_SSID           "YOUR-WIFI"             // Enter your WiFi Name
#define WIFI_PASSWORD       "YOUR-WIFI-PASSWORD"    // Enter your WiFi Password
#define OPENAI_API_KEY      "OPEN-AI-API-KEY"       // Enter your OpenAI Sk- Key
```

**Optional Settings:**
- Change `OPENAI_MODEL` to use a different model (default: `gpt-3.5-turbo`).
- Adjust `OFFSET_X` and `OFFSET_Y` if your screen content is shifted.

### 6. Build the Project
Run the following command to compile the project:
```bash
idf.py build
```

### 7. Flash to ESP32
Connect your ESP32 via USB and flash the firmware (replace `PORT` with your serial port, e.g., `/dev/ttyUSB0` or `COM3`):
```bash
idf.py -p PORT flash
```

### 8. Monitor Output
To see logs and debug information:
```bash
idf.py monitor
```

## Usage

1. **Power On**: The device will connect to WiFi. The screen will show "ONLINE" and the IP address.
2. **Ask a Question**:
   - Connect your phone/PC to the same WiFi.
   - Go to the URL shown on the screen: `http://<ESP32-IP>/chat_get?q=Your Question Here`
   - Example: `http://192.168.1.50/chat_get?q=What is the capital of Turkey?`
3. **Response**: 
   - The ESP32 will display "AI DUSUNUYOR" (AI Thinking).
   - Once received, the answer will appear on the ST7735 screen and in the web browser as JSON.
4. **System Monitor**:
   - Press the Button (GPIO 2) to view RAM usage and WiFi Signal strength.
   - Press again to return to the main usage screen.

## License
MIT License
