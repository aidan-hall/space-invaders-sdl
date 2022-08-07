#include "sdl.hpp"
#include <SDL.h>
#include <SDL_events.h>
#include <SDL_keyboard.h>
#include <SDL_rect.h>
#include <SDL_render.h>
#include <SDL_timer.h>
#include <SDL_video.h>
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

struct RenderCopy {
  SDL_Texture *texture;
  int w;
  int h;
};

struct Chasing {
  Entity target;
  glm::vec1 speed;
};

struct Health {
  float current;
  float max;
};

// Framerate.

constexpr int32_t SCREEN_FPS = 60;
constexpr int32_t SCREEN_TICKS_PER_FRAME = 1000 / SCREEN_FPS;

void makeStaticSprite(Entity entity, Coordinator &ecs, Position initPos,
                      SDL_Texture *texture) {
  ecs.addComponent<Position>(entity);
  ecs.addComponent<RenderCopy>(entity);
  ecs.addComponent<Velocity>(entity);

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
                   SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE,
                   {"fonts/GroovetasticRegular.ttf"});

  printf("SDL initialised\n");

  const auto POSITION_COMPONENT = ecs.registerComponent<Position>();
  const auto RENDERCOPY_COMPONENT = ecs.registerComponent<RenderCopy>();
  const auto VELOCITY_COMPONENT = ecs.registerComponent<Velocity>();
  const auto CHASING_COMPONENT = ecs.registerComponent<Chasing>();
  const auto PLAYER_COMPONENT = ecs.registerComponent<Player>();
  const auto HEALTH_COMPONENT = ecs.registerComponent<Health>();

  // Set up player.
  auto player = ecs.newEntity();
  makeStaticSprite(player, ecs, {{100, 100}},
                   sdl.loadTexture("art/player.png"));
  ecs.addComponent<Velocity>(player);
  ecs.addComponent<Player>(player);
  ecs.getComponent<Velocity>(player) = {{0, 0}};
  ecs.addComponent<Health>(player);
  ecs.getComponent<Health>(player) = {5.0, 7.0};

  // Set up aliens.
  auto alienTexture = sdl.loadTexture("art/alien1.png");
  std::vector<Entity> aliens;
  for (int i = 0; i < 32; ++i) {
    for (int j = 0; j < 32; ++j) {
      auto alien = ecs.newEntity();
      makeStaticSprite(alien, ecs, {{j + 3 + i * 50, i * 3 + j * 50}},
                       alienTexture);
      ecs.addComponent<Chasing>(alien);
      ecs.getComponent<Chasing>(alien) = {alien - 1, glm::vec1(2)};
      aliens.push_back(alien);
    }
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

  struct ChasingSystem : System {
    using System::System;
    void run(const std::set<Entity> &entities, Coordinator &ecs) {
      for (auto &e : entities) {
        const auto &chasing = ecs.getComponent<Chasing>(e);
        const auto &targetPos = ecs.getComponent<Position>(chasing.target).p;
        const auto &myPos = ecs.getComponent<Position>(e).p;
        auto &velocity = ecs.getComponent<Velocity>(e);

        auto difference = targetPos - myPos;
        constexpr float CHASING_SPACE = 30;
        if (glm::length(difference) > CHASING_SPACE) {
          velocity = {glm::normalize(difference) * chasing.speed};
        } else {
          velocity = {{0, 0}};
        }
      }
    }
  } chasingSystem(componentsSignature({CHASING_COMPONENT, POSITION_COMPONENT,
                                       VELOCITY_COMPONENT}),
                  ecs);

  struct PlayerControlSystem : System {
    using System::System;
    void run(const std::set<Entity> &entities, Coordinator &ecs) {
      auto keyboardState = SDL_GetKeyboardState(nullptr);
      constexpr float PLAYER_MAX_SPEED = 5.0;
      constexpr float PLAYER_MAX_SPEED_SQUARED =
          PLAYER_MAX_SPEED * PLAYER_MAX_SPEED;
      for (auto &e : entities) {
        auto &velocity = ecs.getComponent<Velocity>(e).v;
        if (keyboardState[SDL_SCANCODE_UP]) {
          velocity.y -= 0.2;
        }
        if (keyboardState[SDL_SCANCODE_DOWN]) {
          velocity.y += 0.2;
        }
        if (keyboardState[SDL_SCANCODE_LEFT]) {
          velocity.x -= 0.2;
        }
        if (keyboardState[SDL_SCANCODE_RIGHT]) {
          velocity.x += 0.2;
        }

        if (velocity.x * velocity.x + velocity.y * velocity.y >=
            PLAYER_MAX_SPEED_SQUARED) {
          velocity = glm::normalize(velocity) * PLAYER_MAX_SPEED;
        }
      }
    }
  } playerControlSystem(
      componentsSignature({PLAYER_COMPONENT, VELOCITY_COMPONENT}), ecs);

  // A system that simply calls SDL_RenderCopy().
  struct RenderCopySystem : System {
    SDL_Renderer *renderer = nullptr;

    void run(const std::set<Entity> &entities, Coordinator &ecs) {
      for (auto &e : entities) {
        const auto &pos = ecs.getComponent<Position>(e).p;
        const auto &rc = ecs.getComponent<RenderCopy>(e);
        const SDL_Rect renderRect = {(int)pos.x, (int)pos.y, rc.w, rc.h};
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
      constexpr int BAR_HOVER_DISTANCE = -20;
      SDL_Rect current_bar;
      SDL_Rect empty_bar;
      empty_bar.w = BAR_LENGTH;
      empty_bar.h = BAR_HEIGHT;
      current_bar.h = BAR_HEIGHT;
      for (auto &e : entities) {
        const auto &pos = ecs.getComponent<Position>(e).p;
        const auto &health = ecs.getComponent<Health>(e);
        empty_bar.y = current_bar.y = pos.y + BAR_HOVER_DISTANCE - BAR_HEIGHT;
        current_bar.x = pos.x - (float)BAR_LENGTH/2;
        current_bar.w = (health.current/health.max) * BAR_LENGTH;
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
    runSystem(chasingSystem, ecs);
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
