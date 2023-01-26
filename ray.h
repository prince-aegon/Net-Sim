#ifndef RAY
#define RAY

#include <stdio.h>
#include "raylib.h"

enum Direction
{
    Top,
    Right,
    Down,
    Left
};
typedef struct
{
    int pointX;
    int pointY;
} Point;

typedef struct
{
    int cellX;
    int cellY;
    Color color;
} Cell;

struct pair_cell_dir
{
    Cell cell;
    int dir;
};
void DrawLine(int startPosX, int startPosY, int endPosX, int endPosY, Color color);
void DrawRectangle(int posX, int posY, int width, int height, Color color);
void InitWindow(int width, int height, const char *title);
void SetTargetFPS(int fps);
bool WindowShouldClose(void);
void ClearBackground(Color color);
void BeginDrawing(void);
void EndDrawing(void);

#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif

#endif