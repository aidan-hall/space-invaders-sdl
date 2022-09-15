#include "alien_movement_system.hpp"
#include "components.hpp"
#include "game_event.hpp"
#include "rectangle.hpp"
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
#include <string>
#include <tecs.hpp>
#include <thread>
#include <tuple>
#include <vector>

using namespace Tecs;

using LayerMask = std::bitset<8>;

struct CollisionBounds {
  glm::vec2 spacing;
  LayerMask layer;
  [[nodiscard]] inline Rectangle rectangle(const Position &pos) const {
    return {pos.p.x - spacing.x, pos.p.y - spacing.y, spacing.x * 2,
            spacing.y * 2};
  }
  [[nodiscard]] inline SDL_Rect sdl_rectangle(const Position &pos) const {
    Rectangle box = rectangle(pos);
    SDL_Rect sdl_rectangle = {static_cast<int>(box.x), static_cast<int>(box.y),
                              static_cast<int>(box.w), static_cast<int>(box.h)};
    return sdl_rectangle;
  }
};

// Sounds
Mix_Chunk *sound_shoot = nullptr;
Mix_Chunk *sound_explosion = nullptr;
SDL_Texture *player_texture = nullptr;

void makeStaticSprite(Entity entity, Coordinator &ecs, Position initPos,
                      SDL_Texture *texture, int w, int h) {
  ecs.addComponent<Position>(entity);
  ecs.addComponent<RenderCopy>(entity);

  ecs.getComponent<Position>(entity) = initPos;

  auto &render_copy = ecs.getComponent<RenderCopy>(entity);
  render_copy.texture = texture;
  render_copy.w = w;
  render_copy.h = h;
}

void makeAnimatedSprite(Entity entity, Coordinator &ecs, Position initPos,
                        SDL_Texture *texture, Animation animation) {
  ecs.addComponent<Animation>(entity);
  ecs.addComponent<Position>(entity);
  ecs.addComponent<RenderCopy>(entity);

  ecs.getComponent<Position>(entity) = initPos;

  auto &animation_component = ecs.getComponent<Animation>(entity);
  // TODO: Actually *copy* the Animation.
  animation_component = animation;
  auto &render_copy = ecs.getComponent<RenderCopy>(entity);
  render_copy.texture = texture;
  render_copy.w = animation.src_rect.w;
  render_copy.h = animation.src_rect.h;
}

std::vector<GameEvent> events;

Entity makeBullet(Coordinator &ecs, Position initPos, Velocity initVel,
                  SDL_Texture *texture, const CollisionBounds &bounds,
                  int animation_steps) {
  Mix_PlayChannel(-1, sound_shoot, 0);
  auto bullet = ecs.newEntity();
  makeAnimatedSprite(bullet, ecs, initPos, texture,
                     {{0, 0, 4, 8}, 0, animation_steps, 5, 0});

  ecs.addComponent<Velocity>(bullet);
  ecs.getComponent<Velocity>(bullet) = {initVel};
  ecs.addComponent<Health>(bullet);
  ecs.getComponent<Health>(bullet) = {1.0, 1.0};
  ecs.addComponent<CollisionBounds>(bullet);
  ecs.getComponent<CollisionBounds>(bullet) = bounds;

  return bullet;
}

struct AlienEncroachmentSystem : System {
  int border;
  int screen_width;
  SDL_Renderer *renderer;
  AlienEncroachmentSystem(const Tecs::Signature &sig, Tecs::Coordinator &coord,
                          const SDL::Context &sdl)
      : System(sig, coord), border{sdl.windowDimensions.h - 80},
        screen_width{sdl.windowDimensions.w}, renderer{sdl.renderer} {}
  void run(const std::set<Entity> &aliens, Coordinator &ecs) override {
    SDL_SetRenderDrawColor(renderer, 0xFF, 0x00, 0x00, 0x00);
    SDL_RenderDrawLine(renderer, 0, border, screen_width, border);
    for (const auto &e : aliens) {
      if (ecs.getComponent<Position>(e).p.y > border) {
        events.push_back(GameEvent::GameOver);
      }
    }
  }
};
struct DeathSystem : System {
  using System::System;

  void run(const std::set<Entity> &entities, Coordinator &ecs) override {
    for (const auto &e : entities) {
      const auto &health = ecs.getComponent<Health>(e);
      if (health.current <= 0.0) {
        ecs.queueDestroyEntity(e);
        Mix_PlayChannel(-1, sound_explosion, 0);

        if (ecs.hasComponent<Player>(e)) {
          events.push_back(GameEvent::GameOver);
        }
        if (ecs.hasComponent<Alien>(e)) {
          events.push_back(GameEvent::Scored);
        }
      }
    }
  }
};

constexpr int ALIEN_ROWS = 4;
constexpr int ALIEN_COLUMNS = 20;

struct CollisionSystem : System {
  using System::System;
  void run(const std::set<Entity> &entities, Coordinator &ecs) override {
    for (const auto &a : entities) {
      const auto &aPos = ecs.getComponent<Position>(a);
      const auto &aBounds = ecs.getComponent<CollisionBounds>(a);
      auto &aHealth = ecs.getComponent<Health>(a);
      for (const auto &b : entities) {
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
          if ((aBounds.layer & bBounds.layer & LayerMask{0x4}) !=
              LayerMask{0}) {
            events.push_back(GameEvent::GameOver);
          }
        }
      }
    }
  }
};
struct HealthBarSystem : System {
  SDL_Renderer *renderer;

  HealthBarSystem(Signature sig, Coordinator &coord, SDL_Renderer *renderer)
      : System(sig, coord), renderer{renderer} {}

  void run(const std::set<Entity> &entities, Coordinator &ecs) override {
    constexpr int BAR_HEIGHT = 5;
    constexpr int BAR_LENGTH = 30;
    SDL_Rect current_bar;
    SDL_Rect empty_bar;
    empty_bar.w = BAR_LENGTH;
    empty_bar.h = BAR_HEIGHT;
    current_bar.h = BAR_HEIGHT;
    for (const auto &e : entities) {
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
};

struct PlayerControlSystem : System {
  const int window_width;

  PlayerControlSystem(const Signature &sig, Coordinator &coord,
                      const int windowWidth)
      : System(sig, coord), window_width(windowWidth) {}
  void run(const std::set<Entity> &entities, Coordinator &ecs) override {
    const auto *const keyboardState = SDL_GetKeyboardState(nullptr);
    constexpr float PLAYER_MAX_SPEED = 5.0;
    constexpr float PLAYER_MAX_SPEED_SQUARED =
        PLAYER_MAX_SPEED * PLAYER_MAX_SPEED;
    for (const auto &e : entities) {
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
      if (pos.x > window_width - WINDOW_MARGIN) {
        pos.x = window_width - WINDOW_MARGIN;
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
};
struct VelocitySystem : public System {
  using System::System;
  void run(const std::set<Entity> &entities, Coordinator &ecs) override {
    for (const auto &e : entities) {
      auto &[pos] = ecs.getComponent<Position>(e);
      const auto &[vel] = ecs.getComponent<Velocity>(e);
      pos += vel;
    }
  }
};
struct OffscreenSystem : System {
  Rectangle screen_space;

  OffscreenSystem(const Tecs::Signature &sig, Tecs::Coordinator &coord,
                  SDL_Rect &screen_dimensions)
      : System(sig, coord), screen_space{
                                0, 0, static_cast<float>(screen_dimensions.w),
                                static_cast<float>(screen_dimensions.h)} {}

  void run(const std::set<Entity> &entities, Coordinator &ecs) override {
    for (const auto &e : entities) {
      if (not pointInRectangle(screen_space, ecs.getComponent<Position>(e))) {
        ecs.queueDestroyEntity(e);
      }
    }
  }
};

// Return the input rectangle, with its centre where its top left corner was.
SDL_Rect centered_rectangle(SDL_Rect rect) {
  return {rect.x - rect.w / 2, rect.y - rect.h / 2, rect.w, rect.h};
}

struct StaticSpriteRenderingSystem : System {
  SDL_Renderer *renderer = nullptr;

  void run(const std::set<Entity> &entities, Coordinator &ecs) override {
    for (const auto &e : entities) {
      const auto &[pos] = ecs.getComponent<Position>(e);
      const auto &render_copy = ecs.getComponent<RenderCopy>(e);
      const SDL_Rect renderRect = centered_rectangle(
          {(int)pos.x, (int)pos.y, render_copy.w, render_copy.h});
      SDL_RenderCopy(renderer, render_copy.texture, nullptr, &renderRect);
    }
  }

  StaticSpriteRenderingSystem(const Signature &sig, Coordinator &coord,
                              SDL_Renderer *theRenderer)
      : System(sig, coord) {
    this->renderer = theRenderer;
  }
};

struct AnimatedSpriteRenderingSystem : System {
  SDL_Renderer *renderer = nullptr;

  // TODO: Animation must be added before RenderCopy, so the static renderer
  // doesn't get it.
  AnimatedSpriteRenderingSystem(const Signature &sig, Coordinator &coord,
                                SDL_Renderer *renderer)
      : System(sig, coord), renderer(renderer) {}

  void run(const std::set<Entity> &entities, Coordinator &ecs) override {
    for (const auto &e : entities) {
      auto &animation = ecs.getComponent<Animation>(e);

      // Update animation step & step frames as appropriate.
      if (animation.current_step_frames >= animation.frames_per_step) {
        animation.step++;
        animation.current_step_frames = 0;

        if (animation.step >= animation.n_steps) {
          animation.step = 0;
        }

        // Assuming sprites are in a horizontal line and of uniform size,
        // only the x component of the source rectangle needs updating.
        animation.src_rect.x = animation.step * animation.src_rect.w;
      }

      const auto &pos = ecs.getComponent<Position>(e).p;
      const auto &render_copy = ecs.getComponent<RenderCopy>(e);
      const SDL_Rect renderRect = centered_rectangle(
          {(int)pos.x, (int)pos.y, render_copy.w, render_copy.h});

      SDL_RenderCopy(renderer, render_copy.texture, &animation.src_rect,
                     &renderRect);

      animation.current_step_frames++;
    }
  }
};

struct EnemyShootingSystem : System {

  EnemyShootingSystem(Signature sig, Coordinator &coord,
                      SDL_Texture *enemy_bullet)
      : System(sig, coord),
        enemyBullet{enemy_bullet}, gen{std::mt19937(std::random_device()())},
        firing{std::binomial_distribution<>(3000)} {}
  SDL_Texture *enemyBullet{};
  std::random_device rd;
  std::mt19937 gen;
  std::binomial_distribution<> firing;
  int nextFire = 0;
  void run(const std::set<Entity> &entities, Coordinator &ecs) override {
    for (const auto &e : entities) {
      // Generate a binomially distributed random number indicating how many
      // aliens to go along before firing.
      if (nextFire <= 0) {
        makeBullet(ecs, ecs.getComponent<Position>(e), {{0, 6}}, enemyBullet,
                   {{2, 4}, 0x2}, 6);
        nextFire = firing(gen);
      } else {
        nextFire -= 1;
      }
    }
  }
};

// Framerate.

constexpr int32_t SCREEN_FPS = 60;
constexpr int32_t SCREEN_TICKS_PER_FRAME = 1000 / SCREEN_FPS;

// Scores
uint32_t player_score = 0;
std::array<uint32_t, 5> high_scores = {0, 0, 0, 0, 0};

#define SCORE_PREFIX "Score: "

void updateTextTexture(Coordinator &ecs, SDL::Context &sdl, Entity score_entity,
                       uint32_t font_idx, const std::string &text) {
  auto text_texture =
      sdl.loadFromRenderedText(text, {255, 255, 255, 0}, font_idx);
  auto &render_copy = ecs.getComponent<RenderCopy>(score_entity);
  render_copy.texture = text_texture.texture;
  render_copy.w = text_texture.w;
  render_copy.h = text_texture.h;
}

GameEvent title_screen(SDL::Context &sdl, const std::string &subtitle) {
  auto makeTextBox = [&sdl](const std::string &text,
                            int x) -> std::pair<SDL_Texture *, SDL_Rect> {
    SDL::TextTexture textTexture =
        sdl.loadFromRenderedText(text, {255, 255, 255, 0}, 0);
    const SDL_Rect rect = {(sdl.windowDimensions.w - textTexture.w) / 2, x,
                           textTexture.w, textTexture.h};

    return {textTexture.texture, rect};
  };

  auto drawTextBox = [&sdl](const std::pair<SDL_Texture *, SDL_Rect> &textBox) {
    const auto &[texture, box] = textBox;
    SDL_RenderCopy(sdl.renderer, texture, nullptr, &box);
  };

  auto title = makeTextBox("Space Invaders", 200);
  auto controls = makeTextBox("Press Space to begin", 250);
  auto subtitle_box = makeTextBox(subtitle, 300);

  std::string high_scores_string =
      std::accumulate(std::next(high_scores.begin()), high_scores.end(),
                      "High Scores: " + std::to_string(high_scores.front()),
                      [](const std::string &text, uint32_t score) {
                        return text + ", " + std::to_string(score);
                      });

  auto highscore = makeTextBox(high_scores_string, 350);

  bool finished = false;
  while (!finished) {
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      switch ((SDL_EventType)event.type) {
      case SDL_QUIT:
        return GameEvent::Quit;
        break;
      case SDL_KEYDOWN:
        if (event.key.keysym.sym == SDLK_SPACE) {
          finished = true;
        }
        break;
      default:
        break;
      }
    }

    sdl.setRenderDrawColor(0x000000);
    sdl.renderClear();

    drawTextBox(title);
    drawTextBox(subtitle_box);
    drawTextBox(controls);
    drawTextBox(highscore);
    sdl.renderPresent();
  }

  return GameEvent::Progress;
}

GameEvent gameplay(SDL::Context &sdl, const int alien_rows,
                   const int alien_columns, const int level) {

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
  const auto ANIMATION_COMPONENT = ecs.registerComponent<Animation>();

  // Set up player.
  auto player = ecs.newEntity();
  makeStaticSprite(player, ecs,
                   {{sdl.windowDimensions.w / 2, sdl.windowDimensions.h - 40}},
                   player_texture, 64, 32);

  ecs.addComponent<Velocity>(player);
  ecs.addComponent<Player>(player);
  ecs.addComponent<Health>(player);
  ecs.getComponent<Health>(player) = {3.0, 3.0};
  ecs.addComponent<HealthBar>(player);
  ecs.getComponent<HealthBar>(player) = {25.0};
  ecs.addComponent<CollisionBounds>(player);
  ecs.getComponent<CollisionBounds>(player) = {{32, 16}, 0x2 | 0x4};

  // Add level text box.
  Entity level_text_entity = ecs.newEntity();
  ecs.addComponent<RenderCopy>(level_text_entity);
  updateTextTexture(ecs, sdl, level_text_entity, 0,
                    "Level: " + std::to_string(level));
  ecs.addComponent<Position>(level_text_entity);
  {
    const auto &render_copy = ecs.getComponent<RenderCopy>(level_text_entity);
    ecs.getComponent<Position>(level_text_entity) = {
        {render_copy.w / 2 + 5, render_copy.h / 2 + 5}};
  }

  // Add score text box.
  Entity score_entity = ecs.newEntity();
  ecs.addComponent<Position>(score_entity);
  ecs.addComponent<RenderCopy>(score_entity);

  updateTextTexture(ecs, sdl, score_entity, 0, SCORE_PREFIX "0");
  ecs.getComponent<Position>(score_entity) = {{sdl.windowDimensions.w / 2, 20}};

  // Set up aliens.

  std::vector<SDL_Texture *> alien_textures =
    sdl.loadTextures({"art/alien1.png", "art/alien2.png", "art/alien3.png"});
  std::vector<Entity> aliens;
  Animation alien_animation = {{0, 0, 32, 32}, 0, 2, 20, 0};
  for (int j = 1; j <= alien_rows; ++j) {
    for (int i = 1; i <= alien_columns; ++i) {
      auto alien = ecs.newEntity();
      glm::vec2 pos = {i * 50 + j * 2, j * 60};
      makeAnimatedSprite(alien, ecs, {{pos.x + j * 20, pos.y}},
                         alien_textures[alien_textures.size() * (j-1) / alien_rows],
                         alien_animation);
      alien_animation.step =
          (alien_animation.step + 1) % alien_animation.n_steps;
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
    alien_animation.frames_per_step -= 3;
  }

  // Set up barriers.
  auto *barrierTexture = sdl.loadTexture("art/barrier.png");
  for (int i = 0; i < 4; ++i) {
    auto barrier = ecs.newEntity();
    constexpr int BARRIER_SCALE = 3;
    makeStaticSprite(barrier, ecs,
                     {{sdl.windowDimensions.w * (0.5 + i) / 4.0,
                       sdl.windowDimensions.h - 150}},
                     barrierTexture, 32 * BARRIER_SCALE, 16 * BARRIER_SCALE);

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

  VelocitySystem velocitySystem(
      componentsSignature({VELOCITY_COMPONENT, POSITION_COMPONENT}), ecs);

  PlayerControlSystem playerControlSystem(
      componentsSignature(
          {PLAYER_COMPONENT, VELOCITY_COMPONENT, POSITION_COMPONENT}),
      ecs, sdl.windowDimensions.w);

  AlienMovementSystem alienMovementSystem(
      componentsSignature(
          {ALIEN_COMPONENT, POSITION_COMPONENT, VELOCITY_COMPONENT}),
      ecs, alien_rows * alien_columns, ALIEN_INIT_SPEED, events);

  // A system that simply calls SDL_RenderCopy().
  StaticSpriteRenderingSystem staticSpriteRenderingSystem(
      componentsSignature({POSITION_COMPONENT, RENDERCOPY_COMPONENT},
                          {ANIMATION_COMPONENT}),
      ecs, sdl.renderer);

  AnimatedSpriteRenderingSystem animatedSpriteRenderingSystem(
      componentsSignature(
          {POSITION_COMPONENT, RENDERCOPY_COMPONENT, ANIMATION_COMPONENT}),
      ecs, sdl.renderer);

  HealthBarSystem healthBarSystem(
      componentsSignature(
          {HEALTH_COMPONENT, HEALTH_BAR_COMPONENT, POSITION_COMPONENT}),
      ecs, sdl.renderer);

  DeathSystem deathSystem(componentsSignature({HEALTH_COMPONENT}), ecs);

  EnemyShootingSystem enemyShootingSystem(
      componentsSignature({ALIEN_COMPONENT, POSITION_COMPONENT}), ecs,
      sdl.loadTexture("art/enemy-bullet.png"));

  CollisionSystem collisionSystem(componentsSignature({
                                      HEALTH_COMPONENT,
                                      POSITION_COMPONENT,
                                      COLLISION_BOUNDS_COMPONENT,
                                  }),
                                  ecs);

  AlienEncroachmentSystem alienEncroachmentSystem(
      componentsSignature({ALIEN_COMPONENT, POSITION_COMPONENT}), ecs, sdl);

  OffscreenSystem offscreenSystem(componentsSignature({POSITION_COMPONENT}),
                                  ecs, sdl.windowDimensions);

  printf("ECS initialised\n");

  auto last_shot = std::chrono::high_resolution_clock::now();
  player_score = 0;

  bool quit = false;

  while (!quit) {

    auto tick = std::chrono::high_resolution_clock::now();
    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
      switch ((SDL_EventType)e.type) {
      case SDL_QUIT:
        return GameEvent::Quit;
        break;
      case SDL_KEYDOWN: {
        switch (e.key.keysym.sym) {
        case SDLK_SPACE:
          // Limit bullet firing to once every N milliseconds, and don't fire on
          // key repeat.
          if (e.key.repeat == 0 &&
              tick > last_shot + std::chrono::milliseconds(500)) {
            makeBullet(ecs, ecs.getComponent<Position>(player), {{0, -8}},
                       bulletTexture, {{2, 4}, 0x1}, 2);
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

    runSystem(staticSpriteRenderingSystem, ecs);
    runSystem(animatedSpriteRenderingSystem, ecs);
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
      case GameEvent::Progress:
        break;
      }
    }
    events.clear();

    sdl.renderPresent();

    ecs.destroyQueued();

    std::this_thread::sleep_until(
        tick + std::chrono::milliseconds(SCREEN_TICKS_PER_FRAME));
  }

  return GameEvent::Quit;
}

int main() {
  SDL::Context sdl(SDL_INIT_VIDEO, "Space Invaders",
                   {SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720},
                   SDL_WINDOW_SHOWN, {"fonts/GroovetasticRegular.ttf"});

  const std::string preferences_path =
      SDL_GetPrefPath("AidanGames", "Space Invaders SDL");
  const auto high_scores_filename = preferences_path + "/high_scores";
  {
    FILE *high_scores_file = std::fopen(high_scores_filename.c_str(), "rb");
    if (high_scores_file != nullptr) {
      std::ignore =
          std::fread(high_scores.data(), sizeof(decltype(high_scores[0])),
                     high_scores.size(), high_scores_file);
      std::ignore = std::fclose(high_scores_file);
    }
  }

  SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

  sound_explosion = Mix_LoadWAV("sound/explosion.wav");
  if (sound_explosion == nullptr) {
    throw SDL::Error(__FILE__, __LINE__);
  }
  sound_shoot = Mix_LoadWAV("sound/shoot.wav");
  if (sound_shoot == nullptr) {
    throw SDL::Error(__FILE__, __LINE__);
  }

  printf("SDL initialised\n");
  player_texture = sdl.loadTexture("art/player.png");

  GameEvent res = title_screen(sdl, "Space to shoot; Arrow Keys to move.");

  int level = 1;

  while (res != GameEvent::Quit) {
    // Level starts at 1 but ALIEN_ROWS should apply to level 1.
    res = gameplay(sdl, ALIEN_ROWS - 1 + level, ALIEN_COLUMNS, level);
    if (player_score > high_scores.back()) {
      high_scores.back() = player_score;
      std::ranges::sort(high_scores, std::greater<>());
    }
    if (res == GameEvent::Win) {
      res = title_screen(sdl, "Finished Level: " + std::to_string(level));
      level += 1;
    } else if (res == GameEvent::GameOver) {
      res = title_screen(sdl, "Game Over");
      level = 1;
      player_score = 0;
    }
  }

  // Attempt to write the high scores back to the file.
  {
    FILE *high_scores_file = std::fopen(high_scores_filename.c_str(), "wb");
    if (high_scores_file != nullptr) {
      std::ignore =
          std::fwrite(high_scores.data(), sizeof(decltype(high_scores[0])),
                      high_scores.size(), high_scores_file);
      std::ignore = std::fclose(high_scores_file);
    }
  }

  Mix_FreeChunk(sound_explosion);
  Mix_FreeChunk(sound_shoot);
}
