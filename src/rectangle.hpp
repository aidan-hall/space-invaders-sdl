//
// Created by aidan on 13/09/22.
//

#ifndef GAME_RECTANGLE_HPP
#define GAME_RECTANGLE_HPP

#include "components.hpp"
#include "sdl.hpp"
#include <SDL.h>
#include <SDL_events.h>
#include <SDL_filesystem.h>
#include <SDL_hints.h>
#include <SDL_keyboard.h>
#include <SDL_mixer.h>
#include <SDL_rect.h>
#include <SDL_render.h>
#include <SDL_video.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include <random>
#include <ranges>
#include <string>
#include <tecs.hpp>
#include <thread>
#include <tuple>
#include <vector>

struct Rectangle {
  float x;
  float y;
  float w;
  float h;

  explicit Rectangle(SDL_Rect rect)
      : Rectangle(static_cast<float>(rect.x), static_cast<float>(rect.y),
                  static_cast<float>(rect.w), static_cast<float>(rect.h)) {}

  Rectangle(float x, float y, float w, float h) : x{x}, y{y}, w{w}, h{h} {}
};
inline bool rectangleIntersection(const Rectangle &a, const Rectangle &b) {
  return !(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y ||
           b.y + b.h <= a.y);
}
inline bool pointInRectangle(const Rectangle &a, const Position &pos) {
  const auto &p = pos.p;
  return (a.x < p.x && p.x < a.x + a.w && a.y < p.y && p.y < a.y + a.h);
}
#endif // GAME_RECTANGLE_HPP
