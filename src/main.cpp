#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7789.h>

#include "animation.h"
#include "display_config.h"

namespace {
using namespace DisplayConfig;

constexpr int16_t kCanvasWidth = (kRotation & 1) ? kHeight : kWidth;
constexpr int16_t kCanvasHeight = (kRotation & 1) ? kWidth : kHeight;
static_assert(kRotation <= 3 && kBoxCount > 0 && kBoxCount <= 32,
              "Use rotation 0..3 and 1..32 boxes");
static_assert(kMinBoxSize >= 3 && kMaxBoxSize >= kMinBoxSize &&
                  kMaxBoxSize < kCanvasWidth &&
                  kMaxBoxSize < kCanvasHeight - kHudHeight,
              "Boxes must fit the animation area");
static_assert(kHudHeight >= 20 && kCanvasWidth >= 200 && kSpeedScale > 0 &&
                  kFrameIntervalUs > 0 && kFpsUpdateIntervalUs > 0 &&
                  kMaxPhysicsStepUs > 0,
              "Invalid display or timing configuration");

Adafruit_ST7789 display(&SPI, kCs, kDc, kReset);
// 整帧合成，SPI 只传输发生变化的矩形。
GFXcanvas16 frame(kCanvasWidth, kCanvasHeight);
Animation::Box boxes[kBoxCount];
Animation::Rect dirtyRegions[kBoxCount];

constexpr uint16_t kColors[] = {
    ST77XX_CYAN, ST77XX_YELLOW, ST77XX_GREEN, ST77XX_MAGENTA,
    ST77XX_RED, ST77XX_BLUE, 0xFD20, 0x867F};
constexpr size_t kColorCount = sizeof(kColors) / sizeof(kColors[0]);
uint32_t lastFrameUs = 0;
uint32_t fpsWindowStartUs = 0;
uint32_t completedFrames = 0;
char fpsLabel[20] = "FPS: --.-";
bool hudDirty = false;

bool initializeBoxes() {
  for (size_t i = 0; i < kBoxCount; ++i) {
    auto &box = boxes[i];
    box.size = kMinBoxSize + (kMaxBoxSize - kMinBoxSize) * i /
                                (kBoxCount > 1 ? kBoxCount - 1 : 1);
    box.velocityX = (45.0f + (i * 17) % 101) * kSpeedScale *
                    ((i & 1) ? -1.0f : 1.0f);
    box.velocityY = (35.0f + (i * 23) % 91) * kSpeedScale *
                    ((i & 2) ? -1.0f : 1.0f);
    box.color = kColors[i % kColorCount];
  }
  return Animation::arrange(boxes, kBoxCount, kCanvasWidth, kCanvasHeight, kHudHeight);
}

void composeFrame() {
  frame.fillScreen(ST77XX_BLACK);
  for (const auto &box : boxes) {
    const auto rect = box.bounds();
    frame.fillRect(rect.x, rect.y, rect.width, rect.height, box.color);
    frame.drawRect(rect.x, rect.y, rect.width, rect.height, ST77XX_WHITE);
  }

  frame.fillRect(0, 0, kCanvasWidth, kHudHeight, 0x18E3);
  frame.drawFastHLine(0, kHudHeight - 1, kCanvasWidth, 0x7BEF);
  frame.setTextSize(1);
  frame.setTextWrap(false);
  frame.setTextColor(ST77XX_YELLOW);
  frame.setCursor(6, 8);
  frame.print(fpsLabel);
  frame.setTextColor(ST77XX_WHITE);
  frame.setCursor(kCanvasWidth - 66, 8);
  frame.print("BOXES: ");
  frame.print(kBoxCount);
}

void uploadRegion(const Animation::Rect &rect) {
  display.startWrite();
  display.setAddrWindow(rect.x, rect.y, rect.width, rect.height);
  for (int16_t row = 0; row < rect.height; ++row) {
    uint16_t *pixels = frame.getBuffer() + (rect.y + row) * kCanvasWidth + rect.x;
    display.writePixels(pixels, rect.width);
  }
  display.endWrite();
}

void recordCompletedFrame() {
  ++completedFrames;
  const uint32_t now = micros();
  const uint32_t elapsed = now - fpsWindowStartUs;
  if (elapsed >= kFpsUpdateIntervalUs) {
    // 在传输完成后计数，统计包含绘制、SPI 传输和等待的真实帧率。
    const float fps = completedFrames * 1000000.0f / elapsed;
    snprintf(fpsLabel, sizeof(fpsLabel), "FPS: %4.1f", fps);
    completedFrames = 0;
    fpsWindowStartUs = now;
    hudDirty = true;  // 下一帧将新读数绘制到屏幕。
  }
}
}  // namespace

void setup() {
  pinMode(kBacklight, OUTPUT);
  digitalWrite(kBacklight, kBacklightOn == HIGH ? LOW : HIGH);
  Serial.begin(115200);
  Serial.println("\nESP32-S3 N16R8 / ST7789 multi-box animation");
  Serial.printf("Flash: %u MB, PSRAM: %u MB\n",
                static_cast<unsigned>(ESP.getFlashChipSize() / (1024 * 1024)),
                static_cast<unsigned>(ESP.getPsramSize() / (1024 * 1024)));

  if (frame.getBuffer() == nullptr) {
    while (true) {
      Serial.println("ERROR: display frame buffer allocation failed");
      delay(1000);
    }
  }

  SPI.begin(kSck, -1, kMosi, kCs);
  display.init(kWidth, kHeight, SPI_MODE0);
  display.setSPISpeed(kSpiFrequency);
  display.setRotation(kRotation);
  display.invertDisplay(kInvertColors);

  if (!initializeBoxes()) {
    display.fillScreen(ST77XX_BLACK);
    display.setCursor(8, 40);
    display.setTextColor(ST77XX_RED);
    display.print("Too many / too large boxes");
    digitalWrite(kBacklight, kBacklightOn);
    while (true) {
      Serial.println("ERROR: boxes do not fit; reduce box count or maximum size");
      delay(1000);
    }
  }
  composeFrame();
  uploadRegion({0, 0, kCanvasWidth, kCanvasHeight});
  digitalWrite(kBacklight, kBacklightOn);
  lastFrameUs = micros();
  fpsWindowStartUs = lastFrameUs;
  Serial.printf("Display: %d x %d; boxes: %u; animation started\n",
                display.width(), display.height(), static_cast<unsigned>(kBoxCount));
}

void loop() {
  const uint32_t now = micros();
  const uint32_t elapsed = now - lastFrameUs;  // 无符号相减支持 micros() 回绕。
  if (elapsed < kFrameIntervalUs) {
    delay(1);
    return;
  }
  lastFrameUs = now;
  const float seconds = min(elapsed, kMaxPhysicsStepUs) / 1000000.0f;

  for (size_t i = 0; i < kBoxCount; ++i) {
    dirtyRegions[i] = boxes[i].bounds();
  }
  Animation::advanceAll(boxes, kBoxCount, seconds, kCanvasWidth, kCanvasHeight, kHudHeight);
  for (size_t i = 0; i < kBoxCount; ++i) {
    dirtyRegions[i] = Animation::combine(dirtyRegions[i], boxes[i].bounds());
  }

  composeFrame();
  for (const auto &region : dirtyRegions) {
    uploadRegion(region);
  }
  if (hudDirty) {
    uploadRegion({0, 0, kCanvasWidth, kHudHeight});
    hudDirty = false;
  }
  recordCompletedFrame();
}
