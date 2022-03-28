// gcc Video.c -o Video.so --shared -fpic -lSDL2

#include <SDL2/SDL.h>

static SDL_Renderer *renderer;
static SDL_Window *window;

void
Video_Init(double* xres, double* yres)
{
    SDL_Init(SDL_INIT_VIDEO);
    SDL_CreateWindowAndRenderer(*xres, *yres, 0, &window, &renderer);
}

void
Video_Clear(void)
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
    SDL_RenderClear(renderer);
}

void
Video_Draw(double* r, double* g, double* b, double* x, double* y)
{
    SDL_SetRenderDrawColor(renderer, *r, *g, *b, 255);
    SDL_RenderDrawPoint(renderer, *x, *y);
}

void
Video_Present(void)
{
    SDL_RenderPresent(renderer);
}

void
Video_Delay(double* ms)
{
    SDL_Delay(*ms);
}

void
Video_Destroy(void)
{
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
