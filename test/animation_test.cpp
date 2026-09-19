// Host-side checks: g++ -std=c++11 -Iinclude test/animation_test.cpp -o <output>
#include <cassert>
#include <cmath>
#include <cstdio>
#include <random>
#include "animation.h"

static bool contains(const Animation::Rect &outer, const Animation::Rect &inner) {
  return outer.x <= inner.x && outer.y <= inner.y &&
         outer.x + outer.width >= inner.x + inner.width &&
         outer.y + outer.height >= inner.y + inner.height;
}

static void checkSeparated(const Animation::Box *boxes, size_t count,
                           int width = 240, int height = 240, int top = 24) {
  for (size_t i = 0; i < count; ++i) {
    const auto &a = boxes[i];
    assert(a.x >= 0 && a.x + a.size <= width);
    assert(a.y >= top && a.y + a.size <= height);
    for (size_t j = i + 1; j < count; ++j) {
      const auto &b = boxes[j];
      assert(a.x + a.size <= b.x || b.x + b.size <= a.x ||
             a.y + a.size <= b.y || b.y + b.size <= a.y);
      const auto ar = a.bounds(), br = b.bounds();
      assert(ar.x + ar.width <= br.x || br.x + br.width <= ar.x ||
             ar.y + ar.height <= br.y || br.y + br.height <= ar.y);
    }
  }
}

static void testCollisions() {
  Animation::Box pair[] = {{40, 60, 60, 10, 12, 0}, {54, 60, -60, 10, 12, 0}};
  Animation::advanceAll(pair, 2, 0.05f, 240, 240, 24);
  assert(pair[0].velocityX == -60 && pair[1].velocityX == 60);
  assert(pair[0].velocityY == 10 && pair[1].velocityY == 10);
  checkSeparated(pair, 2);

  Animation::Box vertical[] = {{60, 40, 10, 60, 12, 0}, {60, 54, 10, -60, 12, 0}};
  Animation::advanceAll(vertical, 2, 0.05f, 240, 240, 24);
  assert(vertical[0].velocityY == -60 && vertical[1].velocityY == 60);
  assert(vertical[0].velocityX == 10 && vertical[1].velocityX == 10);
  checkSeparated(vertical, 2);

  // The leading box is faster: do not bounce two boxes moving apart.
  Animation::Box apart[] = {{40, 60, 60, 0, 12, 0}, {52, 60, 120, 0, 12, 0}};
  Animation::advanceAll(apart, 2, 0.05f, 240, 240, 24);
  assert(apart[0].velocityX == 60 && apart[1].velocityX == 120);
  checkSeparated(apart, 2);

  // Enough travel to cross the other box in a single frame, without tunnelling.
  Animation::Box fast[] = {{40, 60, 2000, 0, 12, 0}, {120, 60, -2000, 0, 12, 0}};
  Animation::advanceAll(fast, 2, 0.05f, 240, 240, 24);
  assert(fast[0].x + fast[0].size <= fast[1].x);
  assert(fast[0].velocityX < 0 && fast[1].velocityX > 0);
  checkSeparated(fast, 2);

  // A touching chain against the wall must not be pushed outside or overlap.
  Animation::Box chain[] = {{0, 24, -120, -50, 12, 0},
                            {12, 24, -160, -40, 12, 0},
                            {24, 24, -200, -30, 12, 0}};
  for (int i = 0; i < 1000; ++i) {
    Animation::advanceAll(chain, 3, 0.05f, 240, 240, 24);
    checkSeparated(chain, 3);
  }

  // Varied sizes, dense populations, frame times and directions over long runs.
  std::minstd_rand rng(12345);
  for (size_t count : {size_t(1), size_t(8), size_t(30)}) {
    Animation::Box boxes[32]{};
    float originalEnergy = 0;
    for (size_t i = 0; i < count; ++i) {
      boxes[i].size = 12 + (24 * i) / (count > 1 ? count - 1 : 1);
      boxes[i].velocityX = static_cast<int>(rng() % 401) - 200;
      boxes[i].velocityY = static_cast<int>(rng() % 401) - 200;
      originalEnergy += boxes[i].velocityX * boxes[i].velocityX +
                        boxes[i].velocityY * boxes[i].velocityY;
    }
    assert(Animation::arrange(boxes, count, 240, 240, 24));
    checkSeparated(boxes, count);
    for (int frame = 0; frame < 3000; ++frame) {
      Animation::Rect previous[32];
      for (size_t i = 0; i < count; ++i) previous[i] = boxes[i].bounds();
      Animation::advanceAll(boxes, count, 0.001f * (1 + rng() % 50), 240, 240, 24);
      checkSeparated(boxes, count);
      float energy = 0;
      for (size_t i = 0; i < count; ++i) {
        const auto current = boxes[i].bounds();
        const auto dirty = Animation::combine(previous[i], current);
        assert(contains(dirty, previous[i]) && contains(dirty, current));
        assert(dirty.x >= 0 && dirty.y >= 24);
        assert(dirty.x + dirty.width <= 240 && dirty.y + dirty.height <= 240);
        energy += boxes[i].velocityX * boxes[i].velocityX +
                  boxes[i].velocityY * boxes[i].velocityY;
      }
      assert(energy == originalEnergy);
    }
  }
  Animation::Box tooMany[32]{};
  for (auto &box : tooMany) box.size = 36;
  assert(!Animation::arrange(tooMany, 32, 240, 240, 24));
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
  testCollisions();
  std::puts("PASS: walls, pair collisions, no tunnelling, dense scenes, layout, dirty coverage");
}
