#include "sdl.hpp"
#include <SDL.h>
#include <SDL_hints.h>
#include <SDL_image.h>
#include <SDL_joystick.h>
#include <SDL_log.h>
#include <SDL_mixer.h>
#include <SDL_pixels.h>
#include <SDL_rect.h>
#include <SDL_render.h>
#include <SDL_surface.h>
#include <SDL_ttf.h>
#include <SDL_version.h>
#include <SDL_video.h>
#include <cstddef>
#include <string>

namespace SDL {
Context::Context(Uint32 initFlags, std::string name, SDL_Rect dimensions,
                 Uint32 windowFlags, std::vector<const char *> fontFiles = {}) {
  // Load SDL
  if (SDL_Init(initFlags) < 0)
    throw Error(__FILE__, __LINE__);

  // Load SDL_image
  const int MY_IMG_FLAGS = IMG_INIT_PNG | IMG_INIT_JPG;
  if ((IMG_Init(MY_IMG_FLAGS) & MY_IMG_FLAGS) == 0)
    throw Error(__FILE__, __LINE__);

  // Load SDL_mixer
  const int MY_MIXER_FLAGS = MIX_INIT_OGG;
  if (Mix_Init(MY_MIXER_FLAGS) < 0)
    throw Error(__FILE__, __LINE__);

  if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    throw Error(__FILE__, __LINE__);

  // Create window
  windowDimensions = dimensions;
  window = SDL_CreateWindow(name.c_str(), dimensions.x, dimensions.y,
                            dimensions.w, dimensions.h, windowFlags);

  if (window == nullptr)
    throw Error(__FILE__, __LINE__);

  // Create renderer for window
  renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
  if (renderer == nullptr)
    throw Error(__FILE__, __LINE__);

  // TTF
  if (TTF_Init() == -1)
    throw Error(__FILE__, __LINE__);

  // Load Font
  for (auto &fontFile : fontFiles) {
    TTF_Font *font = TTF_OpenFont(fontFile, 28);
    if (font == nullptr)
      throw Error(__FILE__, __LINE__);

    fonts.push_back(font);
  }

  // Texture Filtering
  if (!SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1")) {
    SDL_LogWarn(SDL_LOG_CATEGORY_RENDER, "Couldn't enable linear filtering.\n");
  }

  // Joystick
  SDL_Joystick *gameController = nullptr;
  if (SDL_NumJoysticks() < 1) {
    SDL_LogError(SDL_LOG_CATEGORY_INPUT, "No joystick found!\n");
  } else {
    gameController = SDL_JoystickOpen(0);
    if (gameController == nullptr) {
      SDL_LogError(SDL_LOG_CATEGORY_INPUT, "Couldn't open joystick 0.\n");
    }
  }
}

Context::~Context() {
  for (auto font : this->fonts) {
    TTF_CloseFont(font);
  }

  for (auto texture : textures) {
    SDL_DestroyTexture(texture);
  }
  textures.clear();

  SDL_DestroyWindow(window);
  SDL_DestroyRenderer(renderer);
  TTF_Quit();
  Mix_Quit();
  IMG_Quit();
  // Called when game ends anyway; had been hanging. Should resolve?!
  // SDL_Quit();
}

SDL_Texture *Context::fancyTextureFromSurface(SDL_Surface *surface) {
  if (surface == nullptr)
    throw Error(__FILE__, __LINE__);

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
  if (texture == nullptr)
    throw Error(__FILE__, __LINE__);

  return texture;
}

TextTexture Context::loadFromRenderedText(std::string text, SDL_Color color,
                                          size_t fontIdx) {
  auto surface =
      TTF_RenderText_Solid(this->fonts.at(fontIdx), text.c_str(), color);
  auto texture = fancyTextureFromSurface(surface);
  TextTexture t = {texture, surface->w, surface->h};
  SDL_FreeSurface(surface);
  return t;
}

SDL_Texture *Context::loadTexture(std::string path) {
  SDL_Surface *mediaSurface = IMG_Load(path.c_str());
  SDL_Texture *mediaTexture = fancyTextureFromSurface(mediaSurface);
  SDL_FreeSurface(mediaSurface);
  textures.push_back(mediaTexture);
  return mediaTexture;
}

std::vector<SDL_Texture *>
Context::loadTextures(std::vector<std::string> paths) {
  auto loadingTextures = std::vector<SDL_Texture *>();
  for (auto path : paths) {
    auto t = loadTexture(path.c_str());
    loadingTextures.push_back(t);
    textures.push_back(t);
  }
  return loadingTextures;
}

void Context::setRenderDrawColor(Uint32 hex) {
  SDL_SetRenderDrawColor(renderer, (hex & 0xFF000000) >> 24,
                         (hex & 0x00FF0000) >> 16, (hex & 0x0000FF00) >> 8,
                         (hex & 0x000000FF));
}

void Context::renderClear() { SDL_RenderClear(this->renderer); }
void Context::renderPresent() { SDL_RenderPresent(this->renderer); }
} // namespace SDL
