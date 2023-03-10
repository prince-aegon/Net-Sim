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
    Cell loc;
    BTS resp[6];
    int numberofBTS;
    int ue_connects;
} MSC;
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
    int connection;
} Cell;

typedef struct
{
    Cell loc;
    int radius;
    int number_of_UE;
} BTS;

typedef struct Timer
{
    double startTime; // Start time (seconds)
    double lifeTime;  // Lifetime (seconds)
} Timer;

struct pair_cell_dir
{
    Cell cell;
    int dir;
};

struct pair_int_int
{
    int x;
    int y;
};

void DrawLine(int startPosX, int startPosY, int endPosX, int endPosY, Color color);
void DrawRectangle(int posX, int posY, int width, int height, Color color);
void InitWindow(int width, int height, const char *title);
void SetTargetFPS(int fps);
bool WindowShouldClose(void);
void ClearBackground(Color color);
void BeginDrawing(void);
void EndDrawing(void);
void DrawText(const char *text, int posX, int posY, int fontSize, Color color);
void DrawCircle(int centerX, int centerY, float radius, Color color);
void StartTimer(Timer *timer, double lifetime);
bool TimerDone(Timer timer);
double GetElapsed(Timer timer);

#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (!FALSE)
#endif

#endif