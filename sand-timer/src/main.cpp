#include <Arduino.h>
#include <MatrixHardware_ESP32_V0.h>
#include <SmartMatrix.h>
#include <WiFiManager.h>
// AsyncElegantOTA must be after WifiManager to prevent build issues.
#include <AsyncElegantOTA.h>

#include "MPU6050_6Axis_MotionApps_V6_12.h"
#include "RemoteDebug.h"
#include "failsafe.h"
#include "simulation.cpp"

AsyncWebServer server(80);
RemoteDebug Debug;

// Choose the color depth used for storing pixels in the layers: 24 or 48 (24 is
// good for most sketches - If the sketch uses type `rgb24` directly,
// COLOR_DEPTH must be 24)
#define COLOR_DEPTH 24
// Set to the width of your display, must be a multiple of 8
const uint16_t kMatrixWidth = 64;
// Set to the height of your display
const uint16_t kMatrixHeight = 32;
// Tradeoff of color quality vs refresh rate, max brightness, and RAM usage.  36
// is typically good, drop down to 24 if you need to.  On Teensy, multiples of
// 3, up to 48: 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45, 48.  On
// ESP32: 24, 36, 48
const uint8_t kRefreshDepth = 36;
// known working: 2-4, use 2 to save RAM, more to keep from dropping frames and
// automatically lowering refresh rate.  (This isn't used on ESP32, leave as
// default)
const uint8_t kDmaBufferRows = 4;
// Choose the configuration that matches your panels.  See more details in
// MatrixCommonHub75.h and the docs:
// https://github.com/pixelmatix/SmartMatrix/wiki
const uint8_t kPanelType = SM_PANELTYPE_HUB75_32ROW_MOD16SCAN;
// see docs for options: https://github.com/pixelmatix/SmartMatrix/wiki
const uint32_t kMatrixOptions = (SM_HUB75_OPTIONS_NONE);
const uint8_t kBackgroundLayerOptions = (SM_BACKGROUND_OPTIONS_NONE);

SMARTMATRIX_ALLOCATE_BUFFERS(matrix, kMatrixWidth, kMatrixHeight, kRefreshDepth,
                             kDmaBufferRows, kPanelType, kMatrixOptions);

SMARTMATRIX_ALLOCATE_BACKGROUND_LAYER(backgroundLayer, kMatrixWidth,
                                      kMatrixHeight, COLOR_DEPTH,
                                      kBackgroundLayerOptions);

const int defaultBrightness = (100 * 255) / 100;  // full (100%) brightness

MPU6050 mpu;
Quaternion q;
uint8_t fifoBuffer[64];
VectorFloat gravity;
bool dmpReady = false;

Simulation simulation;

void setup() {
  FailSafe.checkBoot();

  Serial.begin(115200);

  WiFi.hostname("james_sand_timer");
  WiFiManager wifiManager;
  wifiManager.autoConnect();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(200, "text/plain", "Hi! I am ESP32.");
  });

  // Bypass ElegantOTA so that it doesn't crash with the display timer.
  //   server.on("/update/identity", HTTP_GET, [&](AsyncWebServerRequest
  //   *request) {
  //     request->send(200, "application/json",
  //                   "{\"id\": \"clock\", \"hardware\": \"ESP32\"}");
  //   });
  //   server.on("/update", HTTP_GET, [&](AsyncWebServerRequest *request) {
  //     // Wifi is really unresponsive if the display is still spitting out
  //     signals. dmaOutput.stopDMAoutput();

  //     AsyncWebServerResponse *response = request->beginResponse_P(
  //         200, "text/html", ELEGANT_HTML, ELEGANT_HTML_SIZE);
  //     response->addHeader("Content-Encoding", "gzip");
  //     request->send(response);
  //   });

  AsyncElegantOTA.begin(&server);
  server.begin();

  Debug.begin("Test");

  matrix.addLayer(&backgroundLayer);
  //   matrix.addLayer(&scrollingLayer);
  //   matrix.addLayer(&indexedLayer);
  matrix.begin();

  matrix.setBrightness(defaultBrightness);

  backgroundLayer.enableColorCorrection(true);

  Wire.begin(23, 14);
  int8_t devStatus = mpu.dmpInitialize();

  // supply your own gyro offsets here, scaled for min sensitivity
  mpu.setXAccelOffset(-2279);
  mpu.setYAccelOffset(761);
  mpu.setZAccelOffset(1609);
  mpu.setXGyroOffset(55);
  mpu.setYGyroOffset(-51);
  mpu.setZGyroOffset(15);
  // make sure it worked (returns 0 if so)
  if (devStatus == 0) {
    mpu.CalibrateAccel(6);
    mpu.CalibrateGyro(6);

    mpu.setDMPEnabled(true);
    dmpReady = true;
  }

  Serial.println("Version 5");

  delay(3000);

  simulation.init(kMatrixWidth, kMatrixHeight);
}

void loop() {
  if (dmpReady) {
    mpu.dmpGetCurrentFIFOPacket(fifoBuffer);
    mpu.dmpGetQuaternion(&q, fifoBuffer);
    mpu.dmpGetGravity(&gravity, &q);
  }

  simulation.updateAndDraw(&backgroundLayer, gravity);
  Debug.handle();
  FailSafe.loop();

  delay(1);
}