#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <sqlite3.h>
#include "raylib.h"

#define MAX_INPUT_CHARS 9

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

const int screenWidth = 800;
const int screenHeight = 450;

const int vertical_sep = 50;
const int horizontal_sep = 75;

const int lineGap = 10;

const int total_UE = 4;
const int total_BTS = 3;
const int trail_bits = 4;

int dynamic_UE_count = total_UE;

BTS ListBTS[total_BTS];
Cell state[total_UE][trail_bits];

void StartTimer(Timer *timer, double lifetime)
{
    timer->startTime = GetTime();
    timer->lifeTime = lifetime;
}

bool TimerDone(Timer timer)
{
    return GetTime() - timer.startTime >= timer.lifeTime;
}

double GetElapsed(Timer timer)
{
    return GetTime() - timer.startTime;
}

void CustomLog(int msgType, const char *text, va_list args)
{
    char timeStr[64] = {0};
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", tm_info);
    printf("[%s] ", timeStr);

    switch (msgType)
    {
    case LOG_INFO:
        printf("[INFO] : ");
        break;
    case LOG_ERROR:
        printf("[ERROR]: ");
        break;
    case LOG_WARNING:
        printf("[WARN] : ");
        break;
    case LOG_DEBUG:
        printf("[DEBUG]: ");
        break;
    default:
        break;
    }

    vprintf(text, args);
    printf("\n");
}

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
    return (total * 100) / (dynamic_UE_count);
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
    sqlite3 *DB;
    int exit_stat = 0;
    char *sql = "CREATE TABLE UE("
                "ID INT PRIMARY KEY NOT NULL,"
                "X  INT             NOT NULL,"
                "Y  INT             NOT NULL,"
                "COLOR CHAR(32)             ,"
                "CONNECTION INT             );";

    exit_stat = sqlite3_open("net-sim.db", &DB);

    char *messaggeError;

    exit_stat = sqlite3_exec(DB, sql, NULL, 0, &messaggeError);

    if (exit_stat != SQLITE_OK)
    {
        printf("Error opening db %s \n", sqlite3_errmsg(DB));
        sqlite3_free(messaggeError);
        return -1;
    }
    else
    {
        printf("Table created successfully \n");
    }
    SetTraceLogCallback(CustomLog);

    InitWindow(screenWidth, screenHeight, "Prince");
    SetTargetFPS(10);

    int cdire[total_UE];

    char *placeholder1 = "Network-Sim";

    // Cell state[4];
    Color stateColor[trail_bits] = {WHITE,
                                    LIGHTGRAY,
                                    GRAY,
                                    DARKGRAY};

    // Man data 1
    int startLoc[total_UE][2] = {{30, 30}, {10, 10}, {40, 40}, {20, 20}};
    // printf("%s\n", (dynamic_UE_count));
    // printf("%d\n", (trail_bits));

    for (int i = 0; i < dynamic_UE_count; i++)
    {
        for (int j = 0; j < trail_bits; j++)
        {
            state[i][j].cellX = startLoc[i][0];
            state[i][j].cellY = startLoc[i][1];
            state[i][j].color = stateColor[j];
            state[i][j].connection = -1;
        }
    }

    // NEED TO IMPROVE THIS

    // Man data 2
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

    char inUE[MAX_INPUT_CHARS + 1] = "\0"; // NOTE: One extra space required for null terminator char '\0'
    int letterCount = 0;
    int framesCounter = 0;

    int currScreen = 0;

    // activate when cursor in box
    bool mouseOnText = false;

    // activate when click
    bool mouseClick = false;

    Rectangle textBox = {60, 60, 80, 30};

    while (!WindowShouldClose())
    {

        if (currScreen == 0)
        {
            if (CheckCollisionPointRec(GetMousePosition(), textBox))
                mouseOnText = true;
            else
                mouseOnText = false;

            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_FORWARD) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE))
            {
                Point cursor;
                cursor.pointX = GetMouseX();
                cursor.pointY = GetMouseY();
                if (cursor.pointX >= textBox.x && cursor.pointX <= textBox.x + textBox.width)
                {
                    if (cursor.pointY >= textBox.y && cursor.pointY <= textBox.y + textBox.height)
                    {
                        mouseClick = true;
                    }
                    else
                    {
                        mouseClick = false;
                    }
                }
                else
                {
                    mouseClick = false;
                }
            }

            if (mouseClick)
            {
                // Set the window's cursor to the I-Beam
                SetMouseCursor(MOUSE_CURSOR_IBEAM);

                // Get char pressed (unicode character) on the queue
                int key = GetCharPressed();

                // Check if more characters have been pressed on the same frame
                while (key > 0)
                {
                    // NOTE: Only allow keys in range [32..125]
                    if ((key >= 32) && (key <= 125) && (letterCount < MAX_INPUT_CHARS))
                    {
                        inUE[letterCount] = (char)key;
                        inUE[letterCount + 1] = '\0'; // Add null terminator at the end of the string.
                        letterCount++;
                    }

                    key = GetCharPressed(); // Check next character in the queue
                }

                if (IsKeyPressed(KEY_BACKSPACE))
                {
                    letterCount--;
                    if (letterCount < 0)
                        letterCount = 0;
                    inUE[letterCount] = '\0';
                }
            }
            else
                SetMouseCursor(MOUSE_CURSOR_DEFAULT);

            if (mouseClick)
                framesCounter++;
            else
                framesCounter = 0;
        }

        BeginDrawing();

        if (currScreen == 0)
        {
            ClearBackground(WHITE);
            DrawText("WELCOME!!!", 20, 20, 15, BLACK);

            DrawRectangleRec(textBox, LIGHTGRAY);
            if (mouseClick)
                DrawRectangleLines((int)textBox.x, (int)textBox.y, (int)textBox.width, (int)textBox.height, RED);
            else
                DrawRectangleLines((int)textBox.x, (int)textBox.y, (int)textBox.width, (int)textBox.height, DARKGRAY);

            DrawText(inUE, (int)textBox.x + 5, (int)textBox.y + 8, 5, MAROON);
            DrawText("#BTS : ", (int)textBox.x - 40, (int)textBox.y + 10, 5, BLACK);

            // DrawText(TextFormat("INPUT CHARS: %i/%i", letterCount, MAX_INPUT_CHARS), 315, 250, 20, DARKGRAY);

            // if (mouseClick)
            // {
            //     if (letterCount < MAX_INPUT_CHARS)
            //     {
            // Draw blinking underscore char
            if (((framesCounter / 10) % 2) == 0)
                DrawText(" |", (int)textBox.x + 4 + MeasureText(inUE, 5), (int)textBox.y + 8, 5, MAROON);
            //     }
            //     else
            //     {
            //     }
            //     // DrawText("Press BACKSPACE to delete chars...", 230, 300, 20, GRAY);
            // }

            if (IsKeyPressed(KEY_ENTER))
            {
                dynamic_UE_count = atoi(inUE);
                currScreen = 1;
            }
        }
        else if (currScreen == 1)
        {

            ClearBackground(BLACK);

            DrawText(placeholder1, 10, 10, 5, WHITE);

            DrawText("Number of UE's", 10, 20, 5, WHITE);
            DrawText(TextFormat("%d", ListBTS[0].number_of_UE), 10, 35, 10, BLUE);
            DrawText(TextFormat("%d", ListBTS[1].number_of_UE), 10, 45, 10, GREEN);
            DrawText(TextFormat("%d", ListBTS[2].number_of_UE), 10, 55, 10, RED);
            DrawText(TextFormat("Connection Rate : %0.f", getConnectionRate()), screenWidth / 2, 10, 10, WHITE);

            // int k = atoi(inUE);
            // k = k + 100;
            // char buf[sizeof(int) * 3 + 2];
            // snprintf(buf, sizeof buf, "%d", k);

            DrawText(inUE, 10, 75, 10, WHITE);

            drawBTS();
            createGrid();

            // update connections between bts and ue
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

            // init BTS connection count
            for (int i = 0; i < total_BTS; i++)
            {
                ListBTS[i].number_of_UE = 0;
            }

            for (int i = 0; i < dynamic_UE_count; i++)
            {
                if (state[i][0].connection > -1)
                {
                    ListBTS[state[i][0].connection].number_of_UE++;
                }
            }

            for (int i = 0; i < dynamic_UE_count; i++)
            {
                for (int j = 0; j < trail_bits; j++)
                {
                    mark(state[i][j]);
                }
            }

            srand(time(NULL));

            // get new direction based on curr and state
            for (int i = 0; i < dynamic_UE_count; i++)
            {
                cdire[i] = newDirection(state[i][0], cdire[i]);
            }

            // move colors across tail
            for (int i = 0; i < dynamic_UE_count; i++)
            {
                for (int j = trail_bits - 1; j > 0; j--)
                {
                    Color temp = state[i][j].color;
                    state[i][j] = state[i][j - 1];
                    state[i][j].color = temp;
                }
            }

            // update state based on new direction
            for (int i = 0; i < dynamic_UE_count; i++)
            {
                state[i][0] = update(state[i][0], cdire[i]);
            }

            // set all BTS
            for (int i = 0; i < total_BTS; i++)
            {
                mark(ListBTS[i].loc);
                initBTS(ListBTS[i]);
            }
        }

        EndDrawing();
    }

    CloseWindow();

    sqlite3_close(DB);

    return 0;
}
