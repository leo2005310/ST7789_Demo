#pragma once

#include <algorithm>
#include <cmath>
#include <stddef.h>
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

// 选择接近正方形的网格，每个方块在独立单元中居中，初始状态留有间隔。
inline bool arrange(Box *boxes, size_t count, int16_t width,
                    int16_t height, int16_t top) {
  if (count == 0 || width <= 0 || height <= top) return false;
  int16_t largest = 0;
  for (size_t i = 0; i < count; ++i) largest = std::max(largest, boxes[i].size);
  size_t columns = 0;
  int bestDifference = 32767;
  for (size_t candidate = 1; candidate <= count; ++candidate) {
    const size_t rows = (count + candidate - 1) / candidate;
    const int cellWidth = width / candidate;
    const int cellHeight = (height - top) / rows;
    if (cellWidth < largest + 2 || cellHeight < largest + 2) continue;
    const int difference = std::abs(cellWidth - cellHeight);
    if (difference < bestDifference) {
      columns = candidate;
      bestDifference = difference;
    }
  }
  if (columns == 0) return false;
  const int cellWidth = width / columns;
  const int cellHeight = (height - top) / ((count + columns - 1) / columns);
  for (size_t i = 0; i < count; ++i) {
    boxes[i].x = (i % columns) * cellWidth + (cellWidth - boxes[i].size) / 2;
    boxes[i].y = top + (i / columns) * cellHeight + (cellHeight - boxes[i].size) / 2;
  }
  return true;
}

// 沿一个轴扫描移动路径，停在最近的墙或方块前，不先重叠再推开。
inline void moveWithCollisions(Box *boxes, size_t count, size_t index,
                               bool horizontal, float seconds,
                               int16_t width, int16_t height, int16_t top) {
  Box &box = boxes[index];
  float &position = horizontal ? box.x : box.y;
  float &velocity = horizontal ? box.velocityX : box.velocityY;
  if (velocity == 0) return;
  const bool positive = velocity > 0;
  const float perpendicular = horizontal ? box.y : box.x;
  float limit = positive ? (horizontal ? width : height) - box.size
                         : (horizontal ? 0 : top);
  size_t blocker = count;  // count 表示碰到墙。
  constexpr float gap = 1.0f / 64.0f;  // 小间隙避免浮点误差导致渗透。
  for (size_t i = 0; i < count; ++i) {
    if (i == index) continue;
    const Box &other = boxes[i];
    const float otherPerpendicular = horizontal ? other.y : other.x;
    if (perpendicular + box.size <= otherPerpendicular ||
        otherPerpendicular + other.size <= perpendicular) continue;
    const float otherPosition = horizontal ? other.x : other.y;
    if (positive && otherPosition >= position + box.size) {
      const float edge = std::max(position, otherPosition - box.size - gap);
      if (edge < limit) { limit = edge; blocker = i; }
    } else if (!positive && otherPosition + other.size <= position) {
      const float edge = std::min(position, otherPosition + other.size + gap);
      if (edge > limit) { limit = edge; blocker = i; }
    }
  }

  const float target = position + velocity * seconds;
  if ((positive && target < limit) || (!positive && target > limit)) {
    position = target;
    return;
  }
  position = limit;
  if (blocker == count) {
    velocity = -velocity;
  } else {
    float &otherVelocity = horizontal ? boxes[blocker].velocityX
                                     : boxes[blocker].velocityY;
    // 仅在相互靠近时交换法向速度；沿接触面方向的速度不变。
    if ((positive && velocity > otherVelocity) ||
        (!positive && velocity < otherVelocity)) {
      std::swap(velocity, otherVelocity);
    }
  }
}

inline void advanceAll(Box *boxes, size_t count, float seconds,
                       int16_t width, int16_t height, int16_t top) {
  if (seconds <= 0 || count == 0) return;
  float maxSpeed = 0;
  for (size_t i = 0; i < count; ++i) {
    maxSpeed = std::max(maxSpeed, std::fabs(boxes[i].velocityX));
    maxSpeed = std::max(maxSpeed, std::fabs(boxes[i].velocityY));
  }
  // 每个轴每小步最多移动半个像素，降低顺序更新带来的运动偏差。
  const int steps = std::max(1, static_cast<int>(std::ceil(maxSpeed * seconds * 2)));
  const float stepSeconds = seconds / steps;
  for (int step = 0; step < steps; ++step) {
    for (size_t i = 0; i < count; ++i) {
      moveWithCollisions(boxes, count, i, true, stepSeconds, width, height, top);
    }
    for (size_t i = 0; i < count; ++i) {
      moveWithCollisions(boxes, count, i, false, stepSeconds, width, height, top);
    }
  }
}

inline Rect combine(const Rect &a, const Rect &b) {
  const int16_t x = std::min(a.x, b.x);
  const int16_t y = std::min(a.y, b.y);
  return {x, y,
          static_cast<int16_t>(std::max(a.x + a.width, b.x + b.width) - x),
          static_cast<int16_t>(std::max(a.y + a.height, b.y + b.height) - y)};
}
}  // namespace Animation
