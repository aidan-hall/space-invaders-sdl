#include "sdl.hpp"
#include <SDL.h>
#include <SDL_events.h>
#include <SDL_rect.h>
#include <SDL_render.h>
#include <SDL_timer.h>
#include <SDL_video.h>
#include <cstdio>
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

  // Set up player.
  auto player = ecs.newEntity();
  makeStaticSprite(player, ecs, {{100, 100}},
                   sdl.loadTexture("art/player.png"));

  // Set up aliens.
  auto alienTexture = sdl.loadTexture("art/alien1.png");
  std::vector<Entity> aliens;
  for (int i = 0; i < 1000; ++i) {
    auto alien = ecs.newEntity();
    makeStaticSprite(alien, ecs, {{i * 10, 200 + i * 10}}, alienTexture);
    ecs.addComponent<Chasing>(alien);
    ecs.getComponent<Chasing>(alien) = {player, glm::vec1(1)};
    aliens.push_back(alien);
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

        velocity = {glm::normalize(targetPos - myPos) * chasing.speed};
      }
    }
  } chasingSystem(componentsSignature({CHASING_COMPONENT, POSITION_COMPONENT,
                                       VELOCITY_COMPONENT}),
                  ecs);

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

    runSystem(chasingSystem, ecs);
    runSystem(velocitySystem, ecs);

    sdl.renderClear();
    runSystem(renderCopySystem, ecs);
    sdl.renderPresent();
    SDL_Delay(tick + SCREEN_TICKS_PER_FRAME - SDL_GetTicks64());
  }
}
