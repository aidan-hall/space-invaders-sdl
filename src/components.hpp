#ifndef GAME_COMPONENTS_HPP
#define GAME_COMPONENTS_HPP

#include <SDL_render.h>
#include <glm/ext/vector_float2.hpp>

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
struct Animation {
  SDL_Rect src_rect;
  int step;
  int n_steps;
  int frames_per_step;
  int current_step_frames;
};
struct Health {
  float current;
  float max;
};
struct HealthBar {
  float hover_distance;
};
#endif // GAME_COMPONENTS_HPP
