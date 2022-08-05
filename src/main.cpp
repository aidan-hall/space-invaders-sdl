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
  std::cout<< "Hello world!\n";
  Coordinator ecs;
  SystemManager systems;
  const auto VEC2_COMPONENT = ecs.registerComponent<Vec2>();
  const auto BAR_COMPONENT = ecs.registerComponent<Bar>();
  auto physics_system = ecs.registerSystem({VEC2_COMPONENT, BAR_COMPONENT});
  Entity player = ecs.newEntity();
  Entity alien = ecs.newEntity();

  ecs.addComponent<Vec2>(player);
  auto v = ecs.getComponent<Vec2>(player);
  v.x = 5;
  v.y = 4;
}
