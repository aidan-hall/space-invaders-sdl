#ifndef SDLPP_H
#define SDLPP_H

#include <SDL2/SDL.h>
#include <SDL_image.h>
#include <SDL_log.h>
#include <SDL_ttf.h>
#include <SDL_video.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace SDL {
struct TextTexture {
  SDL_Texture *texture;
  int w;
  int h;
};

struct Context {
  SDL_Window *window = nullptr;
  SDL_Surface *windowSurface = nullptr;
  SDL_Renderer *renderer = nullptr;
  SDL_Rect windowDimensions;
  std::vector<TTF_Font *> fonts;
  std::vector<SDL_Texture *> textures;

  Context(Uint32 initFlags, std::string name, SDL_Rect dimensions,
          Uint32 windowFlags, std::vector<const char *> fontFiles);

  ~Context();

  SDL_Texture *loadTexture(std::string path);
  TextTexture loadFromRenderedText(std::string text, SDL_Color textColor,
                                   size_t fontIdx);
  SDL_Texture *fancyTextureFromSurface(SDL_Surface *surface);
  std::vector<SDL_Texture *> loadTextures(std::vector<std::string> paths);

  // Renderer stuff.
  void setRenderDrawColor(Uint32 hex);
  void renderClear();
  void renderPresent();
};

struct Error : std::runtime_error {
  Error() : runtime_error(SDL_GetError()) {}
  explicit Error(int lineNumber)
      : runtime_error(std::to_string(lineNumber) + ":" +
                      std::string(SDL_GetError())) {}
  Error(const std::string &file, int lineNumber)
      : runtime_error(file + ":" + std::to_string(lineNumber) + ":" +
                      std::string(SDL_GetError())) {}
};

} // namespace SDL
#endif // SDLPP_H
