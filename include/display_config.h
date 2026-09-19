#pragma once

#include <Arduino.h>

namespace DisplayConfig {
// ST7789 接线；SDA 是 SPI MOSI，不是 I2C SDA。
constexpr int8_t kSck = 12;
constexpr int8_t kMosi = 11;
constexpr int8_t kReset = 8;
constexpr int8_t kDc = 9;
constexpr int8_t kCs = 10;
constexpr int8_t kBacklight = 7;

constexpr uint16_t kWidth = 240;
constexpr uint16_t kHeight = 240;
constexpr uint8_t kRotation = 0;  // 0 / 1 / 2 / 3
constexpr bool kInvertColors = true;
constexpr uint8_t kBacklightOn = HIGH;  // 低电平点亮的模块改成 LOW。
constexpr uint32_t kSpiFrequency = 20000000;  // 20 MHz

constexpr uint8_t kBoxCount = 8;
constexpr int16_t kMinBoxSize = 12;
constexpr int16_t kMaxBoxSize = 36;
constexpr float kSpeedScale = 1.0f;  // 所有方块的速度倍率。
constexpr int16_t kHudHeight = 24;  // 顶部 FPS 信息栏，不参与弹跳。
constexpr uint32_t kFrameIntervalUs = 16667;  // 帧率上限约 60 FPS。
constexpr uint32_t kFpsUpdateIntervalUs = 500000;  // 每 0.5 秒更新实测 FPS。
constexpr uint32_t kMaxPhysicsStepUs = 50000;  // 暂停后避免物体大幅跳跃。
}  // namespace DisplayConfig
