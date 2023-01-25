#include "raylib.h"

const int screenWidth = 800;
const int screenHeight = 450;

const int vertical_sep = 50;
const int horizontal_sep = 75;

typedef struct
{
    int pointX;
    int pointY;
} Point;

void createGrid()
{
    Point startv, endv, starth, endh;
    Point points[4];
    for (int i = horizontal_sep; i < screenWidth - horizontal_sep; i += 10)
    {
        points[0].pointX = i;
        points[0].pointY = vertical_sep;

        points[1].pointX = i;
        points[1].pointY = screenHeight - vertical_sep - 10;

        DrawLine(points[0].pointX, points[0].pointY, points[1].pointX, points[1].pointY, LIGHTGRAY);
    }
    for (int i = vertical_sep; i < screenHeight - vertical_sep; i += 10)
    {
        points[2].pointX = horizontal_sep;
        points[2].pointY = i;

        points[3].pointX = screenWidth - horizontal_sep - 10;
        points[3].pointY = i;

        DrawLine(points[2].pointX, points[2].pointY, points[3].pointX, points[3].pointY, LIGHTGRAY);
    }
}

int main(void)
{

    InitWindow(screenWidth, screenHeight, "Prince");
    SetTargetFPS(60);

    Color color = BLACK;

    while (!WindowShouldClose())
    {
        BeginDrawing();

        createGrid();

        // mark(int x, int y);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
