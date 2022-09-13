//
// Created by aidan on 13/09/22.
//

#ifndef GAME_GAME_EVENT_HPP
#define GAME_GAME_EVENT_HPP

#include "alien_movement_system.hpp"
#include "components.hpp"
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
#include <ranges>
#include <string>
#include <tecs.hpp>
#include <thread>
#include <tuple>
#include <vector>
enum class GameEvent {
  GameOver,
  Quit, // Called when player closes window.
  Scored,
  Win,
  Progress, // Go to the next scene
};
#endif // GAME_GAME_EVENT_HPP
