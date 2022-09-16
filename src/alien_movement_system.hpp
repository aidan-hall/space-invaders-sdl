#ifndef GAME_ALIEN_MOVEMENT_SYSTEM_HPP
#define GAME_ALIEN_MOVEMENT_SYSTEM_HPP

#include "components.hpp"
#include "game_event.hpp"
#include "tecs.hpp"
using namespace Tecs;
// haha

constexpr float ALIEN_INIT_SPEED = 0.2;

struct AlienMovementSystem : System {
  int initial_n_aliens;
  const float base_alien_speed;
  float alien_speed;
  size_t current_n_aliens;
  std::vector<GameEvent> &events;
  AlienMovementSystem(const Signature &sig, Coordinator &coord,
                      int initialNAliens, float alienSpeed,
                      std::vector<GameEvent> &events)
      : System(sig, coord), initial_n_aliens(initialNAliens),
        base_alien_speed(alienSpeed), alien_speed(alienSpeed),
        current_n_aliens(initialNAliens), events(events) {}
  void run(const std::set<Entity> &entities, Coordinator &ecs,
           const Duration delta);
};

#endif // GAME_ALIEN_MOVEMENT_SYSTEM_HPP
