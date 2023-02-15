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
#define GET_VARIABLE_NAME(Variable) (#Variable)
#define PBSTR "############################################################"
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

struct map
{
    int key;
    char *value;
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

int ue_x = -1, ue1 = -1, ue2 = -1;
int ue_recharge = 0;

float validity_update = 0.1f;

char *status_update = "none";

BTS ListBTS[total_BTS];
Cell state[total_UE][trail_bits];

const int cTotalLength = 10;

char *EID[] = {"V001", "C001", "V002", "A001"};
struct map EIDMapping[total_UE];

void populateEID()
{
    for (int i = 0; i < dynamic_UE_count; i++)
    {
        EIDMapping[i].key = i;
        EIDMapping[i].value = EID[i];
    }
}

// for sqlite console display
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

int callback_get(void *data, int argc, char **argv, char **azColName)
{
    int i;
    // fprintf(stderr, "%s: ", (const char *)data);
    char *tempEID;
    int checker = 0;
    for (i = 0; i < argc; i++)
    {
        // printf("%s : %s", azColName[i], argv[i]);
        if (strcmp(azColName[i], "EID") == 0)
        {
            tempEID = argv[i];
        }
        if (strcmp(azColName[i], "STATUS") == 0)
        {
            checker = atoi(argv[i]);
        }

        // printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");

        if (strcmp(azColName[i], "VALIDITY") == 0)
        {
            // printf("here : %d \n", argv[i]);
            int val = atoi(argv[i]);
            if (val <= validity_update)
            {
                // printf("%s : %s \n", azColName[i], val);
                if (checker != -1)
                {
                    printf("Connection for EID : %s has been disabled \n", tempEID);
                    status_update = tempEID;
                    // printf("status : %s \n", status_update);
                }

                // printf("status_update : %s \n", status_update);
                // printf("\n");
            }
        }
    }

    // printf("\n");
    return 0;
}

// progress bar
void printProgress(double percentage)
{
    int val = (int)(percentage * 100);
    int lpad = (int)(percentage * PBWIDTH);
    int rpad = PBWIDTH - lpad;
    printf("\r%3d%% [%.*s%*s]", val, lpad, PBSTR, rpad, "");
    fflush(stdout);
}

// progress bar better [not used]
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

// timer for raylib
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

// logger for raylib
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

// init grid
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

// convert values in cell format to pixel format
Point cell_to_pixel(Cell cell)
{
    Point ret;
    ret.pointX = cell.cellX * lineGap + horizontal_sep;
    ret.pointY = cell.cellY * lineGap + vertical_sep;

    return ret;
}

// display id of ue
void ueID(Cell cell)
{
    char buf[4];
    snprintf(buf, sizeof buf, "%d", (cell.id + 1));
    DrawText(buf, (cell_to_pixel(cell)).pointX + 3, (cell_to_pixel(cell)).pointY - 3, 5, WHITE);
}

// validator for ue
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

// mark cells
void mark(Cell curr)
{
    DrawRectangle(horizontal_sep + lineGap * curr.cellX, vertical_sep + lineGap * curr.cellY, lineGap, lineGap, curr.color);
}

void initBTS(BTS bts)
{
    DrawCircleLines(horizontal_sep + lineGap * bts.loc.cellX + 0.5 * lineGap, vertical_sep + lineGap * bts.loc.cellY + 0.5 * lineGap, bts.radius, bts.loc.color);
}

// update ue path
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

// get new direction for ue
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

void connectUEtoBTS(Cell ue, Cell BTS, int connection_type)
{
    Point point_bts = cell_to_pixel(BTS);
    Point point_ue = cell_to_pixel(ue);

    // not connected to another ue
    if (connection_type == 0)
    {
        DrawLine(point_ue.pointX, point_ue.pointY, point_bts.pointX, point_bts.pointY, BTS.color);
    }
    else
    {
        DrawLine(point_ue.pointX, point_ue.pointY, point_bts.pointX, point_bts.pointY, PURPLE);
    }
}

// used when two ue's are connected
void connectBTStoBTS(Cell BTS1, Cell BTS2)
{
    Point point_bts1 = cell_to_pixel(BTS1);
    Point point_bts2 = cell_to_pixel(BTS2);
    DrawLine(point_bts1.pointX, point_bts1.pointY, point_bts2.pointX, point_bts2.pointY, PURPLE);
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

// logistic growth function for progress bar
float get_progress(int i)
{
    return 1 / (1 + (56 * pow(2.71, (-0.003 * 100 * i))));
}

// for user input
void *user_input(void *arg)
{
    char c;
    while (1)
    {
        char str[10];
        fgets(str, 10, stdin);

        str[strcspn(str, "\n")] = 0;

        if (strcmp(str, "connect") == 0)
        {
            sem_wait(mysemp);
            printf("Enter EID of UE's u wish to connect : ");
            scanf("%d %d", &ue1, &ue2);
            sem_post(mysemp);
        }
        else if (strcmp(str, "recharge") == 0)
        {
            sem_wait(mysemp);
            printf("Enter EID of UE you wish to recharge and duration(1-100) : ");
            scanf("%d %d", &ue_x, &ue_recharge);
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

    char *sql_ue = "CREATE TABLE IF NOT EXISTS UE("
                   "EID INT PRIMARY KEY NOT NULL,"
                   "ID INT              NOT NULL,"
                   "X  INT              NOT NULL,"
                   "Y  INT              NOT NULL);";

    char *sql_hlr = "CREATE TABLE IF NOT EXISTS HLR("
                    "EID CHAR(6) PRIMARY KEY NOT NULL,"
                    "X INT              NOT NULL,"
                    "Y INT              NOT NULL,"
                    "ISP CHAR(12)       NOT NULL,"
                    "STATUS INT         NOT NULL,"
                    "VALIDITY REAL       NOT NULL,"
                    "CONNECT INT                );";

    /*
    Each UE has its data in HLR:
        1. EID      : Equipment ID for BTS to identify
        2. X        : X Coordinate of the UE
        3. Y        : Y Coordinate of the UE
        4. ISP      : ISP of the UE
        5. Status   : Whether active on network
        6. Validity : Number of days before disconnection
        7. Connect  : BTS to which it is connected
    */

    exit_stat = sqlite3_open("net-sim.db", &DB);
    if (exit_stat != SQLITE_OK)
    {
        fprintf(stderr, "rc = %d\n", exit_stat);
        fprintf(stderr, "Cannot open sqlite3 database : %s\n", sqlite3_errmsg(DB));
        return -1;
    }

    // create ue table
    exit_stat = sqlite3_exec(DB, sql_ue, NULL, 0, &messageError);

    if (exit_stat != SQLITE_OK)
    {
        printf("Error creating UE table %s \n", sqlite3_errmsg(DB));
        sqlite3_free(messageError);
        return -1;
    }
    else
    {
        printf("Table %s created successfully \n", GET_VARIABLE_NAME(sql_ue));
    }

    // create hlr table
    exit_stat = sqlite3_exec(DB, sql_hlr, NULL, 0, &messageError);

    if (exit_stat != SQLITE_OK)
    {
        printf("Error creating HLR table %s \n", sqlite3_errmsg(DB));
        sqlite3_free(messageError);
        return -1;
    }
    else
    {
        printf("Table %s created successfully \n", GET_VARIABLE_NAME(sql_hlr));
    }

    // insert dummy values into ue table
    char *sql_insert_ue = "INSERT OR IGNORE INTO UE VALUES('V001', 1, 20, 10);"
                          "INSERT OR IGNORE INTO UE VALUES('C001', 2, 5, 5);"
                          "INSERT OR IGNORE INTO UE VALUES('V002', 3, 10, 10);"
                          "INSERT OR IGNORE INTO UE VALUES('A001', 4, 15, 15);";

    exit_stat = sqlite3_exec(DB, sql_insert_ue, NULL, 0, &messageError);

    if (exit_stat != SQLITE_OK)
    {

        fprintf(stderr, "Error pushing def data into UE Table: %s\n", messageError);

        sqlite3_free(messageError);
        sqlite3_close(DB);

        return 1;
    }
    else
    {
        printf("Insertion into %s successfully \n", GET_VARIABLE_NAME(sql_ue));
    }

    // insert dummy values into hlr table
    char *sql_insert_hlr = "INSERT OR IGNORE INTO HLR VALUES('V001', 20, 10, 'VERIZON', 1, 11, 1);"
                           "INSERT OR IGNORE INTO HLR VALUES('C001', 5,  5,  'COMCAST', 1, 12, 2);"
                           "INSERT OR IGNORE INTO HLR VALUES('V002', 10, 10, 'VERIZOM', 0,  5, 0);"
                           "INSERT OR IGNORE INTO HLR VALUES('A001', 15, 15, 'ATnT',    1,  27, 1);";

    exit_stat = sqlite3_exec(DB, sql_insert_hlr, NULL, 0, &messageError);

    if (exit_stat != SQLITE_OK)
    {

        fprintf(stderr, "Error pushing def data into HLR Table: %s\n", messageError);

        sqlite3_free(messageError);
        sqlite3_close(DB);

        return 1;
    }
    else
    {
        printf("Insertion into %s successfully \n", GET_VARIABLE_NAME(sql_hlr));
    }

    // disply the ue table
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

    populateEID();
    SetTraceLogCallback(CustomLog);

    InitWindow(screenWidth, screenHeight, "Net-Sim");
    SetWindowPosition(50, 100);
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
            ClearBackground(BLACK);
            DrawText("WELCOME!!!", 20, 20, 15, WHITE);

            DrawRectangleRec(textBox, LIGHTGRAY);
            if (mouseClick)
            {
                DrawRectangleLines((int)textBox.x, (int)textBox.y, (int)textBox.width, (int)textBox.height, RED);
            }
            else
            {
                DrawRectangleLines((int)textBox.x, (int)textBox.y, (int)textBox.width, (int)textBox.height, DARKGRAY);
            }
            DrawText(inUE, (int)textBox.x + 5, (int)textBox.y + 8, 5, BLACK);
            DrawText("#BTS : ", (int)textBox.x - 40, (int)textBox.y + 10, 5, WHITE);

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

            bool connectPermission[dynamic_UE_count];

            sqlite3_stmt *stmt;
            int rc;
            rc = sqlite3_prepare_v2(DB, "SELECT * FROM HLR", -1, &stmt, 0);
            if (rc != SQLITE_OK)
            {
                fprintf(stderr, "Cannot prepare statement: %s\n", sqlite3_errmsg(DB));
                return 1;
            }
            int pos = 0, i = 0;
            while (sqlite3_step(stmt) == SQLITE_ROW)
            {
                int validity = sqlite3_column_int(stmt, 4);
                // printf("id=%d \n", id);
                if (validity == -1)
                {
                    connectPermission[i] = false;
                    // uncomment this if you wish to stop ue on end of validity
                    // state[pos][0].flag = 0;
                }
                else
                {
                    connectPermission[i] = true;
                    pos++;
                }
                i++;
            }
            // for (int i = 0; i < dynamic_UE_count; i++)
            // {
            //     printf("%d ", connectPermission[i]);
            // }
            // printf("\n");

            // update connections between bts and ue
            // if
            for (int i = 0; i < dynamic_UE_count; i++)
            {
                struct pair_int_int ue_to_bts;
                ue_to_bts = nearestBTS(state[i][0]);

                if (ue_to_bts.y > -1)
                {
                    if (connectPermission[i])
                    {
                        state[i][0].connection = ue_to_bts.y;
                        state[i][0].color = ListBTS[ue_to_bts.y].loc.color;
                    }
                    else
                    {
                        state[i][0].connection = -1;
                        // DrawLine(horizontal_sep + lineGap * state[i][0].cellX,
                        //          vertical_sep + lineGap * state[i][0].cellY,
                        //          horizontal_sep + (lineGap + 1) * state[i][0].cellX,
                        //          vertical_sep + (lineGap + 1) * state[i][0].cellY, WHITE);
                    }
                }
                else
                {
                    state[i][0].connection = -1;
                    state[i][0].color = WHITE;
                }
                char sql_connection_connect[64];
                sprintf(sql_connection_connect, "UPDATE HLR SET CONNECT=%d WHERE EID = '%s'", state[i][0].connection, EIDMapping[i].value);

                exit_stat = sqlite3_exec(DB, sql_connection_connect, callback, NULL, &messageError);

                if (exit_stat != SQLITE_OK)
                {
                    fprintf(stderr, "Failed to set connect data\n");
                    fprintf(stderr, "SQL error: %s\n", messageError);

                    sqlite3_free(messageError);
                    sqlite3_close(DB);

                    return 1;
                }
                else
                {
                    // printf("Connection estabilished with BTS %d \t", state[0][0].connection);
                }
            }

            // update validity
            for (int i = 0; i < dynamic_UE_count; i++)
            {
                char sql_validity_update[128];
                sprintf(sql_validity_update, "UPDATE HLR SET VALIDITY=VALIDITY-0.1 WHERE EID = '%s' AND VALIDITY > 0 ",
                        // validity_update, EIDMapping[i].value, validity_update);
                        EIDMapping[i].value);

                exit_stat = sqlite3_exec(DB, sql_validity_update, callback, NULL, &messageError);

                if (exit_stat != SQLITE_OK)
                {
                    fprintf(stderr, "Failed to set update validity data\n");
                    fprintf(stderr, "SQL error: %s\n", messageError);

                    sqlite3_free(messageError);
                    sqlite3_close(DB);

                    return 1;
                }
                else
                {
                    // printf("Connection estabilished with BTS %d \t", state[0][0].connection);
                }
            }

            // check if the ue has a valid subscription
            for (int i = 0; i < dynamic_UE_count; i++)
            {
                char sql_display_validity[64];
                sprintf(sql_display_validity, "SELECT * FROM HLR WHERE EID = '%s'", EIDMapping[i].value);
                // printf("status 1 : %s : %d\n", status_update, i);
                exit_stat = sqlite3_exec(DB, sql_display_validity, callback_get, NULL, &messageError);
                // printf("status 2 : %s : %d\n", status_update, i);

                if (exit_stat != SQLITE_OK)
                {
                    fprintf(stderr, "Failed to set update validity data\n");
                    fprintf(stderr, "SQL error: %s\n", messageError);

                    sqlite3_free(messageError);
                    sqlite3_close(DB);

                    return 1;
                }
                else
                {
                    // printf("Connection estabilished with BTS %d \t", state[0][0].connection);
                }
                // printf("status 3 : %s : %d\n", status_update, i);

                // if the ue does not have a valid subscription, set its status as -1
                if (strcmp(status_update, "none") != 0)
                {
                    char sql_update_status[64];
                    sprintf(sql_update_status, "UPDATE HLR SET STATUS = -1 WHERE EID = '%s'", EIDMapping[i].value);

                    exit_stat = sqlite3_exec(DB, sql_update_status, callback_get, NULL, &messageError);

                    if (exit_stat != SQLITE_OK)
                    {
                        fprintf(stderr, "Failed to set update status data\n");
                        fprintf(stderr, "SQL error: %s\n", messageError);

                        sqlite3_free(messageError);
                        sqlite3_close(DB);

                        return 1;
                    }
                    else
                    {
                        // printf("Connection disabled for %s \t", EIDMapping[i].value);
                    }
                    status_update = "none";
                }
            }

            // from the data recieved from io thread, link the ue's
            if (ue1 > -1 && ue2 > -1)
            {

                // check if either of the device has an invalid subscription
                if (!connectPermission[ue1 - 1] || !connectPermission[ue2 - 1])
                {
                    printf("Unauthorized access \n");
                    if (!connectPermission[ue1 - 1])
                        printf("UE with EID : %s has invalid subscription\n", connectPermission[ue1 - 1]);
                    else
                        printf("UE with EID : %s has invalid subscription\n", connectPermission[ue2 - 1]);

                    ue1 = -1;
                    ue2 = -1;
                }

                // check if both devices are connected
                else if (state[ue1 - 1][0].connection > -1 && state[ue2 - 1][0].connection > -1)
                {
                    connectBTStoBTS(ListBTS[state[ue1 - 1][0].connection].loc, ListBTS[state[ue2 - 1][0].connection].loc);
                    connectUEtoBTS(state[ue1 - 1][0], ListBTS[state[ue1 - 1][0].connection].loc, 1);
                    connectUEtoBTS(state[ue2 - 1][0], ListBTS[state[ue2 - 1][0].connection].loc, 1);
                    if (frame_tracker - currFrame == 1)
                    {
                        printProgress(1);
                        printf("Connection Established... \n");
                    }
                    int test_n;
                    state[ue1 - 1][0].flag = 0;
                    state[ue2 - 1][0].flag = 0;
                    if (currFrame == 0)
                        currFrame = frame_tracker;
                    else
                    {
                        if (frame_tracker - currFrame >= 50)
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

                // else run the progress bar
                else
                {
                    float value = get_progress(progress);
                    printProgress(value);
                    progress++;
                    // fflush(stdout);
                }
            }

            // if ue_recharge is positive means a request has been recieved
            if (ue_recharge > 0)
            {
                char sql_recharge[64];
                sprintf(sql_recharge, "UPDATE HLR SET VALIDITY=%d, STATUS=1 WHERE EID = '%s'", ue_recharge, EIDMapping[ue_x - 1].value);

                // printf("sql_update : %s \n", sql_update);
                // printf("reached here \n");

                // sql command to update ue
                exit_stat = sqlite3_exec(DB, sql_recharge, callback, NULL, &messageError);

                if (exit_stat != SQLITE_OK)
                {
                    fprintf(stderr, "Failed to update validity and status data for %s \n", GET_VARIABLE_NAME(sql_recharge));
                    fprintf(stderr, "SQL error: %s\n", messageError);

                    sqlite3_free(messageError);
                    sqlite3_close(DB);

                    return 1;
                }
                else
                {
                    printf("Success!! \n");
                }
                printf("Recharge successful for %d on EID %s \n", ue_recharge, EIDMapping[ue_x - 1].value);
                ue_recharge = 0;
                ue_x = -1;
            }

            // if (framesCounter1 < 20)
            // {
            //     Cell temp1;
            //     temp1.cellX = 20;
            //     temp1.cellY = 15;
            //     temp1.color = WHITE;
            //     mark(temp1);
            // }

            // draw line between ue and connected db
            for (int i = 0; i < total_UE; i++)
            {
                if (ue1 != i + 1 && ue2 != i + 1)
                {
                    connectUEtoBTS(state[i][0], ListBTS[state[i][0].connection].loc, 0);
                }
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
                    if (!connectPermission[i] && j == 0)
                    {

                        // for canceling out invalid ue's
                        DrawLine(cell_to_pixel(state[i][0]).pointX,
                                 cell_to_pixel(state[i][0]).pointY,
                                 cell_to_pixel(state[i][0]).pointX + lineGap,
                                 cell_to_pixel(state[i][0]).pointY + lineGap, BLACK);

                        DrawLine(cell_to_pixel(state[i][0]).pointX + lineGap,
                                 cell_to_pixel(state[i][0]).pointY,
                                 cell_to_pixel(state[i][0]).pointX,
                                 cell_to_pixel(state[i][0]).pointY + lineGap, BLACK);
                    }
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

            // update position in database
            for (int i = 0; i < dynamic_UE_count; i++)
            {
                char sql_update_ue[64], sql_update_hlr[64];
                sprintf(sql_update_ue, "UPDATE UE SET X=%d, Y=%d WHERE EID = '%s'", state[i][0].cellX, state[i][0].cellY, EIDMapping[i].value);
                sprintf(sql_update_hlr, "UPDATE HLR SET X=%d, Y=%d WHERE EID = '%s'", state[i][0].cellX, state[i][0].cellY, EIDMapping[i].value);

                // printf("sql_update : %s \n", sql_update);
                // printf("reached here \n");

                // sql command to update ue
                exit_stat = sqlite3_exec(DB, sql_update_ue, callback, NULL, &messageError);

                if (exit_stat != SQLITE_OK)
                {
                    fprintf(stderr, "Failed to select data for %s \n", GET_VARIABLE_NAME(sql_update_ue));
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

                // sql command to update hlr
                exit_stat = sqlite3_exec(DB, sql_update_hlr, callback, NULL, &messageError);

                if (exit_stat != SQLITE_OK)
                {
                    fprintf(stderr, "Failed to select data for %s \n", GET_VARIABLE_NAME(sql_update_hlr));
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
            }
            for (int i = 0; i < dynamic_UE_count; i++)
            {
                ueID(state[i][0]);
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

    char sql_drop[64];
    sprintf(sql_drop, "DROP TABLE UE");
    // printf("status 1 : %s : %d\n", status_update, i);
    exit_stat = sqlite3_exec(DB, sql_drop, callback_get, NULL, &messageError);
    // printf("status 2 : %s : %d\n", status_update, i);

    if (exit_stat != SQLITE_OK)
    {
        fprintf(stderr, "Failed to set update validity data\n");
        fprintf(stderr, "SQL error: %s\n", messageError);

        sqlite3_free(messageError);
        sqlite3_close(DB);

        return 1;
    }
    else
    {
        // printf("Connection estabilished with BTS %d \t", state[0][0].connection);
    }

    // char sql_drop[64];
    sprintf(sql_drop, "DROP TABLE HLR");
    // printf("status 1 : %s : %d\n", status_update, i);
    exit_stat = sqlite3_exec(DB, sql_drop, callback_get, NULL, &messageError);
    // printf("status 2 : %s : %d\n", status_update, i);

    if (exit_stat != SQLITE_OK)
    {
        fprintf(stderr, "Failed to set update validity data\n");
        fprintf(stderr, "SQL error: %s\n", messageError);

        sqlite3_free(messageError);
        sqlite3_close(DB);

        return 1;
    }
    else
    {
        // printf("Connection estabilished with BTS %d \t", state[0][0].connection);
    }

    sqlite3_close(DB);

    sem_close(mysemp);
    sem_unlink("semname");

    return 0;
}
