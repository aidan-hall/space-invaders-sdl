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
#include <glm/glm.hpp>
#include <iostream>
#include <random>
#include <string>
#include <tecs.hpp>
#include <thread>
#include <tuple>
#include <vector>

using namespace Tecs;
using namespace std::literals::chrono_literals;

using LayerMask = std::bitset<8>;

struct CollisionBounds {
  glm::vec2 spacing{};
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

// Framerate.

constexpr Duration FRAME_DURATION = 1.0s / 60;

// Sounds
Mix_Chunk *sound_shoot = nullptr;
Mix_Chunk *sound_explosion = nullptr;
Mix_Chunk *sound_hit = nullptr;
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
  animation_component = animation;
  auto &render_copy = ecs.getComponent<RenderCopy>(entity);
  render_copy.texture = texture;
  render_copy.w = animation.src_rect.w;
  render_copy.h = animation.src_rect.h;
}

std::vector<GameEvent> events;

Entity makeMothership(Coordinator &ecs, SDL_Texture *texture) {
  const Animation animation{
      {
          0,
          0,
          64,
          32,
      },
      0,
      3,
      Duration(1.0s / 12),
  };
  Entity mothership = ecs.newEntity();

  ecs.addComponent<Mothership>(mothership);

  makeAnimatedSprite(mothership, ecs, {{0, 80}}, texture, animation);
  ecs.addComponent<Velocity>(mothership);
  ecs.getComponent<Velocity>(mothership) = {{100, 0}};
  auto &render_copy = ecs.getComponent<RenderCopy>(mothership);
  constexpr auto MOTHERSHIP_SCALE = 2;
  render_copy.w *= MOTHERSHIP_SCALE;
  render_copy.h *= MOTHERSHIP_SCALE;

  ecs.addComponent<Health>(mothership);
  ecs.getComponent<Health>(mothership) = {
      4,
      4,
  };
  ecs.addComponent<HealthBar>(mothership);
  ecs.getComponent<HealthBar>(mothership) = {
      16.0,
  };
  ecs.addComponent<CollisionBounds>(mothership);
  ecs.getComponent<CollisionBounds>(mothership) = {
      {render_copy.w / 2, render_copy.h / 2},
      LayerMask{0x8},
  };
  return mothership;
}

Entity makeExplosion(Coordinator &ecs, Position initPos, SDL_Texture *texture) {
  auto explosion = ecs.newEntity();
  {
    constexpr Animation explosion_animation{
        {
            0,
            0,
            32,
            32,
        },
        0,
        4,
        5 * FRAME_DURATION,
    };
    makeAnimatedSprite(explosion, ecs, initPos, texture, explosion_animation);
    ecs.addComponent<LifeTime>(explosion);
    ecs.getComponent<LifeTime>(explosion) = {{}, explosion_animation.length()};
  }
  return explosion;
}

Entity makeBullet(Coordinator &ecs, Position initPos, Velocity initVel,
                  SDL_Texture *texture, const CollisionBounds &bounds,
                  int animation_steps) {
  Mix_PlayChannel(-1, sound_shoot, 0);
  auto bullet = ecs.newEntity();
  {
    using namespace std::chrono;
    Animation bullet_animation = {
        {
            0,
            0,
            4,
            8,
        },
        0,
        animation_steps,
        5 * FRAME_DURATION,
    };
    makeAnimatedSprite(bullet, ecs, initPos, texture, bullet_animation);
  }
  ecs.addComponent<Velocity>(bullet);
  ecs.getComponent<Velocity>(bullet) = {initVel};
  ecs.addComponent<Health>(bullet);
  ecs.getComponent<Health>(bullet) = {1.0, 1.0};
  ecs.addComponent<CollisionBounds>(bullet);
  ecs.getComponent<CollisionBounds>(bullet) = bounds;

  return bullet;
}

struct LifeTimeSystem : System {
  LifeTimeSystem(const Signature &sig, Coordinator &coord)
      : System(sig, coord) {}

  void run(const std::set<Entity> &entities, Coordinator &coord,
           const Duration delta) override {
    for (const auto &e : entities) {
      auto &lifetime = coord.getComponent<LifeTime>(e);
      lifetime.lived += delta;
      if (lifetime.lived >= lifetime.lifespan) {
        coord.queueDestroyEntity(e);
      }
    }
  }
};
struct AlienEncroachmentSystem : System {
  int border;
  AlienEncroachmentSystem(const Tecs::Signature &sig, Tecs::Coordinator &coord,
                          const int window_height)
      : System(sig, coord), border{window_height - 80} {}
  void run(const std::set<Entity> &aliens, Coordinator &ecs,
           const Duration delta) override {
    std::ignore = delta;
    for (const auto &e : aliens) {
      if (ecs.getComponent<Position>(e).p.y > border) {
        events.push_back(GameEvent::GameOver);
      }
    }
  }
};
struct DeathSystem : System {
  SDL_Texture *explosion_texture;

  const std::vector<Entity> barriers;

  DeathSystem(const Signature &sig, Coordinator &coord,
              SDL_Texture *explosionTexture,
              const std::vector<Entity> the_barriers)
      : System(sig, coord), explosion_texture(explosionTexture),
        barriers(the_barriers) {}

  void run(const std::set<Entity> &entities, Coordinator &ecs,
           const Duration delta) override {
    std::ignore = delta;
    for (const auto &e : entities) {
      const auto &health = ecs.getComponent<Health>(e);
      if (health.current <= 0.0) {
        ecs.queueDestroyEntity(e);

        bool explosive = true;
        if (ecs.hasComponent<Player>(e)) {
          events.push_back(GameEvent::GameOver);
        } else if (ecs.hasComponent<Alien>(e)) {
          events.push_back(GameEvent::Scored);
        } else if (ecs.hasComponent<Mothership>(e)) {
          events.push_back(GameEvent::KilledMothership);
        } else {
          explosive = false;
        }

        if (explosive) {
          makeExplosion(ecs, ecs.getComponent<Position>(e), explosion_texture);
        }
      }
    }
  }
};

constexpr int ALIEN_ROWS = 4;
constexpr int ALIEN_COLUMNS = 20;

struct CollisionSystem : System {
  using System::System;
  void run(const std::set<Entity> &entities, Coordinator &ecs,
           const Duration delta) override {
    std::ignore = delta;
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
          Health &bHealth = ecs.getComponent<Health>(b);
          bHealth.current -= 1.0;

          if (ecs.hasComponent<Player>(a) || ecs.hasComponent<Player>(b)) {
            Mix_PlayChannel(-1, sound_explosion, 0);
            std::this_thread::sleep_for(10 * FRAME_DURATION);
          } else if (aHealth.current > 0 || bHealth.current > 0) {
            Mix_PlayChannel(-1, sound_hit, 0);
          } else {
            Mix_PlayChannel(-1, sound_explosion, 0);
          }

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

  void run(const std::set<Entity> &entities, Coordinator &ecs,
           const Duration delta) override {
    std::ignore = delta;
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
  static constexpr Duration FIRE_FREQUENCY = 500ms;
  Duration shot_delta{FIRE_FREQUENCY};
  SDL_Texture *bullet_texture;

  PlayerControlSystem(const Signature &sig, Coordinator &coord,
                      const int windowWidth, SDL_Texture *bullet_texture)
      : System(sig, coord), window_width(windowWidth),
        bullet_texture(bullet_texture) {}
  void run(const std::set<Entity> &entities, Coordinator &ecs,
           const Duration delta) override {
    const auto *const keyboardState = SDL_GetKeyboardState(nullptr);
    constexpr float PLAYER_MAX_SPEED = 300;
    for (const auto &e : entities) {
      auto &[velocity] = ecs.getComponent<Velocity>(e);

      if (keyboardState[SDL_SCANCODE_LEFT]) {
        velocity.x = -PLAYER_MAX_SPEED;
      } else if (keyboardState[SDL_SCANCODE_RIGHT]) {
        velocity.x = PLAYER_MAX_SPEED;
      } else {
        velocity.x = 0;
      }

      auto &pos = ecs.getComponent<Position>(e);

      // Handle firing.
      shot_delta += delta;

      if (keyboardState[SDL_SCANCODE_SPACE] && shot_delta >= FIRE_FREQUENCY) {
        makeBullet(ecs, pos,
                   {
                       {0, -480},
                   },
                   bullet_texture,
                   {
                       {2, 4},
                       0x1 | 0x8,
                   },
                   2);
        shot_delta = Duration::zero();
      }

      constexpr int WINDOW_MARGIN = 50;
      if (pos.p.x > (float)window_width - WINDOW_MARGIN) {
        pos.p.x = (float)window_width - WINDOW_MARGIN;
        velocity.x = 0;
      } else if (pos.p.x < WINDOW_MARGIN) {
        pos.p.x = WINDOW_MARGIN;
        velocity.x = 0;
      }
    }
  }
};
struct VelocitySystem : public System {
  using System::System;
  void run(const std::set<Entity> &entities, Coordinator &ecs,
           const Duration delta) override {
    for (const auto &e : entities) {
      auto &[pos] = ecs.getComponent<Position>(e);
      const auto &[vel] = ecs.getComponent<Velocity>(e);

      pos += vel * (float)delta.count();
    }
  }
};
struct OffscreenSystem : System {
  Rectangle screen_space;
  Entity mothership = -1;

  OffscreenSystem(const Tecs::Signature &sig, Tecs::Coordinator &coord,
                  SDL_Rect &screen_dimensions)
      : System(sig, coord), screen_space{
                                0, 0, static_cast<float>(screen_dimensions.w),
                                static_cast<float>(screen_dimensions.h)} {}

  void run(const std::set<Entity> &entities, Coordinator &ecs,
           const Duration delta) override {
    std::ignore = delta;
    for (const auto &e : entities) {
      const auto bounds = ecs.getComponent<CollisionBounds>(e);
      if (not rectangleIntersection(
              screen_space, bounds.rectangle(ecs.getComponent<Position>(e)))) {
        ecs.queueDestroyEntity(e);
        if (e == mothership) {
          events.push_back(GameEvent::MothershipLeft);
        }
      }
    }
  }
};

// Return the input rectangle, with its centre where its top left corner was.
constexpr SDL_Rect centered_rectangle(SDL_Rect rect) {
  return {rect.x - rect.w / 2, rect.y - rect.h / 2, rect.w, rect.h};
}

struct StaticSpriteRenderingSystem : System {
  SDL_Renderer *renderer = nullptr;

  void run(const std::set<Entity> &entities, Coordinator &ecs,
           const Duration delta) override {
    std::ignore = delta;
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

  // Animation must be added before RenderCopy, so the static renderer doesn't
  // get it.
  AnimatedSpriteRenderingSystem(const Signature &sig, Coordinator &coord,
                                SDL_Renderer *renderer)
      : System(sig, coord), renderer(renderer) {}

  void run(const std::set<Entity> &entities, Coordinator &ecs,
           const Duration delta) override {
    for (const auto &e : entities) {
      auto &animation = ecs.getComponent<Animation>(e);

      // Update animation step & step frames as appropriate.
      if (animation.current_step_time >= animation.step_time) {
        animation.step++;
        animation.current_step_time -= animation.step_time;

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

      animation.current_step_time += delta;
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
  void run(const std::set<Entity> &entities, Coordinator &ecs,
           const Duration delta) override {
    std::ignore = delta;
    for (const auto &e : entities) {
      // Generate a binomially distributed random number indicating how many
      // aliens to go along before firing.
      if (nextFire <= 0) {
        makeBullet(ecs, ecs.getComponent<Position>(e), {{0, 360}}, enemyBullet,
                   {{2, 4}, 0x2}, 6);
        nextFire = firing(gen);
      } else {
        nextFire -= 1;
      }
    }
  }
};

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

constexpr int32_t PLAYER_WIDTH = 96;
constexpr int32_t PLAYER_HEIGHT = 48;

GameEvent title_screen(SDL::Context &sdl, const std::string &subtitle,
                       SDL_Texture *player_texture) {
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

  const SDL_Rect player_pos = centered_rectangle({sdl.windowDimensions.w / 2,
                                                  sdl.windowDimensions.h - 40,
                                                  PLAYER_WIDTH, PLAYER_HEIGHT});

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
    SDL_RenderCopy(sdl.renderer, player_texture, nullptr, &player_pos);
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
  const auto LIFETIME_COMPONENT = ecs.registerComponent<LifeTime>();
  ecs.registerComponent<Mothership>();

  // Set up player.
  auto player = ecs.newEntity();
  makeStaticSprite(player, ecs,
                   {{sdl.windowDimensions.w / 2, sdl.windowDimensions.h - 40}},
                   player_texture, PLAYER_WIDTH, PLAYER_HEIGHT);

  ecs.addComponent<Velocity>(player);
  ecs.addComponent<Player>(player);
  ecs.addComponent<Health>(player);
  ecs.getComponent<Health>(player) = {3.0, 3.0};
  ecs.addComponent<HealthBar>(player);
  ecs.getComponent<HealthBar>(player) = {35.0};
  ecs.addComponent<CollisionBounds>(player);
  ecs.getComponent<CollisionBounds>(player) = {
      {PLAYER_WIDTH / 2, PLAYER_HEIGHT / 2}, 0x2 | 0x4};

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
  Animation alien_animation = {
      {
          0,
          0,
          32,
          32,
      },
      0,
      2,
      Duration(0.5s),
      {},
  };

  std::random_device rd;
  std::default_random_engine eng(rd());
  std::uniform_real_distribution<Duration::rep> step_frames_rng(
      FRAME_DURATION.count(), alien_animation.step_time.count());
  for (int j = 1; j <= alien_rows; ++j) {
    for (int i = 1; i <= alien_columns; ++i) {
      auto alien = ecs.newEntity();
      glm::vec2 pos = {i * 50 + j * 2, j * 60};
      alien_animation.current_step_time = Duration(step_frames_rng(eng));
      makeAnimatedSprite(
          alien, ecs, {{pos.x + j * 20, pos.y}},
          alien_textures[alien_textures.size() * (j - 1) / alien_rows],
          alien_animation);
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
  auto *barrierTexture = sdl.loadTexture("art/barrier.png");
  std::vector<Entity> barriers;
  for (int i = 0; i < 4; ++i) {
    auto barrier = ecs.newEntity();
    barriers.push_back(barrier);
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

  VelocitySystem velocitySystem(
      componentsSignature({VELOCITY_COMPONENT, POSITION_COMPONENT}), ecs);

  PlayerControlSystem playerControlSystem(
      componentsSignature(
          {PLAYER_COMPONENT, VELOCITY_COMPONENT, POSITION_COMPONENT}),
      ecs, sdl.windowDimensions.w, sdl.loadTexture("art/bullet.png"));

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

  DeathSystem deathSystem(componentsSignature({HEALTH_COMPONENT}), ecs,
                          sdl.loadTexture("art/explosion.png"), barriers);

  LifeTimeSystem lifeTimeSystem(componentsSignature({LIFETIME_COMPONENT}), ecs);

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
      componentsSignature({ALIEN_COMPONENT, POSITION_COMPONENT}), ecs,
      sdl.windowDimensions.h);

  OffscreenSystem offscreenSystem(
      componentsSignature({POSITION_COMPONENT, COLLISION_BOUNDS_COMPONENT}),
      ecs, sdl.windowDimensions);

  printf("ECS initialised\n");

  bool quit = false;

  auto previous_tick = TimePoint::clock::now() - FRAME_DURATION;

  auto mothership_rng_engine =
      std::default_random_engine(std::random_device()());
  std::uniform_int_distribution<int> mothership_rng(0, 256);

  auto *mothership_texture = sdl.loadTexture("art/mothership.png");

  bool mothership_active = false;

  while (!quit) {

    auto tick = TimePoint::clock::now();

    SDL_Event e;
    while (SDL_PollEvent(&e) != 0) {
      switch ((SDL_EventType)e.type) {
      case SDL_QUIT:
        return GameEvent::Quit;
        break;
      default:
        break;
      }
    }

    const auto delta = tick - previous_tick;

    if (not mothership_active) {
      if (mothership_rng(mothership_rng_engine) == 0) {
        offscreenSystem.mothership = makeMothership(ecs, mothership_texture);
        mothership_active = true;
      }
    }

    runSystem(playerControlSystem, ecs, delta);
    runSystem(alienMovementSystem, ecs, delta);
    runSystem(enemyShootingSystem, ecs, delta);
    runSystem(velocitySystem, ecs, delta);

    runSystem(collisionSystem, ecs, delta);
    runSystem(alienEncroachmentSystem, ecs, delta);

    // Systems specifically for destroying entities.
    runSystem(lifeTimeSystem, ecs, delta);
    runSystem(offscreenSystem, ecs, delta);
    runSystem(deathSystem, ecs, delta);

    // Prevent destroyed entities from rendering for an extra frame.
    ecs.destroyQueued();

    SDL_SetRenderDrawColor(sdl.renderer, 0x00, 0x00, 0x00, 0x00);
    sdl.renderClear();

    // Border
    SDL_SetRenderDrawColor(sdl.renderer, 0xFF, 0x00, 0x00, 0x00);
    SDL_RenderDrawLine(sdl.renderer, 0, alienEncroachmentSystem.border,
                       sdl.windowDimensions.w, alienEncroachmentSystem.border);

    runSystem(staticSpriteRenderingSystem, ecs, delta);
    runSystem(animatedSpriteRenderingSystem, ecs, delta);
    runSystem(healthBarSystem, ecs, delta);
    sdl.renderPresent();
    // Process events
    for (const auto &event : events) {
      switch (event) {
      case GameEvent::GameOver:
        ecs.destroyQueued();
        return GameEvent::GameOver;
      case GameEvent::Win:
        return GameEvent::Win;
      case GameEvent::MothershipLeft:
        mothership_active = false;
        break;
      case GameEvent::KilledMothership:
        mothership_active = false;
        // Hacky way of giving 10 points for a mothership.
        player_score += 9;
        [[fallthrough]];
      case GameEvent::Scored:
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

    previous_tick = tick;
    std::this_thread::sleep_until(tick + FRAME_DURATION);
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
  sound_hit = Mix_LoadWAV("sound/hit.wav");
  if (sound_hit == nullptr) {
    throw SDL::Error(__FILE__, __LINE__);
  }

  printf("SDL initialised\n");
  player_texture = sdl.loadTexture("art/player.png");

  GameEvent res =
      title_screen(sdl, "Space to shoot; Arrow Keys to move.", player_texture);

  int level = 1;

  while (res != GameEvent::Quit) {
    // Level starts at 1 but ALIEN_ROWS should apply to level 1.
    res = gameplay(sdl, ALIEN_ROWS - 1 + level, ALIEN_COLUMNS, level);
    if (player_score > high_scores.back() && res != GameEvent::Win) {
      high_scores.back() = player_score;
      std::ranges::sort(high_scores, std::greater<>());
    }

    if (res == GameEvent::Win) {
      res = title_screen(sdl,
                         "Finished Level: " + std::to_string(level) +
                             ", Score: " + std::to_string(player_score),
                         player_texture);
      level += 1;
    } else if (res == GameEvent::GameOver) {
      res = title_screen(sdl, "Game Over", player_texture);
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
