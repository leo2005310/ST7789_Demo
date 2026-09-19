#pragma once

#include <algorithm>
#include <stdint.h>

namespace Animation {
struct Rect {
  int16_t x, y, width, height;
};

struct Box {
  float x, y;
  float velocityX, velocityY;  // 像素/秒。
  int16_t size;
  uint16_t color;

  Rect bounds() const {
    return {static_cast<int16_t>(x), static_cast<int16_t>(y), size, size};
  }
};

// 保留越界距离，使反弹前后的运动速度一致，也支持一次跨越多个边界。
inline void moveAxis(float &position, float &velocity, float elapsedSeconds,
                     float lower, float upper) {
  if (upper <= lower) {
    position = lower;
    return;
  }
  position += velocity * elapsedSeconds;
  while (position < lower || position > upper) {
    if (position < lower) {
      position = 2.0f * lower - position;
      velocity = -velocity;
    }
    if (position > upper) {
      position = 2.0f * upper - position;
      velocity = -velocity;
    }
  }
  if ((position == lower && velocity < 0) ||
      (position == upper && velocity > 0)) {
    velocity = -velocity;
  }
}

inline void advance(Box &box, float elapsedSeconds, int16_t width,
                    int16_t height, int16_t top) {
  moveAxis(box.x, box.velocityX, elapsedSeconds, 0, width - box.size);
  moveAxis(box.y, box.velocityY, elapsedSeconds, top, height - box.size);
}

inline Rect combine(const Rect &a, const Rect &b) {
  const int16_t x = std::min(a.x, b.x);
  const int16_t y = std::min(a.y, b.y);
  return {x, y,
          static_cast<int16_t>(std::max(a.x + a.width, b.x + b.width) - x),
          static_cast<int16_t>(std::max(a.y + a.height, b.y + b.height) - y)};
}
}  // namespace Animation
