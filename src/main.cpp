#include "sdl.hpp"
#include <SDL.h>
#include <SDL_events.h>
#include <SDL_hints.h>
#include <SDL_keyboard.h>
#include <SDL_rect.h>
#include <SDL_render.h>
#include <SDL_timer.h>
#include <SDL_video.h>
#include <cstdint>
#include <cstdio>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include <tecs.hpp>
#include <vector>

using namespace Tecs;

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
  float hover_distance;
};

constexpr float ALIEN_INIT_SPEED = 0.5;
constexpr float ALIEN_SHUFFLE_DISTANCE = 100.0;
constexpr float ALIEN_DROP_DISTANCE = 20.0;
constexpr int ALIEN_ROWS = 4;
constexpr int ALIEN_COLUMNS = 10;
constexpr int INITIAL_N_ALIENS = ALIEN_ROWS * ALIEN_COLUMNS;
constexpr float ALIEN_SPEED_INCREMENT = 0.05;

// Framerate.

constexpr int32_t SCREEN_FPS = 60;
constexpr int32_t SCREEN_TICKS_PER_FRAME = 1000 / SCREEN_FPS;

void makeStaticSprite(Entity entity, Coordinator &ecs, Position initPos,
                      SDL_Texture *texture) {
  ecs.addComponent<Position>(entity);
  ecs.addComponent<RenderCopy>(entity);

  ecs.getComponent<Position>(entity) = initPos;
  {
    auto &rc = ecs.getComponent<RenderCopy>(entity);
    rc.texture = texture;
    SDL_QueryTexture(rc.texture, nullptr, nullptr, &rc.w, &rc.h);
  }
}

int main() {
  Coordinator ecs;

  SDL::Context sdl(SDL_INIT_VIDEO, "Space Invaders",
                   {SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480},
                   SDL_WINDOW_SHOWN, {"fonts/GroovetasticRegular.ttf"});

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

  printf("SDL initialised\n");

  const auto POSITION_COMPONENT = ecs.registerComponent<Position>();
  const auto RENDERCOPY_COMPONENT = ecs.registerComponent<RenderCopy>();
  const auto VELOCITY_COMPONENT = ecs.registerComponent<Velocity>();
  const auto PLAYER_COMPONENT = ecs.registerComponent<Player>();
  const auto HEALTH_COMPONENT = ecs.registerComponent<Health>();
  const auto ALIEN_COMPONENT = ecs.registerComponent<Alien>();

  // Set up player.
  auto player = ecs.newEntity();
  makeStaticSprite(player, ecs,
                   {{sdl.windowDimensions.w / 2, sdl.windowDimensions.h - 40}},
                   sdl.loadTexture("art/player.png"));
  ecs.addComponent<Velocity>(player);
  ecs.addComponent<Player>(player);
  ecs.addComponent<Health>(player);
  ecs.getComponent<Health>(player) = {3.0, 3.0, 25.0};

  // Set up aliens.

  auto alienTexture = sdl.loadTexture("art/alien1.png");
  std::vector<Entity> aliens;
  for (int j = 1; j <= ALIEN_ROWS; ++j) {
    for (int i = 1; i <= ALIEN_COLUMNS; ++i) {
      auto alien = ecs.newEntity();
      glm::vec2 pos = {i * 50, j * 40};
      makeStaticSprite(alien, ecs, {pos}, alienTexture);
      ecs.addComponent<Alien>(alien);
      ecs.addComponent<Velocity>(alien);

      ecs.getComponent<Alien>(alien).start_x = pos.x;
      ecs.getComponent<Velocity>(alien) = {{ALIEN_INIT_SPEED, 0}};
      aliens.push_back(alien);
    }
  }

  // Set up barriers.
  auto barrierTexture = sdl.loadTexture("art/barrier.png");
  for (int i = 0; i < 4; ++i) {
    auto barrier = ecs.newEntity();
    makeStaticSprite(barrier, ecs,
                     {{80 + 160 * i, sdl.windowDimensions.h - 150}},
                     barrierTexture);
    auto &rc = ecs.getComponent<RenderCopy>(barrier);
    constexpr int BARRIER_SCALE = 3;
    rc.w *= BARRIER_SCALE;
    rc.h *= BARRIER_SCALE;

    ecs.addComponent<Health>(barrier);
    ecs.getComponent<Health>(barrier) = {100.0, 100.0, 40.0};
  }

  struct VelocitySystem : public System {
    using System::System;
    void run(const std::set<Entity> &entities, Coordinator &ecs) {
      for (auto &e : entities) {
        auto &pos = ecs.getComponent<Position>(e).p;
        const auto &vel = ecs.getComponent<Velocity>(e).v;
        pos += vel;
      }
    }
  } velocitySystem(
      componentsSignature({VELOCITY_COMPONENT, POSITION_COMPONENT}), ecs);

  struct PlayerControlSystem : System {
    using System::System;
    SDL::Context *sdl;
    void run(const std::set<Entity> &entities, Coordinator &ecs) {
      auto keyboardState = SDL_GetKeyboardState(nullptr);
      constexpr float PLAYER_MAX_SPEED = 5.0;
      constexpr float PLAYER_MAX_SPEED_SQUARED =
          PLAYER_MAX_SPEED * PLAYER_MAX_SPEED;
      for (auto &e : entities) {
        auto &velocity = ecs.getComponent<Velocity>(e).v;
        if (keyboardState[SDL_SCANCODE_LEFT]) {
          velocity.x -= 0.2;
        }
        if (keyboardState[SDL_SCANCODE_RIGHT]) {
          velocity.x += 0.2;
        }

        auto &pos = ecs.getComponent<Position>(e).p;

        constexpr int WINDOW_MARGIN = 50;
        if (pos.x > sdl->windowDimensions.w - WINDOW_MARGIN) {
          pos.x = sdl->windowDimensions.w - WINDOW_MARGIN;
          velocity.x = 0;
        } else if (pos.x < WINDOW_MARGIN) {
          pos.x = WINDOW_MARGIN;
          velocity.x = 0;
        } else if (velocity.x * velocity.x + velocity.y * velocity.y >=
                   PLAYER_MAX_SPEED_SQUARED) {
          velocity = glm::normalize(velocity) * PLAYER_MAX_SPEED;
        }
      }
    }
  } playerControlSystem(
      componentsSignature(
          {PLAYER_COMPONENT, VELOCITY_COMPONENT, POSITION_COMPONENT}),
      ecs);
  playerControlSystem.sdl = &sdl;

  struct AlienMovementSystem : System {
    using System::System;
    SDL::Context *sdl;
    float alien_speed = ALIEN_INIT_SPEED;
    void run(const std::set<Entity> &entities, Coordinator &ecs) {
      int current_n_aliens = 0;
      for (auto &e : entities) {
        current_n_aliens += 1;

        auto &pos = ecs.getComponent<Position>(e).p;
        auto &vel = ecs.getComponent<Velocity>(e).v;
        const auto &start_x = ecs.getComponent<Alien>(e).start_x;
        if (pos.x < start_x) {
          pos.y += ALIEN_DROP_DISTANCE;
          vel.x = alien_speed;
        } else if (pos.x > start_x + ALIEN_SHUFFLE_DISTANCE) {
          pos.y += ALIEN_DROP_DISTANCE;
          vel.x = -alien_speed;
        }
      }

      alien_speed = ALIEN_INIT_SPEED + ALIEN_SPEED_INCREMENT * (INITIAL_N_ALIENS - current_n_aliens);
    }
  } alienMovementSystem(
      componentsSignature(
          {ALIEN_COMPONENT, POSITION_COMPONENT, VELOCITY_COMPONENT}),
      ecs);
  alienMovementSystem.sdl = &sdl;

  // A system that simply calls SDL_RenderCopy().
  struct RenderCopySystem : System {
    SDL_Renderer *renderer = nullptr;

    void run(const std::set<Entity> &entities, Coordinator &ecs) {
      for (auto &e : entities) {
        const auto &pos = ecs.getComponent<Position>(e).p;
        const auto &rc = ecs.getComponent<RenderCopy>(e);
        const SDL_Rect renderRect = {(int)pos.x - rc.w / 2,
                                     (int)pos.y - rc.h / 2, rc.w, rc.h};
        SDL_RenderCopy(renderer, rc.texture, nullptr, &renderRect);
      }
    }

    RenderCopySystem(const Signature &sig, Coordinator &coord,
                     SDL_Renderer *theRenderer)
        : System(sig, coord) {
      this->renderer = theRenderer;
    }
  } renderCopySystem(
      componentsSignature({POSITION_COMPONENT, RENDERCOPY_COMPONENT}), ecs,
      sdl.renderer);

  struct HealthSystem : System {
    SDL_Renderer *renderer = nullptr;

    using System::System;
    void run(const std::set<Entity> &entities, Coordinator &ecs) {
      constexpr int BAR_HEIGHT = 5;
      constexpr int BAR_LENGTH = 30;
      SDL_Rect current_bar;
      SDL_Rect empty_bar;
      empty_bar.w = BAR_LENGTH;
      empty_bar.h = BAR_HEIGHT;
      current_bar.h = BAR_HEIGHT;
      for (auto &e : entities) {
        const auto &pos = ecs.getComponent<Position>(e).p;
        const auto &health = ecs.getComponent<Health>(e);
        empty_bar.y = current_bar.y = pos.y + health.hover_distance - BAR_HEIGHT;
        current_bar.x = pos.x - (float)BAR_LENGTH / 2;
        current_bar.w = (health.current / health.max) * BAR_LENGTH;
        empty_bar.x = current_bar.x + current_bar.w;
        empty_bar.w = BAR_LENGTH - current_bar.w;
        // Draw remaining health.
        SDL_SetRenderDrawColor(renderer, 0xFF, 0xFF, 0x00, 0x00);
        SDL_RenderFillRect(renderer, &current_bar);
        // Draw leftover health bar.
        SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0x00);
        SDL_RenderFillRect(renderer, &empty_bar);
      }
    }
  } healthSystem(componentsSignature({HEALTH_COMPONENT, POSITION_COMPONENT}),
                 ecs);
  healthSystem.renderer = sdl.renderer;

  printf("ECS initialised\n");

  bool quit = false;
  while (!quit) {

    auto tick = SDL_GetTicks64();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch ((SDL_EventType)e.type) {
      case SDL_QUIT:
        quit = true;
        break;
      default:
        break;
      }
    }

    runSystem(playerControlSystem, ecs);
    runSystem(alienMovementSystem, ecs);
    runSystem(velocitySystem, ecs);

    SDL_SetRenderDrawColor(sdl.renderer, 0x00, 0x00, 0x00, 0x00);

    // Rendering
    sdl.renderClear();

    runSystem(renderCopySystem, ecs);
    runSystem(healthSystem, ecs);

    sdl.renderPresent();

    SDL_Delay(tick + SCREEN_TICKS_PER_FRAME - SDL_GetTicks64());
  }
}
