#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <omp.h>

#define SCREEN_WIDTH 1000
#define SCREEN_HEIGHT 800
#define MAX_ITER 500
#define MIN_ITER 50

typedef struct {
    double real;
    double imag;
} Complex;

typedef struct {
    double centerReal;
    double centerImag;
    double zoom;
    int maxIter;
} View;

static Color mandelbrotColor(int iter, int maxIter) {
    if (iter >= maxIter) {
        return (Color) {10, 10, 18, 255};
    }
    float t = (float)iter / maxIter;
    unsigned char r = (unsigned char)(9.0f * (1.0f - t) * t * t * t * 255.0f);
    unsigned char g = (unsigned char)(15.0f * (1.0f - t) * (1.0f - t) * t * t * 255.0f);
    unsigned char b = (unsigned char)(8.5f * (1.0f - t) * (1.0f - t) * (1.0f - t) * t * 255.0f);
    return (Color){r, g, b, 255};
}

static Color mandelbrotPixel(double cReal, double cImag, int maxIter) {
    double zReal = 0.0;
    double zImag = 0.0;
    int iter = 0;

        // Main cardioid
    double x = cReal - 0.25;
    double q = x * x + cImag * cImag;
    if (q * (q + x) <= 0.25 * cImag * cImag) {
        return mandelbrotColor(maxIter, maxIter);
    }

    // Period-2 bulb
    double bulbX = cReal + 1.0;
    if (bulbX * bulbX + cImag * cImag <= 0.0625) {
        return mandelbrotColor(maxIter, maxIter);
    }

    while (zReal * zReal + zImag * zImag <= 4.0 && iter < maxIter) {
        double nextReal = zReal * zReal - zImag * zImag + cReal;
        double nextImag = 2.0 * zReal * zImag + cImag;
        zReal = nextReal;
        zImag = nextImag;
        iter++;
    }

    return mandelbrotColor(iter, maxIter);
}

static void renderFractal(Color *pixels, const View *view) {
    #pragma omp parallel for schedule(dynamic, 4)
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            double cReal = view->centerReal + (x - SCREEN_WIDTH / 2.0) * view->zoom;
            double cImag = view->centerImag + (y - SCREEN_HEIGHT / 2.0) * view->zoom;
            pixels[y * SCREEN_WIDTH + x] = mandelbrotPixel(cReal, cImag, view->maxIter);
        }
    }
}

static void zoomAt(View *view, Vector2 mouse, double zoomFactor)
{
    //finding the complex point currently under the cursor.
    double anchorReal = view->centerReal + (mouse.x - SCREEN_WIDTH / 2.0) * view->zoom;
    double anchorImag = view->centerImag + (SCREEN_HEIGHT / 2.0 - mouse.y) * view->zoom;

    view->zoom /= zoomFactor;

    //adjusting the center of the view
    view->centerReal = anchorReal - (mouse.x - SCREEN_WIDTH / 2.0) * view->zoom;
    view->centerImag = anchorImag - (SCREEN_HEIGHT / 2.0 - mouse.y) * view->zoom;
}

int main(void) {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "Mandelbrot Explorer");
    SetTargetFPS(60);

    View view = { -0.5, 0.0, 3.5/SCREEN_WIDTH, MAX_ITER };

    Color *pixels = malloc(sizeof(Color) * SCREEN_WIDTH * SCREEN_HEIGHT);
    if (pixels == NULL) {
        fprintf(stderr, "Could not allocate the pixel buffer.\n");
        CloseWindow();
        return 1;
    }

    renderFractal(pixels, &view);

    Image image = {
        .data = pixels,
        .width = SCREEN_WIDTH,
        .height = SCREEN_HEIGHT,
        .mipmaps = 1,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
    };

    Texture2D texture = LoadTextureFromImage(image);
    if (texture.id == 0) {
        fprintf(stderr, "Could not create the Raylib texture.\n");
        free(pixels);
        CloseWindow();
        return 1;
    }

    bool needsRender = false;
    bool wasDragging = false;
    Vector2 previousMouse = GetMousePosition();

    while (!WindowShouldClose()) {
        Vector2 mouse = GetMousePosition();
        float wheel = GetMouseWheelMove();

        if (wheel != 0.0f) {
            double zoomFactor = wheel > 0.0f ? 1.08 : 1.0 / 1.08;
            zoomAt(&view, mouse, zoomFactor);
            needsRender = true;
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            if (wasDragging) {
                float dx = mouse.x - previousMouse.x;
                float dy = mouse.y - previousMouse.y;

                view.centerReal -= dx * view.zoom;
                view.centerImag += dy * view.zoom;
                needsRender = true;
            }

            wasDragging = true;
        } else {
            wasDragging = false;
        }

        previousMouse = mouse;

        if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) {
            view.centerReal -= 300.0 * GetFrameTime() * view.zoom;
            needsRender = true;
        }
        if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) {
            view.centerReal += 300.0 * GetFrameTime() * view.zoom;
            needsRender = true;
        }
        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W)) {
            view.centerImag += 300.0 * GetFrameTime() * view.zoom;
            needsRender = true;
        }
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S)) {
            view.centerImag -= 300.0 * GetFrameTime() * view.zoom;
            needsRender = true;
        }

        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) {
            if (view.maxIter < MAX_ITER) {
                view.maxIter += 50;
                needsRender = true;
            }
        }
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) {
            if (view.maxIter > MIN_ITER) {
                view.maxIter -= 50;
                needsRender = true;
            }
        }

        if (IsKeyPressed(KEY_R)) {
            view.centerReal = -0.5;
            view.centerImag = 0.0;
            view.zoom = 3.5 / SCREEN_WIDTH;
            view.maxIter = 250;
            needsRender = true;
        }

        if (needsRender) {
            renderFractal(pixels, &view);
            UpdateTexture(texture, pixels);
            needsRender = false;
        }

        if (IsKeyPressed(KEY_P)) {
            TakeScreenshot("mandelbrot.png");
        }

        BeginDrawing();
        ClearBackground(BLACK);
        DrawTexture(texture, 0, 0, WHITE);

        DrawRectangle(10, 10, 440, 112, (Color){ 0, 0, 0, 190 });
        DrawText("Mandelbrot Explorer", 20, 18, 22, RAYWHITE);
        /*DrawText("Scroll: zoom at cursor | Drag: pan", 20, 48, 16, LIGHTGRAY);
        DrawText("Arrows/WASD: move | +/-: iterations | R: reset", 20, 70, 16, LIGHTGRAY);
        DrawText("P: save screenshot | Esc: quit", 20, 92, 16, LIGHTGRAY);

        DrawText(TextFormat("Iterations: %d", view.maxIter),
                 SCREEN_WIDTH - 190, 14, 18, RAYWHITE);*/

        EndDrawing();
    }
 
    UnloadTexture(texture);
    free(pixels);
    CloseWindow();

    return 0;
}