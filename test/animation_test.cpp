// Host-side checks: g++ -std=c++11 -Iinclude test/animation_test.cpp -o <output>
#include <cassert>
#include <cmath>
#include <cstdio>
#include "animation.h"

static bool contains(const Animation::Rect &outer, const Animation::Rect &inner) {
  return outer.x <= inner.x && outer.y <= inner.y &&
         outer.x + outer.width >= inner.x + inner.width &&
         outer.y + outer.height >= inner.y + inner.height;
}

int main() {
  // Overshoot is reflected, including multiple wall crossings in one update.
  float position = 9, velocity = 4;
  Animation::moveAxis(position, velocity, 1, 0, 10);
  assert(position == 7 && velocity == -4);
  position = 5;
  velocity = 100;
  Animation::moveAxis(position, velocity, 1, 0, 10);
  assert(position == 5 && velocity == 100);

  // Hitting a boundary exactly must immediately point the velocity inward.
  position = 1;
  velocity = -2;
  Animation::moveAxis(position, velocity, 0.5f, 0, 10);
  assert(position == 0 && velocity == 2);

  // Motion over equal real time is independent of the number of frames.
  Animation::Box a{100, 100, 145, -91, 36, 0};
  auto b = a;
  for (int i = 0; i < 200; ++i) Animation::advance(a, 0.01f, 240, 240, 24);
  for (int i = 0; i < 40; ++i) Animation::advance(b, 0.05f, 240, 240, 24);
  assert(std::fabs(a.x - b.x) < 0.01f && std::fabs(a.y - b.y) < 0.01f);
  assert(a.velocityX == b.velocityX && a.velocityY == b.velocityY);

  // Long runs exercise all four walls, a reserved HUD, and varied frame times.
  for (int size = 12; size <= 36; ++size) {
    Animation::Box box{0, 24, -145, -91, static_cast<int16_t>(size), 0};
    bool left = false, right = false, top = false, bottom = false;
    for (int i = 0; i < 10000; ++i) {
      const auto old = box.bounds();
      const float oldVx = box.velocityX, oldVy = box.velocityY;
      Animation::advance(box, 0.001f * (1 + i % 50), 240, 240, 24);
      const auto current = box.bounds();
      const auto dirty = Animation::combine(old, current);
      assert(box.x >= 0 && box.x <= 240 - size);
      assert(box.y >= 24 && box.y <= 240 - size);
      assert(contains(dirty, old) && contains(dirty, current));
      assert(dirty.x >= 0 && dirty.y >= 24);
      assert(dirty.x + dirty.width <= 240 && dirty.y + dirty.height <= 240);
      if (oldVx != box.velocityX) (oldVx < 0 ? left : right) = true;
      if (oldVy != box.velocityY) (oldVy < 0 ? top : bottom) = true;
    }
    assert(left && right && top && bottom);
  }
  std::puts("PASS: reflection, frame-independent motion, HUD bounds, dirty coverage");
}
