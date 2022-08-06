#include "sdl.hpp"
#include <SDL.h>
#include <SDL_events.h>
#include <SDL_rect.h>
#include <SDL_render.h>
#include <SDL_video.h>
#include <cstdio>
#include <glm/glm.hpp>
#include <iostream>
#include <tecs.hpp>
#include <vector>

using namespace Tecs;

using Position = glm::vec2;

struct RenderCopy {
  SDL_Texture* texture;
  int w;
  int h;
};

int main() {
  Coordinator ecs;

  SDL::Context sdl(SDL_INIT_VIDEO, "Space Invaders",
                   {SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 640, 480},
                   SDL_WINDOW_SHOWN, {"fonts/GroovetasticRegular.ttf"});

  printf("SDL initialised\n");

  // Set up player.
  const auto POSITION_COMPONENT = ecs.registerComponent<Position>();
  const auto RENDERCOPY_COMPONENT = ecs.registerComponent<RenderCopy>();

  auto player = ecs.newEntity();
  ecs.addComponent<Position>(player);
  ecs.addComponent<RenderCopy>(player);

  ecs.getComponent<Position>(player) = glm::vec2(200, 200);
  {
    auto& rc = ecs.getComponent<RenderCopy>(player);
    rc.texture = sdl.loadTexture("art/player.png");
    SDL_QueryTexture(rc.texture, nullptr, nullptr, &rc.w, &rc.h);
  }

  // A system that simply calls SDL_RenderCopy().
  struct RenderCopySystem : System {
    SDL_Renderer* renderer = nullptr;
    void run(const std::set<Entity> &entities, Coordinator &ecs) {
      for (auto& e : entities) {
        const auto& pos = ecs.getComponent<Position>(e);
        const auto& rc = ecs.getComponent<RenderCopy>(e);
        const SDL_Rect renderRect = {(int) pos.x, (int) pos.y, rc.w, rc.h};
        SDL_RenderCopy(renderer, rc.texture, nullptr, &renderRect);
      }
    }
  } renderCopySystem;
  renderCopySystem.id = ecs.registerSystem({POSITION_COMPONENT, RENDERCOPY_COMPONENT});
  renderCopySystem.renderer = sdl.renderer;

  printf("ECS initialised\n");
  
  bool quit = false;
  while (!quit) {

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


    sdl.renderClear();
    renderCopySystem.run(ecs.systems.systemInterests[renderCopySystem.id], ecs);
    sdl.renderPresent();
  }
}
