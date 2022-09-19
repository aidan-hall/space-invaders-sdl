//
// Created by aidan on 13/09/22.
//

#include "alien_movement_system.hpp"
#include "components.hpp"
#include "rectangle.hpp"
#include "sdl.hpp"
#include <vector>

using namespace std::chrono_literals;

constexpr float ALIEN_SHUFFLE_DISTANCE = 200.0;
constexpr float ALIEN_DROP_DISTANCE = 10.0;
constexpr float ALIEN_SPEED_INCREMENT = 1.8;
constexpr Duration MAX_STEP_DURATION = 500ms;
constexpr Duration MIN_STEP_DURATION = 50ms;

void AlienMovementSystem::run(const std::set<Entity> &entities,
                              Coordinator &ecs, const Duration delta) {
  std::ignore = delta;
  for (const auto &e : entities) {
    auto &[pos] = ecs.getComponent<Position>(e);
    auto &[vel] = ecs.getComponent<Velocity>(e);
    const auto &start_x = ecs.getComponent<Alien>(e).start_x;
    if (pos.x < start_x) {
      pos.y += ALIEN_DROP_DISTANCE;
      vel.x = alien_speed;
    } else if (pos.x > start_x + ALIEN_SHUFFLE_DISTANCE) {
      pos.y += ALIEN_DROP_DISTANCE;
      vel.x = -alien_speed;
    }
    ecs.getComponent<Animation>(e).step_time =
        MIN_STEP_DURATION + (MAX_STEP_DURATION - MIN_STEP_DURATION) *
      ((float)current_n_aliens / (float)initial_n_aliens);
  }

  current_n_aliens = entities.size();
  if (current_n_aliens == 0) {
    events.push_back(GameEvent::Win);
  }

  alien_speed = base_alien_speed +
                ALIEN_SPEED_INCREMENT * (initial_n_aliens - current_n_aliens);
}
