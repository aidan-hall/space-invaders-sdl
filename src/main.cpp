#include "sdl.hpp"
#include <SDL.h>
#include <SDL_events.h>
#include <SDL_hints.h>
#include <SDL_keyboard.h>
#include <SDL_mixer.h>
#include <SDL_rect.h>
#include <SDL_render.h>
#include <SDL_timer.h>
#include <SDL_video.h>
#include <cstdint>
#include <cstdio>
#include <glm/geometric.hpp>
#include <glm/glm.hpp>
#include <iostream>
#include <random>
#include <string>
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
};

struct HealthBar {
  float hover_distance;
};

struct Rectangle {
  float x;
  float y;
  float w;
  float h;

  static Rectangle fromSdlRect(SDL_Rect rect) {

    Rectangle it;
    it.x = static_cast<float>(rect.x);
    it.y = static_cast<float>(rect.y);
    it.w = static_cast<float>(rect.w);
    it.h = static_cast<float>(rect.h);
    return it;
  }
};
inline bool rectangleIntersection(const Rectangle &a, const Rectangle &b) {
  return !(a.x + a.w <= b.x || b.x + b.w <= a.x || a.y + a.h <= b.y ||
           b.y + b.h <= a.y);
}

inline bool pointInRectangle(const Rectangle &a, const Position &pos) {
  const auto &p = pos.p;
  return (a.x < p.x && p.x < a.x + a.w && a.y < p.y && p.y < a.y + a.h);
}

using LayerMask = std::bitset<8>;

struct CollisionBounds {
  glm::vec2 spacing;
  LayerMask layer;
  inline Rectangle rectangle(const Position &pos) const {
    return {pos.p.x - spacing.x, pos.p.y - spacing.y, spacing.x * 2,
            spacing.y * 2};
  }
  inline SDL_Rect sdl_rectangle(const Position &pos) const {
    Rectangle box = rectangle(pos);
    SDL_Rect sdl_rectangle = {static_cast<int>(box.x), static_cast<int>(box.y),
                              static_cast<int>(box.w), static_cast<int>(box.h)};
    return sdl_rectangle;
  }
};

constexpr float ALIEN_INIT_SPEED = 0.2;
constexpr float ALIEN_SHUFFLE_DISTANCE = 200.0;
constexpr float ALIEN_DROP_DISTANCE = 10.0;
constexpr int ALIEN_ROWS = 4;
constexpr int ALIEN_COLUMNS = 20;
constexpr float ALIEN_SPEED_INCREMENT = 0.03;

// Framerate.

constexpr int32_t SCREEN_FPS = 60;
constexpr int32_t SCREEN_TICKS_PER_FRAME = 1000 / SCREEN_FPS;

// Sounds
Mix_Chunk *sound_shoot = nullptr;
Mix_Chunk *sound_explosion = nullptr;
SDL_Texture *alien_texture = nullptr;
SDL_Texture *player_texture = nullptr;

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

Entity makeBullet(Coordinator &ecs, Position initPos, Velocity initVel,
                  SDL_Texture *texture, const CollisionBounds &bounds) {
  Mix_PlayChannel(-1, sound_shoot, 0);
  auto bullet = ecs.newEntity();
  makeStaticSprite(bullet, ecs, initPos, texture);

  ecs.addComponent<Velocity>(bullet);
  ecs.getComponent<Velocity>(bullet) = {initVel};
  ecs.addComponent<Health>(bullet);
  ecs.getComponent<Health>(bullet) = {1.0, 1.0};
  ecs.addComponent<CollisionBounds>(bullet);
  ecs.getComponent<CollisionBounds>(bullet) = bounds;

  return bullet;
}

enum class GameEvent {
  GameOver,
  Quit, // Called when player closes window.
  Scored,
  Win,
};

std::vector<GameEvent> events;
#define SCORE_PREFIX "Score: "

void updateTextTexture(Coordinator &ecs, SDL::Context &sdl, Entity score_entity,
                       uint32_t font_idx, const std::string &text) {
  auto t = sdl.loadFromRenderedText(text, {255, 255, 255, 0}, font_idx);
  auto &r = ecs.getComponent<RenderCopy>(score_entity);
  r.texture = t.texture;
  r.w = t.w;
  r.h = t.h;
}

struct RenderCopySystem : System {
  SDL_Renderer *renderer = nullptr;

  void run(const std::set<Entity> &entities, Coordinator &ecs) {
    for (auto &e : entities) {
      const auto &[pos] = ecs.getComponent<Position>(e);
      const auto &[texture, w, h] = ecs.getComponent<RenderCopy>(e);
      const SDL_Rect renderRect = {(int)pos.x - w / 2, (int)pos.y - h / 2, w,
                                   h};
      SDL_RenderCopy(renderer, texture, nullptr, &renderRect);
    }
  }

  RenderCopySystem(const Signature &sig, Coordinator &coord,
                   SDL_Renderer *theRenderer)
      : System(sig, coord) {
    this->renderer = theRenderer;
  }
};

GameEvent gameplay(SDL::Context &sdl, const int alien_rows,
                   const int alien_columns) {

  events.clear();

  Coordinator ecs;

  const auto POSITION_COMPONENT = ecs.registerComponent<Position>();
  const auto RENDERCOPY_COMPONENT = ecs.registerComponent<RenderCopy>();
  const auto VELOCITY_COMPONENT = ecs.registerComponent<Velocity>();
  const auto PLAYER_COMPONENT = ecs.registerComponent<Player>();
  const auto HEALTH_COMPONENT = ecs.registerComponent<Health>();
  const auto HEALTH_BAR_COMPONENT = ecs.registerComponent<HealthBar>();
  const auto ALIEN_COMPONENT = ecs.registerComponent<Alien>();
  const auto COLLISION_BOUNDS_COMPONENT =
      ecs.registerComponent<CollisionBounds>();

  // Set up player.
  auto player = ecs.newEntity();
  makeStaticSprite(player, ecs,
                   {{sdl.windowDimensions.w / 2, sdl.windowDimensions.h - 40}},
                   player_texture);

  ecs.getComponent<RenderCopy>(player).w *= 2;

  ecs.addComponent<Velocity>(player);
  ecs.addComponent<Player>(player);
  ecs.addComponent<Health>(player);
  ecs.getComponent<Health>(player) = {3.0, 3.0};
  ecs.addComponent<HealthBar>(player);
  ecs.getComponent<HealthBar>(player) = {25.0};
  ecs.addComponent<CollisionBounds>(player);
  ecs.getComponent<CollisionBounds>(player) = {{32, 16}, 0x2 | 0x4};

  // Add score text box.
  Entity score_entity = ecs.newEntity();
  ecs.addComponent<Position>(score_entity);
  ecs.addComponent<RenderCopy>(score_entity);

  updateTextTexture(ecs, sdl, score_entity, 0, SCORE_PREFIX "0");
  ecs.getComponent<Position>(score_entity) = {{sdl.windowDimensions.w / 2, 20}};

  // Set up aliens.

  alien_texture = sdl.loadTexture("art/alien1.png");
  std::vector<Entity> aliens;
  for (int j = 1; j <= alien_rows; ++j) {
    for (int i = 1; i <= alien_columns; ++i) {
      auto alien = ecs.newEntity();
      glm::vec2 pos = {i * 50 + j * 2, j * 60};
      makeStaticSprite(alien, ecs, {{pos.x + j * 20, pos.y}}, alien_texture);
      ecs.addComponent<Alien>(alien);
      ecs.getComponent<Alien>(alien).start_x = pos.x;
      // Off-sets the rows.
      ecs.addComponent<Velocity>(alien);
      ecs.addComponent<CollisionBounds>(alien);

      ecs.addComponent<Health>(alien);
      ecs.getComponent<Health>(alien) = {1.0, 1.0};

      ecs.getComponent<Velocity>(alien) = {{ALIEN_INIT_SPEED, 0}};
      ecs.getComponent<CollisionBounds>(alien) = {{16, 16}, 0x1 | 0x4};
      aliens.push_back(alien);
    }
  }

  // Set up barriers.
  auto barrierTexture = sdl.loadTexture("art/barrier.png");
  for (int i = 0; i < 4; ++i) {
    auto barrier = ecs.newEntity();
    makeStaticSprite(barrier, ecs,
                     {{sdl.windowDimensions.w * (0.5 + i) / 4.0,
                       sdl.windowDimensions.h - 150}},
                     barrierTexture);
    auto &rc = ecs.getComponent<RenderCopy>(barrier);
    constexpr int BARRIER_SCALE = 3;
    rc.w *= BARRIER_SCALE;
    rc.h *= BARRIER_SCALE;

    ecs.addComponent<Health>(barrier);
    ecs.getComponent<Health>(barrier) = {15.0, 15.0};
    ecs.addComponent<HealthBar>(barrier);
    ecs.getComponent<HealthBar>(barrier) = {40.0};
    ecs.addComponent<CollisionBounds>(barrier);
    ecs.getComponent<CollisionBounds>(barrier) = {
        {BARRIER_SCALE * 16, BARRIER_SCALE * 8}, 0x3 | 0x4};
  }

  // Load bullet sprite.
  SDL_Texture *bulletTexture = sdl.loadTexture("art/bullet.png");

  struct VelocitySystem : public System {
    using System::System;
    void run(const std::set<Entity> &entities, Coordinator &ecs) {
      for (auto &e : entities) {
        auto &[pos] = ecs.getComponent<Position>(e);
        const auto &[vel] = ecs.getComponent<Velocity>(e);
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
        auto &[velocity] = ecs.getComponent<Velocity>(e);
        if (keyboardState[SDL_SCANCODE_LEFT]) {
          velocity.x -= 0.2;
        } else if (keyboardState[SDL_SCANCODE_RIGHT]) {
          velocity.x += 0.2;
        } else {
          velocity.x *= 0.9;
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
    int initial_n_aliens;
    void run(const std::set<Entity> &entities, Coordinator &ecs) {
      for (auto &e : entities) {
        auto &[pos] = ecs.getComponent<Position>(e);
        auto &[vel] = ecs.getComponent<Velocity>(e);
        const auto &[start_x] = ecs.getComponent<Alien>(e);
        if (pos.x < start_x) {
          pos.y += ALIEN_DROP_DISTANCE;
          vel.x = alien_speed;
        } else if (pos.x > start_x + ALIEN_SHUFFLE_DISTANCE) {
          pos.y += ALIEN_DROP_DISTANCE;
          vel.x = -alien_speed;
        }
      }

      const auto current_n_aliens = entities.size();
      if (current_n_aliens == 0) {
        events.push_back(GameEvent::Win);
      }

      alien_speed =
          ALIEN_INIT_SPEED +
          ALIEN_SPEED_INCREMENT * (initial_n_aliens - current_n_aliens);
    }
  } alienMovementSystem(
      componentsSignature(
          {ALIEN_COMPONENT, POSITION_COMPONENT, VELOCITY_COMPONENT}),
      ecs);
  alienMovementSystem.sdl = &sdl;
  alienMovementSystem.initial_n_aliens = alien_rows * alien_columns;

  // A system that simply calls SDL_RenderCopy().
  RenderCopySystem renderCopySystem(
      componentsSignature({POSITION_COMPONENT, RENDERCOPY_COMPONENT}), ecs,
      sdl.renderer);

  struct HealthBarSystem : System {
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
        const auto &[pos] = ecs.getComponent<Position>(e);
        const auto &health = ecs.getComponent<Health>(e);
        const auto &bar = ecs.getComponent<HealthBar>(e);
        empty_bar.y = current_bar.y = pos.y + bar.hover_distance - BAR_HEIGHT;
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
  } healthBarSystem(componentsSignature({HEALTH_COMPONENT, HEALTH_BAR_COMPONENT,
                                         POSITION_COMPONENT}),
                    ecs);
  healthBarSystem.renderer = sdl.renderer;

  struct DeathSystem : System {
    using System::System;

    void run(const std::set<Entity> &entities, Coordinator &ecs) {
      for (auto &e : entities) {
        auto &health = ecs.getComponent<Health>(e);
        if (health.current <= 0.0) {
          ecs.queueDestroyEntity(e);
          if (ecs.hasComponent<Player>(e)) {
            events.push_back(GameEvent::GameOver);
          }
          if (ecs.hasComponent<Alien>(e)) {
            events.push_back(GameEvent::Scored);
          }
        }
      }
    }
  } deathSystem(componentsSignature({HEALTH_COMPONENT}), ecs);

  struct EnemyShootingSystem : System {
    using System::System;
    SDL_Texture *enemyBullet;
    std::random_device rd;
    std::mt19937 gen;
    std::binomial_distribution<> firing;
    int nextFire = 0;
    void run(const std::set<Entity> &entities, Coordinator &ecs) {
      for (auto &e : entities) {
        // Generate a binomially distributed random number indicating how many
        // aliens to go along before firing.
        if (nextFire <= 0) {
          makeBullet(ecs, ecs.getComponent<Position>(e), {{0, 6}}, enemyBullet,
                     {{2, 4}, 0x2});
          nextFire = firing(gen);
        } else {
          nextFire -= 1;
        }
      }
    }
  } enemyShootingSystem(
      componentsSignature({ALIEN_COMPONENT, POSITION_COMPONENT}), ecs);
  enemyShootingSystem.firing = std::binomial_distribution<>(3000);
  enemyShootingSystem.gen = std::mt19937(enemyShootingSystem.rd());
  enemyShootingSystem.enemyBullet = sdl.loadTexture("art/enemy-bullet.png");

  struct CollisionSystem : System {
    using System::System;
    SDL_Renderer *renderer;
    void run(const std::set<Entity> &entities, Coordinator &ecs) {
      for (auto &a : entities) {
        const auto &aPos = ecs.getComponent<Position>(a);
        const auto &aBounds = ecs.getComponent<CollisionBounds>(a);
        auto &aHealth = ecs.getComponent<Health>(a);
        for (auto &b : entities) {
          if (b == a) {
            break;
          }

          const auto &bBounds = ecs.getComponent<CollisionBounds>(b);
          const auto &bPos = ecs.getComponent<Position>(b);
          if ((rectangleIntersection(aBounds.rectangle(aPos),
                                     bBounds.rectangle(bPos))) &&
              ((aBounds.layer & bBounds.layer) != LayerMask{0})) {
            aHealth.current -= 1.0;
            ecs.getComponent<Health>(b).current -= 1.0;
            Mix_PlayChannel(-1, sound_explosion, 0);
            if ((aBounds.layer & bBounds.layer & LayerMask{0x4}) !=
                LayerMask{0}) {
              events.push_back(GameEvent::GameOver);
            }
          }
        }
      }
    }
  } collisionSystem(componentsSignature({
                        HEALTH_COMPONENT,
                        POSITION_COMPONENT,
                        COLLISION_BOUNDS_COMPONENT,
                    }),
                    ecs);
  collisionSystem.renderer = sdl.renderer;

  struct AlienEncroachmentSystem : System {
    using System::System;
    int border;
    int screen_width;
    SDL_Renderer *renderer;
    void run(const std::set<Entity> &aliens, Coordinator &ecs) {
      SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0x00);
      SDL_RenderDrawLine(renderer, 0, border, screen_width, border);
      for (auto &e : aliens) {
        if (ecs.getComponent<Position>(e).p.y > border) {
          events.push_back(GameEvent::GameOver);
        }
      }
    }
  } alienEncroachmentSystem(
      componentsSignature({ALIEN_COMPONENT, POSITION_COMPONENT}), ecs);
  alienEncroachmentSystem.border = sdl.windowDimensions.h - 80;
  alienEncroachmentSystem.renderer = sdl.renderer;
  alienEncroachmentSystem.screen_width = sdl.windowDimensions.w;

  struct OffscreenSystem : System {
    using System::System;

    Rectangle screen_space;

    void run(const std::set<Entity> &entities, Coordinator &ecs) {
      for (const auto &e : entities) {
        if (not pointInRectangle(screen_space, ecs.getComponent<Position>(e))) {
          ecs.queueDestroyEntity(e);
        }
      }
    }
  } offscreenSystem(componentsSignature({POSITION_COMPONENT}), ecs);
  offscreenSystem.screen_space = {0, 0,
                                  static_cast<float>(sdl.windowDimensions.w),
                                  static_cast<float>(sdl.windowDimensions.h)};

  printf("ECS initialised\n");

  uint64_t last_shot = 0;
  uint32_t player_score = 0;

  bool quit = false;

  while (!quit) {

    auto tick = SDL_GetTicks64();
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch ((SDL_EventType)e.type) {
      case SDL_QUIT:
        return GameEvent::Quit;
        break;
      case SDL_KEYDOWN: {
        switch (e.key.keysym.sym) {
        case SDLK_SPACE:
          // Limit bullet firing to once every N milliseconds, and don't fire on
          // key repeat.
          if (e.key.repeat == 0 && tick > last_shot + 500) {
            makeBullet(ecs, ecs.getComponent<Position>(player), {{0, -8}},
                       bulletTexture, {{2, 4}, 0x1});
            last_shot = tick;
          }
          break;
        }
        break;
      }
      default:
        break;
      }
    }

    runSystem(playerControlSystem, ecs);
    runSystem(alienMovementSystem, ecs);
    runSystem(velocitySystem, ecs);
    runSystem(enemyShootingSystem, ecs);

    // Rendering
    SDL_SetRenderDrawColor(sdl.renderer, 0x00, 0x00, 0x00, 0x00);
    sdl.renderClear();

    runSystem(collisionSystem, ecs);
    runSystem(alienEncroachmentSystem, ecs);

    runSystem(renderCopySystem, ecs);
    runSystem(healthBarSystem, ecs);
    runSystem(offscreenSystem, ecs);

    runSystem(deathSystem, ecs);

    // Process events
    for (const auto &event : events) {
      switch (event) {
      case GameEvent::GameOver:
        ecs.destroyQueued();
        return GameEvent::GameOver;
      case GameEvent::Win:
        return GameEvent::Win;
      case GameEvent::Scored:
        std::cout << "Score!\n";
        player_score += 1;
        updateTextTexture(ecs, sdl, score_entity, 0,
                          SCORE_PREFIX + std::to_string(player_score));
        break;
      case GameEvent::Quit:
        quit = true;
        break;
      }
    }
    events.clear();

    sdl.renderPresent();

    ecs.destroyQueued();

    while (SDL_GetTicks64() < tick + SCREEN_TICKS_PER_FRAME)
      ;
    // SDL_Delay(tick + SCREEN_TICKS_PER_FRAME - SDL_GetTicks64());
  }

  return GameEvent::Quit;
}

int main() {
  SDL::Context sdl(SDL_INIT_VIDEO, "Space Invaders",
                   {SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720},
                   SDL_WINDOW_SHOWN, {"fonts/GroovetasticRegular.ttf"});

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

  sound_explosion = Mix_LoadWAV("sound/explosion.wav");
  if (sound_explosion == nullptr)
    SDL::Error(__FILE__, __LINE__);
  sound_shoot = Mix_LoadWAV("sound/shoot.wav");
  if (sound_shoot == nullptr)
    SDL::Error(__FILE__, __LINE__);

  printf("SDL initialised\n");
  player_texture = sdl.loadTexture("art/player.png");

  GameEvent res = GameEvent::Win;

  int rows = ALIEN_ROWS;
  while (res != GameEvent::Quit) {
    res = gameplay(sdl, rows, ALIEN_COLUMNS);
    std::cout << "restarting\n";
    if (res == GameEvent::Win)
      rows += 1;
  }

  Mix_FreeChunk(sound_explosion);
  Mix_FreeChunk(sound_shoot);
}
