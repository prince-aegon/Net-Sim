#include "ray.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

const int screenWidth = 800;
const int screenHeight = 450;

const int vertical_sep = 50;
const int horizontal_sep = 75;

const int lineGap = 10;

BTS ListBTS[2];

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

Point cell_to_pixel(Cell cell)
{
    Point ret;
    ret.pointX = cell.cellX * lineGap + horizontal_sep;
    ret.pointY = cell.cellY * lineGap + vertical_sep;

    return ret;
}

struct pair_cell_dir validate(Cell curr)
{
    struct pair_cell_dir validator;

    int valid_dir, flag = 0;

    Point point = cell_to_pixel(curr);
    if (point.pointX <= horizontal_sep)
    {
        curr.cellX = 0;
        valid_dir = 1;
        flag = 1;
    }
    else if (point.pointX >= screenWidth - horizontal_sep - 2 * lineGap)
    {
        curr.cellX = (screenWidth - horizontal_sep - lineGap) / lineGap;
        valid_dir = 3;
        flag = 1;
    }
    if (point.pointY <= vertical_sep)
    {
        curr.cellY = 0;
        valid_dir = 2;
        flag = 1;
    }
    else if (point.pointY >= screenHeight - vertical_sep - 2 * lineGap)
    {
        curr.cellY = (screenHeight - vertical_sep - lineGap) / lineGap;
        valid_dir = 0;
        flag = 1;
    }
    validator.cell.cellX = curr.cellX;
    validator.cell.cellY = curr.cellY;
    if (flag == 0)
    {
        validator.dir = -1;
        return validator;
    }
    validator.dir = valid_dir;
    return validator;
}
void mark(Cell curr)
{
    DrawRectangle(horizontal_sep + lineGap * curr.cellX, vertical_sep + lineGap * curr.cellY, lineGap, lineGap, curr.color);
}

void initBTS(BTS bts)
{
    DrawCircleLines(horizontal_sep + lineGap * bts.loc.cellX + 0.5 * lineGap, vertical_sep + lineGap * bts.loc.cellY + 0.5 * lineGap, bts.radius, bts.loc.color);
}

Cell update(Cell curr, int dir)
{
    if (dir == 0)
        curr.cellY--;
    else if (dir == 1)
        curr.cellX++;
    else if (dir == 2)
        curr.cellY++;
    else
        curr.cellX--;
    return curr;
}

int newDirection(Cell curr, int cDir)
{
    struct pair_cell_dir temp_pair;
    temp_pair = validate(curr);
    if (temp_pair.dir != -1)
        return temp_pair.dir;

    int r = rand();
    int k = r % 4;
    if ((cDir == 0 && k == 2) || (cDir == 2 && k == 0) || (cDir == 1 && k == 3) || (cDir == 3 && k == 1))
        return (k + 7) % 4;
    if (r < RAND_MAX / 8)
    {
        return k;
    }
    else
    {
        return cDir;
    }
}

struct pair_int_int nearestBTS(Cell curr)
{
    struct pair_int_int ret;
    int min_rad_index = -1, min_rad = 5000;
    for (int i = 0; i < 2; i++)
    {
        int lhsX = (ListBTS[i].loc.cellX - curr.cellX) * (ListBTS[i].loc.cellX - curr.cellX);
        int lhsY = (ListBTS[i].loc.cellY - curr.cellY) * (ListBTS[i].loc.cellY - curr.cellY);
        int rhs = ListBTS[i].radius;
        if (lhsX + lhsY < rhs)
        {
            if (lhsX + lhsY < min_rad)
            {
                min_rad = lhsX + lhsY;
                min_rad_index = i;
            }
        }
    }
    ret.x = min_rad;
    ret.y = min_rad_index;
    return ret;
}

int main(void)
{

    InitWindow(screenWidth, screenHeight, "Prince");
    SetTargetFPS(10);

    srand(time(NULL));
    int cDir1 = 1, cDir2 = 1;

    char *placeholder1 = "Network-Game";

    // Cell state[4];
    Color stateColor[4] = {WHITE,
                           LIGHTGRAY,
                           GRAY,
                           DARKGRAY};
    int startLoc[2][2] = {{30, 30}, {10, 10}};
    Cell state[2][4];

    for (int i = 0; i < sizeof(state) / sizeof(state[0]); i++)
    {
        for (int j = 0; j < sizeof(state[0]) / sizeof(state[0][0]); j++)
        {
            state[i][j].cellX = startLoc[i][0];
            state[i][j].cellY = startLoc[i][1];
            state[i][j].color = stateColor[j];
        }
    }

    // NEED TO IMPROVE THIS

    ListBTS[0].loc.cellX = 15;
    ListBTS[0].loc.cellY = 15;
    ListBTS[0].loc.color = BLUE;
    ListBTS[0].radius = 75;

    ListBTS[1].loc.cellX = 35;
    ListBTS[1].loc.cellY = 20;
    ListBTS[1].loc.color = GREEN;
    ListBTS[1].radius = 120;

    while (!WindowShouldClose())
    {

        BeginDrawing();

        ClearBackground(BLACK);

        DrawText(placeholder1, 10, 10, 5, WHITE);

        createGrid();

        for (int i = 0; i < 2; i++)
        {
            struct pair_int_int ue_to_bts;
            ue_to_bts = nearestBTS(state[i][0]);
            if (ue_to_bts.y > -1)
                state[i][0].color = ListBTS[ue_to_bts.y].loc.color;
            else
                state[i][0].color = WHITE;
        }
        /*
        Bug : if we use 2d arrays for implementing multiple ue's below
        the direction remain the same for all.
        */
        for (int i = 0; i < 4; i++)
        {
            mark(state[0][i]);
            mark(state[1][i]);
        }

        cDir1 = newDirection(state[0][0], cDir1);
        cDir2 = newDirection(state[1][0], cDir2);

        for (int i = 3; i > 0; i--)
        {
            Color temp = state[0][i].color;
            state[0][i] = state[0][i - 1];
            state[0][i].color = temp;

            temp = state[1][i].color;
            state[1][i] = state[1][i - 1];
            state[1][i].color = temp;
        }

        state[0][0] = update(state[0][0], cDir1);
        state[1][0] = update(state[1][0], cDir2);

        mark(ListBTS[0].loc);
        initBTS(ListBTS[0]);

        mark(ListBTS[1].loc);
        initBTS(ListBTS[1]);

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
