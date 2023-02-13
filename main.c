#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sqlite3.h>
#include <semaphore.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <math.h>
#include "raylib.h"

#define MAX_INPUT_CHARS 9
#define PBSTR "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
#define PBWIDTH 60

enum Direction
{
    Top,
    Right,
    Down,
    Left
};

typedef enum
{
    Verizon,
    Comcast,
    ATnT
} ISP;
typedef struct
{
    int pointX;
    int pointY;
} Point;

typedef struct
{
    int id;
    int cellX;
    int cellY;
    Color color;
    int connection;
    ISP nodeIsp;
    int flag;
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

sem_t *mysemp;
int oflag = O_CREAT;
mode_t mode = 0666;
const char semname[] = "semname";
unsigned int value = 1;
int sts;

const int screenWidth = 800;
const int screenHeight = 450;

const int vertical_sep = 50;
const int horizontal_sep = 75;

const int lineGap = 10;

const int total_UE = 4;
const int total_BTS = 3;
const int trail_bits = 4;

const int targetfps = 10;

int progress = 0;

int dynamic_UE_count = total_UE;
int frame_tracker = 0;

int ue1 = -1, ue2 = -1;

BTS ListBTS[total_BTS];
Cell state[total_UE][trail_bits];

const int cTotalLength = 10;
// terminating it

int callback(void *data, int argc, char **argv, char **azColName)
{
    int i;
    fprintf(stderr, "%s: ", (const char *)data);

    for (i = 0; i < argc; i++)
    {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }

    printf("\n");
    return 0;
}

void printProgress(double percentage)
{
    int val = (int)(percentage * 100);
    int lpad = (int)(percentage * PBWIDTH);
    int rpad = PBWIDTH - lpad;
    printf("\r%3d%% [%.*s%*s]", val, lpad, PBSTR, rpad, "");
    fflush(stdout);
}

void printProgress1(float lProgress)
{
    char *lBuffer = malloc((cTotalLength + 1) * sizeof *lBuffer); // array to fit 10 chars + '\0'
    lBuffer[cTotalLength] = '\0';
    // float lProgress = 0.3;

    int lFilledLength = lProgress * cTotalLength;

    memset(lBuffer, 'X', lFilledLength);                                // filling filled part
    memset(lBuffer + lFilledLength, '-', cTotalLength - lFilledLength); // filling empty part
    printf("\r[%s] %.1f%%", lBuffer, lProgress * 100);                  // same princip as with the CPP method

    // or you can combine it to a single line if you want to flex ;)
    // printf("\r[%s] %.1f%%", (char*)memset(memset(lBuffer, 'X', lFullLength) + lFullLength, '-', cTotalLength - lFullLength) - lFullLength, lProgress * 100);

    free(lBuffer);
}

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

void numeralMark(Cell cell)
{
    char buf[4];
    snprintf(buf, sizeof buf, "%d", (cell.id + 1));
    DrawText(buf, (cell_to_pixel(cell)).pointX + 3, (cell_to_pixel(cell)).pointY - 3, 5, WHITE);
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
    if (curr.flag == 0)
    {
        return curr;
    }
    else
    {
        if (dir == 0)
        {
            curr.cellY--;
        }
        else if (dir == 1)
        {
            curr.cellX++;
        }
        else if (dir == 2)
        {
            curr.cellY++;
        }
        else
        {
            curr.cellX--;
        }
        return curr;
    }
}

void drawBTS()
{
    for (int i = 0; i < total_BTS; i++)
    {
        DrawText(TextFormat("BTS%d", i), cell_to_pixel(ListBTS[i].loc).pointX + 10, cell_to_pixel(ListBTS[i].loc).pointY + 10, 8, ListBTS[i].loc.color);
    }
}

int newDirection(Cell curr, int cDir)
{
    struct pair_cell_dir temp_pair;
    temp_pair = validate(curr);
    if (temp_pair.dir != -1)
    {
        return temp_pair.dir;
    }

    int r = rand();
    int k = r % 4;
    if ((cDir == 0 && k == 2) || (cDir == 2 && k == 0) || (cDir == 1 && k == 3) || (cDir == 3 && k == 1))
    {
        return (k + 7) % 4;
    }
    if (r < RAND_MAX / 8)
    {
        return k;
    }
    else
    {
        return cDir;
    }
}

void connectUEtoBTS(Cell ue, Cell BTS)
{
    Point point_ue = cell_to_pixel(ue);
    Point point_bts = cell_to_pixel(BTS);
    DrawLine(point_ue.pointX, point_ue.pointY, point_bts.pointX, point_bts.pointY, BTS.color);
}

struct pair_int_int nearestBTS(Cell curr)
{
    struct pair_int_int ret;
    int min_rad_index = -1, min_rad = 5000;
    for (int i = 0; i < total_BTS; i++)
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

float get_progress(int i)
{
    return 1 / (1 + (56 * pow(2.71, (-0.003 * 100 * i))));
}

void *user_input(void *arg)
{
    char c;
    while (1)
    {
        c = getchar();
        if (c == 'p')
        {
            sem_wait(mysemp);
            printf("Enter ue's to connect : ");
            scanf("%d %d", &ue1, &ue2);
            sem_post(mysemp);
        }
    }
}

int main(void)
{

    // init thread to read input while the process in running
    pthread_t thread;

    mysemp = sem_open(semname, oflag, mode, value);

    if (mysemp == (void *)-1)
    {
        perror("sem_open() failed");
    }

    pthread_create(&thread, NULL, user_input, NULL);

    // init sqlite db to store ue data for bts to fetch in a future version where ue and bts
    // can't directly access each others data
    sqlite3 *DB;
    int exit_stat = 0;
    char *messageError;

    char *sql = "CREATE TABLE IF NOT EXISTS UE("
                "ID INT PRIMARY KEY NOT NULL,"
                "X  INT             NOT NULL,"
                "Y  INT             NOT NULL,"
                "COLOR CHAR(32)             ,"
                "CONNECTION INT             ,"
                "ISP INT                    );";

    exit_stat = sqlite3_open("net-sim.db", &DB);

    // create ue table
    exit_stat = sqlite3_exec(DB, sql, NULL, 0, &messageError);

    if (exit_stat != SQLITE_OK)
    {
        printf("Error opening db %s \n", sqlite3_errmsg(DB));
        sqlite3_free(messageError);
        return -1;
    }
    else
    {
        printf("Table created successfully \n");
    }

    // insert dummy values
    char *sql_insert = "INSERT OR IGNORE INTO UE VALUES(1, 20, 10, 'WHITE', 2, 0);"
                       "INSERT OR IGNORE INTO UE VALUES(2, 5, 5, 'BLUE', 4, 1);";

    exit_stat = sqlite3_exec(DB, sql_insert, NULL, 0, &messageError);

    if (exit_stat != SQLITE_OK)
    {

        fprintf(stderr, "SQL error: %s\n", messageError);

        sqlite3_free(messageError);
        sqlite3_close(DB);

        return 1;
    }
    else
    {
        printf("Insertion successfully \n");
    }

    // disply the db
    char *sql_query = "SELECT * FROM UE;";

    exit_stat = sqlite3_exec(DB, sql_query, callback, NULL, &messageError);

    if (exit_stat != SQLITE_OK)
    {
        fprintf(stderr, "Failed to select data\n");
        fprintf(stderr, "SQL error: %s\n", messageError);

        sqlite3_free(messageError);
        sqlite3_close(DB);

        return 1;
    }
    else
    {
        printf("Query passed \n");
    }

    SetTraceLogCallback(CustomLog);

    InitWindow(screenWidth, screenHeight, "Net-Sim");
    SetTargetFPS(targetfps);

    int cdire[total_UE];

    char *placeholder1 = "Network-Sim";

    // Cell state[4];
    Color stateColor[trail_bits] = {WHITE,
                                    LIGHTGRAY,
                                    GRAY,
                                    DARKGRAY};

    // Man data 1
    int startLoc[total_UE][2] = {{10, 10}, {20, 20}, {30, 30}, {40, 40}};
    // printf("%s\n", (dynamic_UE_count));
    // printf("%d\n", (trail_bits));

    // init ue in local storage
    for (int i = 0; i < dynamic_UE_count; i++)
    {
        for (int j = 0; j < trail_bits; j++)
        {
            state[i][j].cellX = startLoc[i][0];
            state[i][j].cellY = startLoc[i][1];
            state[i][j].color = stateColor[j];
            state[i][j].connection = -1;
            state[i][j].nodeIsp = Verizon;
            state[i][j].flag = 1;
            state[i][j].id = i;
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
    int currFrame = 0;

    // activate when cursor in box
    bool mouseOnText = false;

    // activate when click
    bool mouseClick = false;

    // bool connect_1_3 = false;

    Rectangle textBox = {60, 60, 80, 30};

    while (!WindowShouldClose())
    {
        // menu screen
        if (currScreen == 0)
        {
            // [deprecated attempt for selection]
            // if (CheckCollisionPointRec(GetMousePosition(), textBox))
            // {
            //     mouseOnText = true;
            // }
            // else
            // {
            //     mouseOnText = false;
            // }

            // check if mouse in box -> make into function
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
                    {
                        letterCount = 0;
                    }
                    inUE[letterCount] = '\0';
                }
            }
            else
                SetMouseCursor(MOUSE_CURSOR_DEFAULT);

            if (mouseClick)
            {
                framesCounter++;
            }
            else
            {
                framesCounter = 0;
            }
        }
        else if (currScreen == 1)
        {

            // [part of a deprecated implementation]
            // if (IsKeyPressed(KEY_A) && IsKeyPressed(KEY_B))
            // {
            //     connect_1_3 = true;
            // }
            // if (IsKeyReleased(KEY_A) && IsKeyReleased(KEY_B))
            // {
            //     connect_1_3 = false;
            // }
            frame_tracker++;
        }

        BeginDrawing();

        if (currScreen == 0)
        {
            ClearBackground(WHITE);
            DrawText("WELCOME!!!", 20, 20, 15, BLACK);

            DrawRectangleRec(textBox, LIGHTGRAY);
            if (mouseClick)
            {
                DrawRectangleLines((int)textBox.x, (int)textBox.y, (int)textBox.width, (int)textBox.height, RED);
            }
            else
            {
                DrawRectangleLines((int)textBox.x, (int)textBox.y, (int)textBox.width, (int)textBox.height, DARKGRAY);
            }
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

            // exit menu screen when enter is pressed
            // takes delay for some reason
            if (IsKeyPressed(KEY_ENTER))
            {
                dynamic_UE_count = atoi(inUE);
                currScreen = 1;
            }
        }
        // sim screen
        else if (currScreen == 1)
        {
            // printf("connect 1_3 : %d \n", connect_1_3);

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
            // if
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

            // from the data recieved from io thread, link the ue's
            if (ue1 > -1 && ue2 > -1)
            {
                if (state[ue1 - 1][0].connection > -1 && state[ue2 - 1][0].connection > -1)
                {
                    if (frame_tracker - currFrame == 1)
                    {
                        printf("Connection Established... \n");
                        printProgress(1);
                    }
                    int test_n;
                    state[ue1 - 1][0].flag = 0;
                    state[ue2 - 1][0].flag = 0;
                    if (currFrame == 0)
                        currFrame = frame_tracker;
                    else
                    {
                        if (frame_tracker - currFrame >= 10)
                        {
                            printf("Disconnecting... \n");
                            state[ue1 - 1][0].flag = 1;
                            state[ue2 - 1][0].flag = 1;
                            currFrame = 0;
                            ue1 = -1;
                            ue2 = -1;
                            progress = 0;
                            printf("Disconnected successfully... \n");
                        }
                    }
                }
                else
                {
                    float value = get_progress(progress);
                    printProgress(value);
                    progress++;
                    // fflush(stdout);
                }
            }

            // if (framesCounter1 < 20)
            // {
            //     Cell temp1;
            //     temp1.cellX = 20;
            //     temp1.cellY = 15;
            //     temp1.color = WHITE;
            //     mark(temp1);
            // }

            // template to update db on estabilished connections
            char sql_connection[64];
            sprintf(sql_connection, "UPDATE UE SET CONNECTION=%d WHERE ID = 1", state[0][0].connection);

            exit_stat = sqlite3_exec(DB, sql_connection, callback, NULL, &messageError);

            if (exit_stat != SQLITE_OK)
            {
                fprintf(stderr, "Failed to select data\n");
                fprintf(stderr, "SQL error: %s\n", messageError);

                sqlite3_free(messageError);
                sqlite3_close(DB);

                return 1;
            }
            else
            {
                // printf("Connection estabilished with BTS %d \t", state[0][0].connection);
            }

            // draw line between ue and connected db
            for (int i = 0; i < total_UE; i++)
            {
                connectUEtoBTS(state[i][0], ListBTS[state[i][0].connection].loc);
            }

            // init BTS connection count
            for (int i = 0; i < total_BTS; i++)
            {
                ListBTS[i].number_of_UE = 0;
            }

            // update bts memory(temp)
            for (int i = 0; i < dynamic_UE_count; i++)
            {
                if (state[i][0].connection > -1)
                {
                    ListBTS[state[i][0].connection].number_of_UE++;
                }
            }

            // init mark the ue
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

            // template to update position of ue in db
            char sql_update[64];
            sprintf(sql_update, "UPDATE UE SET X=%d, Y=%d WHERE ID = 1", state[0][0].cellX, state[0][0].cellY);

            // printf("sql_update : %s \n", sql_update);
            // printf("reached here \n");

            exit_stat = sqlite3_exec(DB, sql_update, callback, NULL, &messageError);

            if (exit_stat != SQLITE_OK)
            {
                fprintf(stderr, "Failed to select data\n");
                fprintf(stderr, "SQL error: %s\n", messageError);

                sqlite3_free(messageError);
                sqlite3_close(DB);

                return 1;
            }
            else
            {
                // printf("Query passed \n");
                // printf("connect 1_3 : %d \n", connect_1_3);
            }
            for (int i = 0; i < dynamic_UE_count; i++)
            {
                numeralMark(state[i][0]);
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

    sem_close(mysemp);
    sem_unlink("semname");

    return 0;
}
