#pragma once

#include <functional>

template <typename ComponentType>
void flipImageYAxis(
    std::size_t width, std::size_t height, std::size_t numComponent, ComponentType *pixels)
{
  auto *pFirstLine = pixels;
  auto *pLastLine = pixels + (height - 1) * width * numComponent;

  while (pFirstLine < pLastLine) {
    for (std::size_t x = 0; x < width * numComponent; ++x)
      std::swap(pFirstLine[x], pLastLine[x]);
    pFirstLine += width * numComponent;
    pLastLine -= width * numComponent;
  }
}

void renderToImage(std::size_t width, std::size_t height, std::size_t numComponents,
    unsigned char *outPixels, std::function<void()> render, std::function<void()> computeShadows);
// Setup GL state in order to render in texture, call drawScene() then get the
// texture from the GPU and store it on outPixels[0 : width * height *
// numComponent]. Then restore the previous GL state.
//
// For this to work, drawScene must render on the currently bound
// GL_DRAW_FRAMEBUFFER.
// It means that if drawScene change GL_DRAW_FRAMEBUFFER, in must restore it
// before doing final rendering (for example for deferred rendering,
// GL_DRAW_FRAMEBUFFER must be restored before the shading pass).