#include "ray.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

const int screenWidth = 800;
const int screenHeight = 450;

const int vertical_sep = 50;
const int horizontal_sep = 75;

const int lineGap = 10;

void createGrid()
{
    Point points[4];
    for (int i = horizontal_sep; i < screenWidth - horizontal_sep; i += lineGap)
    {
        points[0].pointX = i;
        points[0].pointY = vertical_sep;

        points[1].pointX = i;
        points[1].pointY = screenHeight - vertical_sep - lineGap;

        DrawLine(points[0].pointX, points[0].pointY, points[1].pointX, points[1].pointY, LIGHTGRAY);
    }
    for (int i = vertical_sep; i < screenHeight - vertical_sep; i += lineGap)
    {
        points[2].pointX = horizontal_sep;
        points[2].pointY = i;

        points[3].pointX = screenWidth - horizontal_sep - lineGap;
        points[3].pointY = i;

        DrawLine(points[2].pointX, points[2].pointY, points[3].pointX, points[3].pointY, LIGHTGRAY);
    }
}

void mark(Cell curr, Color color)
{
    DrawRectangle(horizontal_sep + lineGap * curr.cellX, vertical_sep + lineGap * curr.cellY, lineGap, lineGap, color);
}

Cell update(Cell curr, int dir)
{
    if (dir == 0)
        curr.cellY++;
    else if (dir == 1)
        curr.cellX++;
    else if (dir == 2)
        curr.cellY--;
    else
        curr.cellX--;
    return curr;
}

int newDirection(int cDir)
{
    int r = rand() % 4;
    if (cDir == 0 && r == 2 || cDir == 2 && r == 0 || cDir == 1 && r == 3 || cDir == 3 && r == 1)
        return (r + 7) % 4;
    return r;
}

int main(void)
{

    InitWindow(screenWidth, screenHeight, "Prince");
    SetTargetFPS(1);

    Color color = BLACK;
    Cell curr, prev, prev2;
    curr.cellX = prev.cellX = prev2.cellX = 1;
    curr.cellY = prev.cellY = prev2.cellY = 10;

    srand(time(NULL));
    int cDir = 1;
    while (!WindowShouldClose())
    {

        BeginDrawing();
        ClearBackground(BLACK);

        createGrid();

        // for (int i = 0; i < ; i++)
        // {
        mark(curr, WHITE);
        mark(prev, GRAY);
        mark(prev2, LIGHTGRAY);
        // if (i != 1)
        //     curr = update(curr, cDir);
        // }
        cDir = newDirection(cDir);
        prev2 = prev;
        prev = curr;
        curr = update(curr, cDir);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
