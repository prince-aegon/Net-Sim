#include "ray.h"
#include <stdio.h>
#include <time.h>
#include <stdlib.h>

const int screenWidth = 800;
const int screenHeight = 450;

const int vertical_sep = 50;
const int horizontal_sep = 75;

const int lineGap = 10;

const int total_UE = 4;
const int total_BTS = 3;
const int trail_bits = 4;

BTS ListBTS[total_BTS];
Cell state[total_UE][trail_bits];

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

float getConnectionRate()
{
    int total = 0;
    for (int i = 0; i < sizeof(ListBTS) / sizeof(ListBTS[0]); i++)
    {
        total += ListBTS[i].number_of_UE;
    }
    return (total * 100) / (sizeof(state) / sizeof(state[0]));
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

void drawBTS()
{
    for (int i = 0; i < total_BTS; i++)
        DrawText(TextFormat("BTS%d", i), cell_to_pixel(ListBTS[i].loc).pointX + 10, cell_to_pixel(ListBTS[i].loc).pointY + 10, 8, ListBTS[i].loc.color);
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
    for (int i = 0; i < 3; i++)
    {
        int lhsX = (ListBTS[i].loc.cellX - curr.cellX) * (ListBTS[i].loc.cellX - curr.cellX);
        int lhsY = (ListBTS[i].loc.cellY - curr.cellY) * (ListBTS[i].loc.cellY - curr.cellY);
        int rhs = ListBTS[i].radius;
        if (lhsX + lhsY <= rhs)
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
    SetTargetFPS(20);

    int cdire[total_UE];

    char *placeholder1 = "Network-Sim";

    // Cell state[4];
    Color stateColor[trail_bits] = {WHITE,
                                    LIGHTGRAY,
                                    GRAY,
                                    DARKGRAY};
    int startLoc[total_UE][2] = {{30, 30}, {10, 10}, {40, 40}, {20, 20}};

    for (int i = 0; i < sizeof(state) / sizeof(state[0]); i++)
    {
        for (int j = 0; j < sizeof(state[0]) / sizeof(state[0][0]); j++)
        {
            state[i][j].cellX = startLoc[i][0];
            state[i][j].cellY = startLoc[i][1];
            state[i][j].color = stateColor[j];
            state[i][j].connection = -1;
        }
    }

    // NEED TO IMPROVE THIS

    int BTS_Data[total_BTS][4] = {{15, 15, 75, 0}, {35, 20, 120, 0}, {50, 10, 90, 0}};
    Color BTS_Color[total_BTS] = {BLUE, GREEN, RED};

    for (int i = 0; i < sizeof(BTS_Data) / sizeof(BTS_Data[0]); i++)
    {
        for (int j = 0; j < 4; j++)
        {
            ListBTS[i].loc.cellX = BTS_Data[i][0];
            ListBTS[i].loc.cellY = BTS_Data[i][1];
            ListBTS[i].radius = BTS_Data[i][2];
            ListBTS[i].number_of_UE = BTS_Data[i][3];
            ListBTS[i].loc.color = BTS_Color[i];
        }
    }

    while (!WindowShouldClose())
    {

        BeginDrawing();

        ClearBackground(BLACK);

        DrawText(placeholder1, 10, 10, 5, WHITE);

        DrawText("Number of UE's", 10, 20, 5, WHITE);
        DrawText(TextFormat("%d", ListBTS[0].number_of_UE), 10, 35, 10, BLUE);
        DrawText(TextFormat("%d", ListBTS[1].number_of_UE), 10, 45, 10, GREEN);
        DrawText(TextFormat("%d", ListBTS[2].number_of_UE), 10, 55, 10, RED);
        DrawText(TextFormat("Connection Rate : %0.f", getConnectionRate()), screenWidth / 2, 10, 10, WHITE);

        drawBTS();
        createGrid();

        for (int i = 0; i < total_UE; i++)
        {
            struct pair_int_int ue_to_bts;
            ue_to_bts = nearestBTS(state[i][0]);
            if (ue_to_bts.y > -1)
            {
                state[i][0].connection = ue_to_bts.y;
                state[i][0].color = ListBTS[ue_to_bts.y].loc.color;
            }
            else
            {
                state[i][0].connection = -1;
                state[i][0].color = WHITE;
            }
        }
        for (int i = 0; i < total_BTS; i++)
        {
            ListBTS[i].number_of_UE = 0;
        }
        for (int i = 0; i < sizeof(state) / sizeof(state[0]); i++)
        {
            if (state[i][0].connection > -1)
            {
                ListBTS[state[i][0].connection].number_of_UE++;
            }
        }

        for (int i = 0; i < sizeof(state) / sizeof(state[0]); i++)
        {
            for (int j = 0; j < trail_bits; j++)
            {
                mark(state[i][j]);
            }
        }

        srand(time(NULL));

        for (int i = 0; i < sizeof(state) / sizeof(state[0]); i++)
            cdire[i] = newDirection(state[i][0], cdire[i]);

        for (int i = 0; i < sizeof(state) / sizeof(state[0]); i++)
        {
            for (int j = trail_bits - 1; j > 0; j--)
            {
                Color temp = state[i][j].color;
                state[i][j] = state[i][j - 1];
                state[i][j].color = temp;
            }
        }
        for (int i = 0; i < sizeof(state) / sizeof(state[0]); i++)
        {
            state[i][0] = update(state[i][0], cdire[i]);
        }

        for (int i = 0; i < total_BTS; i++)
        {
            mark(ListBTS[i].loc);
            initBTS(ListBTS[i]);
        }

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
