# Space Invaders SDL

This is a Space Invaders clone written in C++ using SDL 2.
It features an ECS heavily inspired by Austin Morlan's blog post:

https://austinmorlan.com/posts/entity_component_system/

I have recorded a demo of the game in action, since I can hardly
expect you to be able to build it:

https://youtu.be/KyWTbw8M7bE

## Building

To build and run the game, you will need a C++20 compiler, CMake,
SDL2, SDL2_image, SDL2_ttf, SDL2_mixer and (admittedly pointlessly)
glm.

You can then just build as you would with any CMake project.

This will probably only compile on Linux with GCC or Clang, but I
won't stop you from trying to get it working on Windows.


## TODO
* Animations:
  * Animation component (src_rect).
* Alien Death Explosions
* More alien sprites: Cycles for green, + red and blue(?)
* Mothership: Flies along the top; fast; bonus points.