//
// Created by aidan on 13/09/22.
//

#ifndef GAME_COMPONENTS_HPP
#define GAME_COMPONENTS_HPP

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
struct Position {
  glm::vec2 p;
};
struct Velocity {
  glm::vec2 v;
};
struct Player {};
struct Alien {
  float start_x;
};
struct RenderCopy {
  SDL_Texture *texture;
  int w;
  int h;
};
struct Health {
  float current;
  float max;
};
struct HealthBar {
  float hover_distance;
};
#endif // GAME_COMPONENTS_HPP
