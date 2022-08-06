#include <iostream>
#include <tecs.hpp>

using namespace Tecs;

struct Vec2 {
  int x;
  int y;
};
struct Bar {
  
};


int main() {
  std::cout << "sizeof(Bar) == " << sizeof(Bar) << std::endl;
  std::cout<< "Hello world!\n";
  Coordinator ecs;
  SystemManager systems;
  const auto VEC2_COMPONENT = ecs.registerComponent<Vec2>();
  const auto BAR_COMPONENT = ecs.registerComponent<Bar>();
  auto physics_system = ecs.registerSystem({VEC2_COMPONENT, BAR_COMPONENT});
  Entity player = ecs.newEntity();
  std::cout<< "Player 1's ID: " << player << std::endl;
  Entity alien = ecs.newEntity();

  std::cout << "Interests of Physics system before & after adding Bar to player:\n";
  ecs.addComponent<Vec2>(player);
  std::cout << ecs.systems.systemInterests[0].size() << std::endl;
  ecs.addComponent<Bar>(player);
  std::cout << ecs.systems.systemInterests[0].size() << std::endl;
  {
    auto& v = ecs.getComponent<Vec2>(player);
    v.x = 5;
    v.y = 4;
  }

  ecs.destroyEntity(player);

  auto player2 = ecs.newEntity();
  std::cout<< "Player 2's ID: " << player2 << std::endl;
  std::cout << "Interests of physics system now: " << ecs.systems.systemInterests[0].size() << std::endl;
  
}
