#ifndef GAME_GAME_EVENT_HPP
#define GAME_GAME_EVENT_HPP
enum class GameEvent {
  GameOver,
  Quit, // Called when player closes window.
  Scored,
  Win,
  Progress, // Go to the next scene
};
#endif // GAME_GAME_EVENT_HPP
