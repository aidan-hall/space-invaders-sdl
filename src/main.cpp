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
  const auto FOO_COMPONENT = ecs.registerComponent<Vec2>();
  const auto BAR_COMPONENT = ecs.registerComponent<Bar>();
  Entity player = ecs.newEntity();
  Entity alien = ecs.newEntity();
  // std::cout
  //   << player << ' '
  //   << alien  << ' '
  //   << std::endl
  //   << FOO_COMPONENT << ' ' << ecs.componentId<Vec2>()
  //   << BAR_COMPONENT << ' ' << ecs.componentId<Bar>()
  //   << std::endl
  //   ;

  auto v = ecs.getComponent<Vec2>(player);
  v.x = 5;
  v.y = 4;
}
