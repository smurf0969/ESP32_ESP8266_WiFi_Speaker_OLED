#include <FS.h> //this needs to be first, or it all crashes and burns...
#include "WiFiConnectOLED.h" //include before SSD1306.h if using custom fonts

#include "play.h"
#include "error.h"
#include "logo.h"

#ifdef ESP32
#include "Arduino.h"
#include "pins_arduino.h" // Heltec_WIFI_Kit_32 pins

#include <SPIFFS.h> // Used to save custom parameters
#endif

#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson

//******* OLED *********

#include <Wire.h>  // Only needed for Arduino 1.6.5 and earlier
/*
   Origional library https://github.com/ThingPulse/esp8266-oled-ssd1306
   Customised library https://github.com/smurf0969/esp8266-oled-ssd1306/tree/Allow-overriding-default-font
*/
#include "SSD1306.h" // alias for `#include "SSD1306Wire.h"


// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// connect ESP12E  D1 to SCL, D2 to SDA
//OLED pins to ESP32 GPIOs via this connecthin:
//OLED_SDA -- GPIO4
//OLED_SCL -- GPIO15
//OLED_RST -- GPIO16

#ifdef ESP32
#define OLED_RESET 16
#else
#define OLED_RESET 10
#endif

#ifdef ESP32
SSD1306  display(0x3c, 4, 15);
#define oledPWR 16 //Pin to supply power to OLED
#else
SSD1306  display(0x3c, 4, 5);
#define oledPWR 10 //Pin to supply power to OLED
#endif

/* Audio */
#include "AudioFileSourceHTTPStream.h"
#include "AudioFileSourceBuffer.h"
#include "AudioGeneratorMP3.h"
#include "AudioOutputI2S.h"

#if !defined(INTERNAL_DAC)
#define INTERNAL_DAC 1
#endif
// To run, set your ESP8266 build to 160MHz,  and upload.
AudioGeneratorMP3 *mp3;
AudioFileSourceHTTPStream  *file;
AudioFileSourceBuffer *buff;
AudioOutputI2S *out;

int lastCode = 0;
int buffUnderflow = 0;
// Called when there's a warning or error (like a buffer underflow or decode hiccup)
void StatusCallback(void *cbData, int code, const char *string)
{
  if (code == 3 && lastCode != 2) {
    buffUnderflow += 1;
  }
  lastCode = code;
  const char *ptr = reinterpret_cast<const char *>(cbData);
  // Note that the string may be in PROGMEM, so copy it to RAM for printf
  char s1[64];
  strncpy_P(s1, string, sizeof(s1));
  s1[sizeof(s1) - 1] = 0;
  Serial.printf("STATUS(%s) '%d' = '%s'\n", ptr, code, s1);
  Serial.flush();
}


/* Our custom Parameters */
char URL[261] = "http://192.168.1.187:8000/live"; //TODO
char server_ip[128] = "192.168.1.187";
char server_port[5] = "8000";
char server_path[128] = "/live";
void setMp3URL() {
  String s = "http://" + String(server_ip) + (strlen(server_port) > 0 ? ":" + String(server_port) : "") + (strlen(server_path) > 0 ? String(server_path) : "");
  strcpy(URL, s.c_str());
  Serial.println(URL);
}
int json_doc_size = 256;
WiFiConnectParam custom_server_ip("server_ip", "server_ip", server_ip, 128);
WiFiConnectParam custom_server_port("server_port", "server_port", server_port, 5);
WiFiConnectParam custom_server_path("server_path", "server_path", server_path, 128);

WiFiConnectOLED wc(&display, oledPWR); //Initialise our connector with an OLED display

/*
    Callback for when parameters need saving.
    Can be used to set a flag preferably with saving taking place in main loop.

*/
bool configNeedsSaving = false;
void saveConfigCallback() {
  Serial.println("Should save config");
  configNeedsSaving = true;
}
void configModeCallback(WiFiConnect *mWiFiConnect) {
  Serial.println("Entering Access Point");
}
/* Save our custom parameters */
void saveConfiguration() {
  configNeedsSaving = false;
  if (!SPIFFS.begin()) {
    Serial.println("UNABLE to open SPIFFS");
    return;
  }

  strcpy(server_ip, custom_server_ip.getValue());
  strcpy(server_port, custom_server_port.getValue());
  strcpy(server_path, custom_server_path.getValue());

  Serial.println("saving config");
  DynamicJsonDocument doc(json_doc_size);


  doc["server_ip"] = server_ip;
  doc["server_port"] = server_port;
  doc["server_path"] = server_path;

  fs::File configFile = SPIFFS.open("/config.json", "w");

  if (!configFile) {
    Serial.println("failed to open config file for writing");
  }
  serializeJson(doc, Serial);
  Serial.println("");
  serializeJson(doc, configFile);
  configFile.close();
  //end save
  SPIFFS.end();
  setMp3URL();
  Serial.println(URL);
}


void startWiFi(boolean showParams = false) {
  wc.begin(true);
  //wc.setDebug(true);
  /* Set our callbacks */
  wc.setSaveConfigCallback(saveConfigCallback);
  wc.setAPCallback(configModeCallback);
  /* Add some custom parameters */
  wc.addParameter(&custom_server_ip);
  wc.addParameter(&custom_server_port);
  wc.addParameter(&custom_server_path);

  //wc.screenTest(); //test screen by cycling through the presete screens
  //wc.resetSettings(); //helper to remove the stored wifi connection, comment out after first upload and re upload

  if (!showParams) {
    /*
       AP_NONE = Continue executing code
       AP_LOOP = Trap in a continuous loop
       AP_RESET = Restart the chip
    */
    if (!wc.autoConnect()) { // try to connect to wifi
      /* We could also use button etc. to trigger the portal on demand within main loop */
      wc.startConfigurationPortal(AP_LOOP);//if not connected show the configuration portal
    }
  } else {
    /* We could also use button etc. to trigger the portal on demand within main loop */
    wc.startParamsPortal(AP_NONE); //Show the parameters portal
  }
  // when displayLoop is called from main loop, will turn of display after time period
  wc.displayTurnOFF((60 * 1000 * 10));
}

void loadConfiguration() {
  if (!SPIFFS.begin()) {
    Serial.println(F("Unable to start SPIFFS"));
    while (1) {
      delay(1000);
    }
  } else {
    Serial.println(F("mounted file system"));

    if (SPIFFS.exists("/config.json")) {
      //file exists, reading and loading
      Serial.println("reading config file");

      fs::File configFile = SPIFFS.open("/config.json", "r");
      if (configFile) {
        Serial.println(F("opened config file"));
        size_t sizec = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[sizec]);

        configFile.readBytes(buf.get(), sizec);
        DynamicJsonDocument doc(1024);
        DeserializationError error = deserializeJson(doc, buf.get());
        if (error) {
          Serial.println("failed to load json config");
        } else {
          strcpy(server_ip, doc["server_ip"]);
          strcpy(server_port, doc["server_port"]);
          strcpy(server_path, doc["server_path"]);
          custom_server_ip.setValue(server_ip);
          custom_server_port.setValue(server_port);
          custom_server_path.setValue(server_path);
        }
        configFile.close();

      }
    } else {
      Serial.println(F("Config file not found"));
    }
    SPIFFS.end();
  }
  setMp3URL();
}

const int preallocateBufferSize = 5 * 1024;
//const int preallocateBufferSize = bufferMultiply * 1024;
const int preallocateCodecSize = 29192; // MP3 codec max mem needed
void *preallocateBuffer = NULL;
void *preallocateCodec = NULL;

bool useBuff = false;
bool buffReserved = false;
bool codecReserved = false;

void startAudio(bool del = false) {
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawStringMaxWidth(128 / 2, (64 / 2) - 20, 128 - 10, F("Trying to start audio"));
  display.display();
  delay(500);
  if (del) {
    delete mp3;
    //delete out;
    delete buff;
    delete file;
    delay(100);
    if (useBuff && buffUnderflow >= 5) {
      useBuff = false;
      free(preallocateBuffer);
      buffReserved = false;
    }
  }
  if (useBuff && !buffReserved) {
    buffReserved = true;
    preallocateBuffer = malloc(preallocateBufferSize);
  }
  if (!codecReserved) {
    codecReserved = true;
    preallocateCodec = malloc(preallocateCodecSize);
  }
  if ((useBuff && !preallocateBuffer) || !preallocateCodec) {
    Serial.printf_P(PSTR("FATAL ERROR:  Unable to preallocate %d bytes for app\n"), preallocateBufferSize + preallocateCodecSize);
    Serial.printf("FreeHeap: %d\n", ESP.getFreeHeap());
    Serial.printf("Buffer: %s, %d\n", (!preallocateBuffer ? "false" : "true"), (!preallocateBuffer ? 0 : preallocateCodecSize));
    Serial.printf("Codec: %s, %d\n", (!preallocateCodec ? "false" : "true"), (!preallocateCodec ? 0 : preallocateCodecSize));

    while (1) delay(1000); // Infinite halt
  } else {
    Serial.println("");
    if (useBuff) {
      Serial.printf_P(PSTR("Preallocate %d bytes for Buffer\n"), preallocateBufferSize);
    }
    Serial.printf_P(PSTR("Preallocate %d bytes for Codec\n"), preallocateCodecSize);
  }

  Serial.println();
  Serial.print(F("Buffer Underflow Count: "));
  Serial.println(buffUnderflow);
  file = new AudioFileSourceHTTPStream(URL);
  if (useBuff) {
    buff = new AudioFileSourceBuffer(file, 2048);
    buff->RegisterStatusCB(StatusCallback, (void*)"buffer");
  }
#ifdef ESP32
  out = new AudioOutputI2S(0, INTERNAL_DAC);
#else
  out = new AudioOutputI2S();
  out->SetGain(0.2); // 0 - 4, 0.3 lower values reduce crackle
  out->SetRate(44100);
  out->SetBitsPerSample(16);
  out->SetChannels(2);
#endif
  mp3 =  new AudioGeneratorMP3(preallocateCodec, preallocateCodecSize);;
  //mp3->RegisterStatusCB(StatusCallback, (void*)"mp3");
  if (useBuff) {
    Serial.println(F("Using Audio Buffer"));
    mp3->begin(buff, out);
  } else {
    Serial.println(F("No Audio Buffer"));
    mp3->begin(file, out);
  }
  Serial.printf("FreeHeap: %d\n", ESP.getFreeHeap()); //Too low on free heap and we will never work!!
}

void setup() {
#ifdef ESP8266
  system_update_cpu_freq(SYS_CPU_160MHZ);
#endif
delay(100);
  Serial.begin(115200);
  while (!Serial) {
    delay(100);
  }

  loadConfiguration();

  startWiFi();
  delay(1500);
  display.clear();
  display.drawXbm(0, 0, logoDataW, logoDataH, logoData);
  display.display();
  delay(1500);
  startAudio();
}

void loop() {
  static int lastms = 0;
  if (configNeedsSaving) {
    saveConfiguration();
  }

  //  if (WiFi.status() != WL_CONNECTED) {
  //    if (!wc.autoConnect()) wc.startConfigurationPortal(AP_RESET);
  //  }
  if (mp3->isRunning()) {
    if (lastms == 0) {
      display.clear();
      display.drawXbm(0, 0, playDataW, playDataH, playData);
      display.display();
    }
    if (millis() - lastms > 1000) {
      lastms = millis();
      //Serial.printf("Running for %d ms...\n", lastms);
      //Serial.flush();
    }
    if (!mp3->loop()) {
      mp3->stop();
      lastms = 0;
    }
  } else {

    display.clear();
    display.drawXbm(0, 0, errorDataW, errorDataH, errorData);
    display.display();
    lastms = 0;
    Serial.printf("MP3 done\n");
    delay(3000);
    if (!wc.autoConnect()) {
      wc.startConfigurationPortal(AP_RESET);
    }
    startAudio(true);
  }
}
