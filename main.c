/**
MIT License

Copyright (c) 2023 Sarthak Jha

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

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
#include <regex.h>
#include <signal.h>
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
    int mscId;
    int roaming;
} Cell;

typedef struct
{
    Cell loc;
    int radius;
    int number_of_UE;
    ISP btsISP;
} BTS;

typedef struct
{
    Cell loc;
    BTS resp[6];
    int numberofBTS;
    int ue_connects;
} MSC;

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

typedef struct node
{
    struct pair_int_int data;
    struct node *next;
} Node;

typedef struct linked_list
{
    Node *head;
} LinkedList;

// semaphore parameters
sem_t *mysemp;
int oflag = O_CREAT;
mode_t mode = 0666;
const char semname[] = "semname";
unsigned int value = 1;
int sts;

sqlite3 *DB;

const int screenWidth = 1000;
const int screenHeight = 800;

const int vertical_sep = 50;
const int horizontal_sep = 75;

const int lineGap = 10;

const int total_UE = 5;
const int total_BTS = 6;
const int total_MSC = 2;
const int trail_bits = 4;

const int targetfpsSim = 10;
const int targetfpsMenu = 60;

const int connectLimit = 10;

int progress = 0;

int dynamic_UE_count = total_UE;
int frame_tracker = 0;

int pause_x = 0;

// conenction related
int ue_x = -1, ue1_g = -1, ue2_g = -1;
int ue_recharge = 0;
regex_t regex;

float validity_update = 0.1f;

char *status_update = "none";

BTS ListBTS[total_BTS];
MSC ListMSC[total_MSC];
Cell state[total_UE][trail_bits];

// ds to store the isp of ue's and bts's
// need to manually add a new entry
// take input of UE from database
ISP BTS_ISP[total_BTS] = {Verizon,
                          Verizon,
                          ATnT,
                          Comcast,
                          Comcast,
                          Comcast};

char *BTS_ISP_DIS[total_BTS] = {"Verizon",
                                "Verizon",
                                "ATnT",
                                "Comcast",
                                "Comcast",
                                "Comcast"};

ISP UE_ISP[total_UE] = {Verizon,
                        Comcast,
                        Verizon,
                        ATnT,
                        ATnT};

char *UE_ISP_DIS[total_UE] = {"Verizon",
                              "Comcast",
                              "Verizon",
                              "ATnT",
                              "ATnT"};

const int cTotalLength = 10;

char *EID[] = {"V001", "C001", "V002", "A001", "A002"};

struct map EIDMapping[total_UE];

LinkedList *list;

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
                    // printf("Connection for EID : %s has been disabled \n", tempEID);
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

LinkedList *create_list()
{
    LinkedList *list = (LinkedList *)malloc(sizeof(LinkedList));
    list->head = NULL;
    return list;
}

void add_node(LinkedList *list, struct pair_int_int data)
{
    Node *new_node = (Node *)malloc(sizeof(Node));
    new_node->data = data;
    new_node->next = NULL;
    if (list->head == NULL)
    {
        list->head = new_node;
    }
    else
    {
        Node *current = list->head;
        while (current->next != NULL)
        {
            current = current->next;
        }
        current->next = new_node;
    }
}

void insert_node(LinkedList *list, struct pair_int_int data, int position)
{
    Node *new_node = (Node *)malloc(sizeof(Node));
    new_node->data = data;
    new_node->next = NULL;
    if (position == 0)
    {
        new_node->next = list->head;
        list->head = new_node;
    }
    else
    {
        Node *current = list->head;
        for (int i = 0; i < position - 1; i++)
        {
            if (current == NULL)
            {
                printf("Position out of range!\n");
                return;
            }
            current = current->next;
        }
        new_node->next = current->next;
        current->next = new_node;
    }
}

void delete_node(LinkedList *list, int position)
{
    if (list->head == NULL)
    {
        printf("List is empty!\n");
        return;
    }
    if (position == 0)
    {
        Node *temp = list->head;
        list->head = list->head->next;
        free(temp);
    }
    else
    {
        Node *current = list->head;
        for (int i = 0; i < position - 1; i++)
        {
            if (current->next == NULL)
            {
                printf("Position out of range!\n");
                return;
            }
            current = current->next;
        }
        Node *temp = current->next;
        current->next = current->next->next;
        free(temp);
    }
}

void print_list(LinkedList *list)
{
    Node *current = list->head;
    while (current != NULL)
    {
        // printf("%d ", current->data);
        current = current->next;
    }
    printf("\n");
}

// init grid
void createGrid(int mode)
{
    Point points[4];
    if (mode == 2)
    {
        DrawLine(horizontal_sep, vertical_sep, horizontal_sep, screenHeight - vertical_sep - lineGap, LIGHTGRAY);
        DrawLine(screenWidth - horizontal_sep - lineGap, vertical_sep, screenWidth - horizontal_sep - lineGap, screenHeight - vertical_sep - lineGap, LIGHTGRAY);

        DrawLine(horizontal_sep, vertical_sep, screenWidth - horizontal_sep - lineGap, vertical_sep, LIGHTGRAY);
        DrawLine(horizontal_sep, screenHeight - vertical_sep - lineGap, screenWidth - horizontal_sep - lineGap, screenHeight - vertical_sep - lineGap, LIGHTGRAY);
    }
    if (mode == 0)
    {
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
    char buf[dynamic_UE_count];
    snprintf(buf, sizeof buf, "%d", (cell.id + 1));

    if ((cell.id + 1) != 5)
    {
        DrawText(buf, (cell_to_pixel(cell)).pointX + 3, (cell_to_pixel(cell)).pointY - 3, 5, WHITE);
    }
    else
    {
        char *mod_x = "X";
        DrawText(TextFormat("$"), (cell_to_pixel(cell)).pointX + 3, (cell_to_pixel(cell)).pointY - 3, 5, WHITE);
    }
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

// draw the main bts marking
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

// draw BTS text indication
void drawBTS()
{
    for (int i = 0; i < total_BTS; i++)
    {
        DrawText(TextFormat("#%d $%s", i, BTS_ISP_DIS[i]), cell_to_pixel(ListBTS[i].loc).pointX + 10, cell_to_pixel(ListBTS[i].loc).pointY + 10, 5, ListBTS[i].loc.color);
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

// connect UE to BTS, the connection type is to give specific color while in
// connection
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

// msc class data init
void initMSC()
{
    ListMSC[0].loc.cellX = 12;
    ListMSC[0].loc.cellY = 34;
    ListMSC[0].loc.color = ORANGE;
    ListMSC[0].numberofBTS = 3;
    ListMSC[0].ue_connects = 0;
    for (int i = 0; i < 3; i++)
        ListMSC[0].resp[i] = ListBTS[i];

    ListMSC[1].loc.cellX = 35;
    ListMSC[1].loc.cellY = 62;
    ListMSC[1].loc.color = ORANGE;
    ListMSC[1].numberofBTS = 3;
    ListMSC[1].ue_connects = 0;
    for (int i = 0; i < 3; i++)
        ListMSC[1].resp[i] = ListBTS[i + 3];
}

// draw the rectangles for msc indication
void drawMSC()
{
    DrawText("MSC #1", cell_to_pixel(ListMSC[0].loc).pointX - 45, cell_to_pixel(ListMSC[0].loc).pointY + 6, 7, MAROON);
    DrawText("MSC #2", cell_to_pixel(ListMSC[1].loc).pointX - 45, cell_to_pixel(ListMSC[1].loc).pointY + 9, 7, MAROON);
    DrawRectangleGradientH(cell_to_pixel(ListMSC[0].loc).pointX, cell_to_pixel(ListMSC[0].loc).pointY + 2, 3 * lineGap, 2 * lineGap, ORANGE, MAROON);
    DrawRectangleGradientV(cell_to_pixel(ListMSC[1].loc).pointX, cell_to_pixel(ListMSC[1].loc).pointY, 2 * lineGap - 2, 3 * lineGap, ORANGE, MAROON);
}

// connect the two msc's currently
void connectMSC()
{
    DrawLineEx((Vector2){cell_to_pixel(ListMSC[0].loc).pointX + 3 * lineGap, cell_to_pixel(ListMSC[0].loc).pointY + 2 * lineGap}, (Vector2){cell_to_pixel(ListMSC[1].loc).pointX, cell_to_pixel(ListMSC[1].loc).pointY}, 2.5, ORANGE);
}

// connect ue to msc *no longer applicable
void connectUEtoMSC(Cell ue, MSC msc)
{
    DrawLine(cell_to_pixel(ue).pointX, cell_to_pixel(ue).pointY, cell_to_pixel(msc.loc).pointX + lineGap, cell_to_pixel(msc.loc).pointY, PURPLE);
}

// connect bts to msc for inter-msc connections
void connectBTStoMSC(BTS bts, MSC msc)
{
    DrawLine(cell_to_pixel(bts.loc).pointX, cell_to_pixel(bts.loc).pointY, cell_to_pixel(msc.loc).pointX + lineGap, cell_to_pixel(msc.loc).pointY, PURPLE);
}
int factorial(int n)
{
    if (n == 0)
        return 1;
    int factorial = 1;
    for (int i = 2; i <= n; i++)
        factorial = factorial * i;
    return factorial;
}

int nCr(int n, int r)
{
    return factorial(n) / (factorial(r) * factorial(n - r));
}

float calcConn(int a, int b)
{
    int connections = a + b;
    int ans = (connections * 100) / connectLimit;
    // printf("%d \n", connections);
    return ans;
}

void drawConnectivity()
{
    // printf("0 : %d, 1 : %d \n", ListMSC[0].ue_connects, ListMSC[1].ue_connects);
    int val = calcConn(ListMSC[0].ue_connects, ListMSC[1].ue_connects);
    if (ListMSC[0].ue_connects == 0 && ListMSC[1].ue_connects == 0)
        val = 0;
    // printf("%d\n", val);
    char buf[sizeof(int) * 3 + 2];
    snprintf(buf, sizeof buf, "%d", val);
    // printf("%d -- \n", val);
    DrawText(buf, 265, 500, 10, WHITE);
}

void getValueMSC()
{
    ListMSC[0].ue_connects = 0;
    ListMSC[1].ue_connects = 0;
    for (int i = 0; i < dynamic_UE_count; i++)
    {
        if (state[i][0].mscId == 0)
            ListMSC[0].ue_connects++;
        else if (state[i][0].mscId == 1)
            ListMSC[1].ue_connects++;
    }
    drawConnectivity();
}

void getValueMSC2()
{
    int connections = 0;
    Node *current = list->head;
    while (current != NULL)
    {
        int uex = current->data.x;
        int uey = current->data.y;

        if (state[uex - 1][0].mscId != state[uey - 1][0].mscId)
        {
            connections += 1;
        }
        current = current->next;
    }
    int ans = (connections * 100) / connectLimit;
    char buf[sizeof(int) * 3 + 2];
    snprintf(buf, sizeof buf, "%d %%", ans);
    DrawText(buf, 250, 475, 10, YELLOW);
}

// hard coded points for msc boundaries
void drawMscDemarc()
{
    Cell cells[4];

    // values are random, don't want to include data science in this
    // making boundary for MSC1
    cells[0].cellX = 0;
    cells[0].cellY = 34;
    cells[0].color = WHITE;

    cells[1].cellX = 37;
    cells[1].cellY = 34;
    cells[1].color = WHITE;

    cells[2].cellX = 63;
    cells[2].cellY = 19;
    cells[2].color = WHITE;

    cells[3].cellX = 63;
    cells[3].cellY = 0;
    cells[3].color = WHITE;

    for (int i = 0; i < 3; i++)
    {
        DrawLineEx((Vector2){cell_to_pixel(cells[i]).pointX, cell_to_pixel(cells[i]).pointY}, (Vector2){cell_to_pixel(cells[i + 1]).pointX, cell_to_pixel(cells[i + 1]).pointY}, 1.5, WHITE);
    }

    // making boundary for MSC2
    cells[0].cellX = 37;
    cells[0].cellY = 69;
    cells[0].color = WHITE;

    cells[1].cellX = 37;
    cells[1].cellY = 40;
    cells[1].color = WHITE;

    cells[2].cellX = 57;
    cells[2].cellY = 34;
    cells[2].color = WHITE;

    cells[3].cellX = 84;
    cells[3].cellY = 38;
    cells[3].color = WHITE;

    for (int i = 0; i < 3; i++)
    {
        // mark(cells[i]);
        DrawLineEx((Vector2){cell_to_pixel(cells[i]).pointX, cell_to_pixel(cells[i]).pointY}, (Vector2){cell_to_pixel(cells[i + 1]).pointX, cell_to_pixel(cells[i + 1]).pointY}, 1.5, WHITE);
    }
}

// used when two ue's are connected
void connectBTStoBTS(Cell BTS1, Cell BTS2)
{
    Point point_bts1 = cell_to_pixel(BTS1);
    Point point_bts2 = cell_to_pixel(BTS2);
    DrawLine(point_bts1.pointX, point_bts1.pointY, point_bts2.pointX, point_bts2.pointY, PURPLE);
}

// get the nearest bts to a ue
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

int reqAuthBTS(BTS bts, Cell ue)
{
    if (bts.btsISP == ue.nodeIsp)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void handle_sigint(int sig)
{
    char *answer;
    scanf("Do you wish to exit? \n");
    scanf("%s", &answer);
    if (strcmp(answer, "yes") == 0)
    {
        printf("Exit success\n");
        exit(0);
    }
    else
    {
        printf("Fine");
        return;
    }
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
            printf("Enter ID of UE's u wish to connect : ");
            scanf("%d %d", &ue1_g, &ue2_g);
            int check = 0;
            if (!(ue1_g > 0 && ue1_g < dynamic_UE_count + 1))
            {
                printf("Incorrect ID for UE 1\n");
                check = 1;
            }
            if (!(ue2_g > 0 && ue2_g < dynamic_UE_count + 1))
            {
                printf("Incorrect ID for UE 2\n");
                check = 1;
            }
            if (!check)
            {
                struct pair_int_int ue_connect_data;
                ue_connect_data.x = ue1_g;
                ue_connect_data.y = ue2_g;
                add_node(list, ue_connect_data);
            }
            sem_post(mysemp);
        }
        else if (strcmp(str, "recharge") == 0)
        {
            sem_wait(mysemp);
            printf("Enter EID of UE you wish to recharge and duration(1-100) : ");
            scanf("%d %d", &ue_x, &ue_recharge);
            if (!(ue_x > 0 && ue_x < dynamic_UE_count + 1))
            {
                printf("Invalid UE ID\n");
            }
            if (!(ue_recharge > 1 && ue_recharge < 100))
            {
                printf("Invalid recharge amt\n");
            }
            sem_post(mysemp);
        }

        else if (strcmp(str, "update") == 0)
        {
            sem_wait(mysemp);
            int choice;
            printf("Select an option:\n");
            printf("\t1. UE\n");
            printf("\t2. BTS\n");
            scanf("%d", &choice);
            if (!(choice > 1 && choice < 2))
            {
                printf("Incorrect choice");
            }
            else
            {
                if (choice == 1)
                {
                    int ue;
                    printf("Enter UE ID\n");
                    scanf("%d", &ue);
                    if (!(ue > 0 && ue < dynamic_UE_count + 1))
                    {
                        printf("Invalid UE ID\n");
                    }
                    else
                    {
                        int x, y;
                        printf("Enter x, y\n");
                        scanf("%d %d", &x, &y);
                        if (!(x > horizontal_sep && x < screenWidth - lineGap))
                        {
                            printf("Out of bounds X value\n");
                        }
                        else if (!(y > vertical_sep && y < screenHeight - lineGap))
                        {
                            printf("Out of bounds Y value\n");
                        }
                        state[ue - 1][0].cellX = x;
                        state[ue - 1][0].cellY = y;
                    }
                }
                else if (choice == 2)
                {
                    int bts;
                    printf("Enter BTS ID\n");
                    scanf("%d", &bts);
                    if (!(bts > 0 && bts < total_BTS + 1))
                    {
                        printf("Invalid BTS ID\n");
                    }
                    else
                    {
                        int x, y, rad;
                        printf("Enter x, y and radius\n");
                        scanf("%d %d %d", &x, &y, &rad);
                        if (!(x > horizontal_sep && x < screenWidth - lineGap))
                        {
                            printf("Out of bounds X value\n");
                        }
                        else if (!(y > vertical_sep && y < screenHeight - lineGap))
                        {
                            printf("Out of bounds Y value\n");
                        }
                        if (!(rad > 0 && rad < 150))
                        {
                            printf("Out of bounds Radius value\n");
                        }
                        ListBTS[bts].loc.cellX = x;
                        ListBTS[bts].loc.cellY = y;
                        ListBTS[bts].radius = rad;
                        ListBTS[bts].number_of_UE = 0;
                    }
                }
            }
            sem_post(mysemp);
        }
        else if (strcmp(str, "pause") == 0)
        {
            sem_wait(mysemp);
            // printf("here 3\n");
            pause_x = 1;
            sem_post(mysemp);
        }
        else if (strcmp(str, "resume") == 0)
        {
            sem_wait(mysemp);

            pause_x = 0;
            sem_post(mysemp);
        }
        // else if (strcmp(str, "exit") == 0)
        // {
        //     sem_wait(mysemp);
        //     printf("here exit\n");
        //     signal(SIGINT, handle_sigint);
        //     sem_post(mysemp);
        // }
        else if (strcmp(str, "help") == 0)
        {
            sem_wait(mysemp);

            char *resp = "\nNET-SIM\n\n"

                         "Options:\n"
                         "\n"
                         "      help     Show this screen.\n\n"
                         "      pause    Pause the simulation.\n"
                         "      resume   Continue the sim\n"
                         "      connect  Connect two UE's\n"
                         "      recharge Recharge a UE\n"
                         "      update   Update params of UE/BTS\n";
            printf("%s", resp);
            sem_post(mysemp);
        }
    }
}

int main(void)
{
#ifdef _WIN32
    printf("This is Windows.\n");
#elif __linux__
    printf("This is Linux.\n");
#elif __APPLE__ || __MACH__
    printf("This is macOS.\n");
#else
    printf("This is some other operating system.\n");
#endif

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
    // sqlite3 *DB;
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
                          "INSERT OR IGNORE INTO UE VALUES('A001', 4, 15, 15);"
                          "INSERT OR IGNORE INTO UE VALUES('A002', 5, 20, 20);";

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
    char *sql_insert_hlr = "INSERT OR IGNORE INTO HLR VALUES('V001', 20, 10, 'VERIZON', 1, 10000, 1);"
                           "INSERT OR IGNORE INTO HLR VALUES('C001', 5,  5,  'COMCAST', 1, 20000, 2);"
                           "INSERT OR IGNORE INTO HLR VALUES('V002', 10, 10, 'VERIZOM', 0, 30000, 0);"
                           "INSERT OR IGNORE INTO HLR VALUES('A001', 15, 15, 'ATnT',    1, 40000, 1);"
                           "INSERT OR IGNORE INTO HLR VALUES('A002', 20, 20, 'ATnT',    1, 40000, 1);";

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

    /*
    FILE *file;
    char buffer[28];
    int count;

    file = fopen("temp.txt", "r");
    if (file == NULL)
        return 1;

    count = fread(buffer, 1, sizeof(buffer), file);
    int reti = regcomp(&regex, "^[\\w\\.]+@([\\w-]+\\.)+[\\w-]{2,4}$", 0);
    if (reti)
    {
        for (int i = 0; i < MAX_INPUT_CHARS - 9; i++)
        {
            printf("%d : %d", MAX_INPUT_CHARS, screenHeight);
            printf("\n");
        }
    }
    reti = regexec(&regex, buffer, 0, NULL, 0);
    if (buffer[15] != '\x40')
        return 1;
    if (reti == 1)
    {
        int currScreen = 0;
    }
    else
    {
        int currFrame = 0;
    }

    regfree(&regex);

    fclose(file);
    */

    populateEID();
    SetTraceLogCallback(CustomLog);

    // if you want dynamic scaling
    // SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    InitWindow(screenWidth, screenHeight, "Net-Sim");

    // these don't work during resize and only when the resize has completed
    // InitWindow(GetScreenWidth(), GetScreenHeight(), "Net-Sim");
    // SetWindowPosition(50, 100);
    SetTargetFPS(targetfpsSim);

    Texture2D background = LoadTexture("assets/cyberpunk_street_background.png");
    Texture2D midground = LoadTexture("assets/cyberpunk_street_midground.png");
    Texture2D foreground = LoadTexture("assets/cyberpunk_street_foreground.png");

    float scrollingBack = 0.0f;
    float scrollingMid = 0.0f;
    float scrollingFore = 0.0f;

    int cdire[total_UE];

    char *placeholder1 = "Network-Sim";

    // Cell state[4];
    Color stateColor[trail_bits] = {WHITE,
                                    LIGHTGRAY,
                                    GRAY,
                                    DARKGRAY};

    list = create_list();
    // Man data 1
    int startLoc[total_UE][2] = {{10, 10}, {20, 20}, {30, 30}, {40, 40}, {50, 50}};
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
            state[i][j].nodeIsp = UE_ISP[i];
            state[i][j].flag = 1;
            state[i][j].id = i;
            state[i][j].mscId = -1;
            state[i][j].roaming = -1;
        }
    }

    // NEED TO IMPROVE THIS

    // Man data 2
    int BTS_Data[total_BTS][4] = {{15, 15, 75, 0}, {35, 20, 120, 0}, {50, 10, 90, 0}, {50, 45, 80, 0}, {70, 50, 130, 0}, {55, 60, 80, 0}};
    Color BTS_Color[total_BTS] = {BLUE, GREEN, RED, SKYBLUE, GOLD, PURPLE};

    for (int i = 0; i < sizeof(BTS_Data) / sizeof(BTS_Data[0]); i++)
    {
        // for (int j = 0; j < 4; j++)
        // {
        ListBTS[i].loc.cellX = BTS_Data[i][0];
        ListBTS[i].loc.cellY = BTS_Data[i][1];
        ListBTS[i].radius = BTS_Data[i][2];
        ListBTS[i].number_of_UE = BTS_Data[i][3];
        ListBTS[i].loc.color = BTS_Color[i];
        ListBTS[i].loc.id = i;
        ListBTS[i].btsISP = BTS_ISP[i];
        // }
    }

    char inUE[MAX_INPUT_CHARS + 1] = "\0"; // NOTE: One extra space required for null terminator char '\0'
    int letterCount = 0;
    int framesCounter = 0;

    int currScreen = 0;
    int currFrame = 0;
    int terminalMarker = 0;

    // activate when cursor in box
    bool mouseOnText = false;

    // activate when click
    bool mouseClick = false;

    // bool connect_1_3 = false;

    Rectangle textBox = {60, 60, 80, 30};

    while (!WindowShouldClose())
    {
        // menu screen - non-display
        if (currScreen == 0)
        {
            if (terminalMarker == 0)
            {
                system("clear");
                printf("------WELCOME------\n");
                char *resp = "\nNET-SIM\n\n"

                             "Options:\n"
                             "\n"
                             "      help     Show this screen.\n\n"
                             "      pause    Pause the simulation.\n"
                             "      resume   Continue the sim\n"
                             "      connect  Connect two UE's\n"
                             "      recharge Recharge a UE\n"
                             "      update   Update params of UE/BTS\n";
                printf("%s", resp);
                printf("\n\n\n\n\n");
                terminalMarker++;
            }
            // SetTargetFPS(targetfpsMenu);

            scrollingBack -= 0.1f;
            scrollingMid -= 0.5f;
            scrollingFore -= 1.0f;

            // NOTE: Texture is scaled twice its size, so it sould be considered on scrolling
            if (scrollingBack <= -background.width * 2)
            {
                scrollingBack = 0;
            }
            if (scrollingMid <= -midground.width * 2)
            {
                scrollingMid = 0;
            }
            if (scrollingFore <= -foreground.width * 2)
            {
                scrollingFore = 0;
            }
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
            // SetTargetFPS(targetfpsSim);

            // [part of a deprecated implementation]
            // if (IsKeyPressed(KEY_A) && IsKeyPressed(KEY_B))
            // {
            //     connect_1_3 = true;
            // }
            // if (IsKeyReleased(KEY_A) && IsKeyReleased(KEY_B))
            // {
            //     connect_1_3 = false;
            // }

            // the 5th ue or indicated by 4, is user movable
            // 1 cell a move
            if (IsKeyDown(KEY_UP))
            {
                if (state[4][0].cellY >= 1)
                {
                    state[4][0] = update(state[4][0], 0);
                }
            }
            else if (IsKeyDown(KEY_DOWN))
            {
                if (state[4][0].cellY <= 67)
                {
                    state[4][0] = update(state[4][0], 2);
                }
            }
            else if (IsKeyDown(KEY_RIGHT))
            {
                if (state[4][0].cellX <= 84)
                {
                    state[4][0] = update(state[4][0], 1);
                }
            }
            else if (IsKeyDown(KEY_LEFT))
            {
                if (state[4][0].cellX >= 0)
                {
                    state[4][0] = update(state[4][0], 3);
                }
            }

            // move same but just 3 moves together for more speed
            if (IsKeyDown(KEY_W))
            {
                if (state[4][0].cellY >= 1)
                {
                    for (int i = 0; i < 3; i++)
                    {
                        state[4][0] = update(state[4][0], 0);
                    }
                }
            }
            else if (IsKeyDown(KEY_S))
            {
                if (state[4][0].cellY <= 67)
                {
                    for (int i = 0; i < 3; i++)
                    {
                        state[4][0] = update(state[4][0], 2);
                    }
                }
            }
            else if (IsKeyDown(KEY_D))
            {
                if (state[4][0].cellX <= 84)
                {
                    for (int i = 0; i < 3; i++)
                    {
                        state[4][0] = update(state[4][0], 1);
                    }
                }
            }
            else if (IsKeyDown(KEY_A))
            {
                if (state[4][0].cellX >= 0)
                {
                    for (int i = 0; i < 3; i++)
                    {
                        state[4][0] = update(state[4][0], 3);
                    }
                }
            }

            frame_tracker++;
        }

        BeginDrawing();

        // menu screen - display
        if (currScreen == 0)
        {
            ClearBackground(GetColor(0x052c46ff));
            // DrawText("WELCOME!!!", 20, 20, 15, WHITE);

            // Draw background image twice
            // NOTE: Texture is scaled twice its size
            DrawTextureEx(background, (Vector2){scrollingBack, 225}, 0.0f, 2.0f, WHITE);
            DrawTextureEx(background, (Vector2){background.width * 2 + scrollingBack, 225}, 0.0f, 2.0f, WHITE);

            // Draw midground image twice
            DrawTextureEx(midground, (Vector2){scrollingMid, 330}, 0.0f, 2.0f, WHITE);
            DrawTextureEx(midground, (Vector2){midground.width * 2 + scrollingMid, 330}, 0.0f, 2.0f, WHITE);

            // Draw foreground image twice
            DrawTextureEx(foreground, (Vector2){scrollingFore, 420}, 0.0f, 2.0f, WHITE);
            DrawTextureEx(foreground, (Vector2){foreground.width * 2 + scrollingFore, 420}, 0.0f, 2.0f, WHITE);

            DrawText("NET - SIM", 40, 40, 20, RED);
            DrawText(" Sarthak Jha (@prince-aegon)", screenWidth - 200, screenHeight - 20, 10, RAYWHITE);

            // uncomment for box
            // DrawRectangleRec(textBox, LIGHTGRAY);
            // if (mouseClick)
            // {
            //     DrawRectangleLines((int)textBox.x, (int)textBox.y, (int)textBox.width, (int)textBox.height, RED);
            // }
            // else
            // {
            //     DrawRectangleLines((int)textBox.x, (int)textBox.y, (int)textBox.width, (int)textBox.height, DARKGRAY);
            // }

            // uncomment for text

            //  DrawText(inUE, (int)textBox.x + 5, (int)textBox.y + 8, 5, BLACK);
            //  DrawText("#BTS : ", (int)textBox.x - 40, (int)textBox.y + 10, 5, WHITE);

            DrawText("PRESS 'ENTER' TO START", 40, 100, 13, MAROON);

            // DrawText(TextFormat("INPUT CHARS: %i/%i", letterCount, MAX_INPUT_CHARS), 315, 250, 20, DARKGRAY);

            // if (mouseClick)
            // {
            //     if (letterCount < MAX_INPUT_CHARS)
            //     {
            // Draw blinking underscore char

            // uncomment for flashing textbar
            // if (((framesCounter / 10) % 2) == 0)
            //     DrawText(" |", (int)textBox.x + 4 + MeasureText(inUE, 5), (int)textBox.y + 8, 5, MAROON);

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
                // dynamic_UE_count = atoi(inUE);

                currScreen = 1;
            }
        }

        // sim screen - display
        else if (currScreen == 1)
        {
            // printf("connect 1_3 : %d \n", connect_1_3);

            ClearBackground(BLACK);

            DrawText(placeholder1, 10, 10, 5, WHITE);

            DrawText("Number of UE's", 10, 20, 5, WHITE);
            DrawText(TextFormat("%d", ListBTS[0].number_of_UE), 10, 35, 10, BLUE);
            DrawText(TextFormat("%d", ListBTS[1].number_of_UE), 10, 45, 10, GREEN);
            DrawText(TextFormat("%d", ListBTS[2].number_of_UE), 10, 55, 10, RED);

            DrawText(TextFormat("%d", ListBTS[3].number_of_UE), 20, 35, 10, SKYBLUE);
            DrawText(TextFormat("%d", ListBTS[4].number_of_UE), 20, 45, 10, GOLD);
            DrawText(TextFormat("%d", ListBTS[5].number_of_UE), 20, 55, 10, PURPLE);
            DrawText(TextFormat("Network Rate : %0.f", getConnectionRate()), screenWidth / 2, 10, 10, WHITE);
            DrawText(TextFormat("FPS : %d", GetFPS()), screenWidth * 0.75, 10, 10, WHITE);

            // display live and pending connections
            DrawText("Connections", 10, 75, 5, WHITE);

            int count_ll = 0;
            Node *current_display = list->head;
            while (current_display != NULL)
            {
                int ue1x = current_display->data.x;
                int ue1y = current_display->data.y;
                if (state[ue1x - 1][0].connection == -1 || state[ue1y - 1][0].connection == -1)
                    DrawText(TextFormat("%d, %d\n", current_display->data.x, current_display->data.y), 10, 85 + 10 * count_ll, 10, RED);
                else
                    DrawText(TextFormat("%d, %d\n", current_display->data.x, current_display->data.y), 10, 85 + 10 * count_ll, 10, GREEN);

                count_ll++;
                current_display = current_display->next;
            }

            // int k = atoi(inUE);
            // k = k + 100;
            // char buf[sizeof(int) * 3 + 2];
            // snprintf(buf, sizeof buf, "%d", k);

            DrawText(inUE, 10, 75, 10, WHITE);

            drawBTS();
            /*
            3 UI Modes:
                1.

            */
            createGrid(1);
            drawMscDemarc();
            initMSC();
            drawMSC();
            connectMSC();

            // getValueMSC();
            getValueMSC2();

            // stop the movement of ue if mouse on top
            for (int i = 0; i < dynamic_UE_count; i++)
            {
                int check = 0;
                for (int j = 0; j < trail_bits; j++)
                {
                    Rectangle rec;
                    rec.x = cell_to_pixel(state[i][j]).pointX;
                    rec.y = cell_to_pixel(state[i][j]).pointY;
                    rec.height = lineGap;
                    rec.width = lineGap;
                    if (CheckCollisionPointRec(GetMousePosition(), rec))
                    {
                        check = 1;
                        break;
                    }
                }
                if (check == 1)
                {
                    state[i][0].flag = 0;
                }
                else
                {
                    if (i != 4)
                    {
                        state[i][0].flag = 1;
                    }
                }
            }

            if (pause_x == 1)
            {
                // printf("here 1\n");
                for (int i = 0; i < dynamic_UE_count; i++)
                {
                    state[i][0].flag = 0;
                    // printf("%d %d", i, state[i][0].flag);
                }
            }
            else
            {
                // printf("here 2\n")s;
                for (int i = 0; i < dynamic_UE_count; i++)
                {
                    if (i != 4)
                    {
                        state[i][0].flag = 1;
                    }
                }
            }

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
                char *eid = sqlite3_column_text(stmt, 0);
                int validity = sqlite3_column_int(stmt, 5);
                int status = sqlite3_column_int(stmt, 4);
                int index = -1;
                for (int i = 0; i < dynamic_UE_count; i++)
                {
                    if (strcmp(EIDMapping[i].value, eid) == 0)
                    {
                        index = EIDMapping[i].key;
                    }
                }
                if (state[index][0].roaming == 1)
                {
                    DrawText(TextFormat("%s (%s) : %d", eid, UE_ISP_DIS[index], validity), 10, 160 + i * 12, 10, RED);
                }
                else if (state[index][0].roaming == 0)
                {
                    DrawText(TextFormat("%s (%s) : %d", eid, UE_ISP_DIS[index], validity), 10, 160 + i * 12, 10, GREEN);
                }
                else
                {
                    DrawText(TextFormat("%s (%s) : %d", eid, UE_ISP_DIS[index], validity), 10, 160 + i * 12, 10, WHITE);
                }
                // printf("id=%d \n", id);
                if (status == -1)
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

                /*

                at all times, we have three states:
                1. WHITE -> Not connected to any BTS
                2. GREEN -> Connected to Home BTS
                3. RED   -> Connected to Visitor BTS

                */
                state[i][0].roaming = -1;

                if (ue_to_bts.y > -1)
                {
                    if (connectPermission[i])
                    {
                        int auth = reqAuthBTS(ListBTS[ue_to_bts.y], state[i][0]);
                        if (auth == 1)
                        {
                            state[i][0].roaming = 0;
                        }
                        else
                        {
                            state[i][0].roaming = 1;
                        }
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

                /*

                Based on connection, we have different deductions:
                1. Outside Range   -> 0.2
                2. Home            -> 1.0
                3. Visitor/Roaming -> 5.0

                */
                if (state[i][0].roaming == 0)
                {
                    sprintf(sql_validity_update, "UPDATE HLR SET VALIDITY=VALIDITY-1 WHERE EID = '%s' AND VALIDITY > 0 ",
                            // validity_update, EIDMapping[i].value, validity_update);
                            EIDMapping[i].value);
                }
                else if (state[i][0].roaming == 1)
                {
                    sprintf(sql_validity_update, "UPDATE HLR SET VALIDITY=VALIDITY-5 WHERE EID = '%s' AND VALIDITY > 0 ",
                            // validity_update, EIDMapping[i].value, validity_update);
                            EIDMapping[i].value);
                }
                else if (state[i][0].roaming == -1)
                {
                    sprintf(sql_validity_update, "UPDATE HLR SET VALIDITY=VALIDITY-0.2 WHERE EID = '%s' AND VALIDITY > 0 ",
                            // validity_update, EIDMapping[i].value, validity_update);
                            EIDMapping[i].value);
                }

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

            /*

            we have a linked list which store's the desired ue's to connect

            we iterate the list and categorise each pair into 3 types:
                1. invalid connection      -> don't handle and rm from list
                2. successful connection   -> connect and hold the connection for the time duration
                3. unsuccessful connection -> retry on next iter and jump to 2 when success

            if the algo asks a node to be removed we store in an array and remove after the algo is done

            */

            // if (ue1 > -1 && ue2 > -1)

            // temp node for iteration, we only require only position for deletion
            // pointer to node is not required
            Node *current = list->head;

            // position for traverse and rem for rm
            int position = 0, rem = 0;
            int remConnections[10];

            while (current != NULL)
            {
                struct pair_int_int ue_connect_data;
                ue_connect_data = current->data;

                int ue1 = ue_connect_data.x, ue2 = ue_connect_data.y;

                if (ue1 > -1 && ue2 > -1)
                {
                    // check if either of the device has an invalid subscription
                    if (!connectPermission[ue1 - 1] || !connectPermission[ue2 - 1])
                    {
                        printf("Unauthorized access \n");
                        if (!connectPermission[ue1 - 1])
                        {
                            printf("UE with EID : %s has invalid subscription\n", EIDMapping[ue1 - 1].value);
                        }
                        else
                        {
                            printf("UE with EID : %s has invalid subscription\n", EIDMapping[ue2 - 1].value);
                        }
                        ue1 = -1;
                        ue2 = -1;
                    }

                    // check if both devices are connected
                    else if (state[ue1 - 1][0].connection > -1 && state[ue2 - 1][0].connection > -1)
                    {

                        int ue1_msc = 0, ue2_msc = 0;
                        for (int msc = 0; msc < total_MSC; msc++)
                        {
                            for (int bts = 0; bts < ListMSC[msc].numberofBTS; bts++)
                            {
                                if (state[ue1 - 1][0].connection == (ListMSC[msc].resp)[bts].loc.id)
                                {
                                    ue1_msc = msc;
                                    state[ue1 - 1][0].mscId = ue1_msc;
                                }
                                if (state[ue2 - 1][0].connection == (ListMSC[msc].resp)[bts].loc.id)
                                {
                                    ue2_msc = msc;
                                    state[ue2 - 1][0].mscId = ue2_msc;
                                }
                            }
                        }

                        if (ue1_msc != ue2_msc)
                        {
                            connectUEtoBTS(state[ue1 - 1][0], ListBTS[state[ue1 - 1][0].connection].loc, 1);
                            connectUEtoBTS(state[ue2 - 1][0], ListBTS[state[ue2 - 1][0].connection].loc, 1);
                            connectBTStoMSC(ListBTS[state[ue1 - 1][0].connection], ListMSC[ue1_msc]);
                            connectBTStoMSC(ListBTS[state[ue2 - 1][0].connection], ListMSC[ue2_msc]);
                            // connectUEtoMSC(state[ue1 - 1][0], ListMSC[ue1_msc]);
                            // connectUEtoMSC(state[ue2 - 1][0], ListMSC[ue2_msc]);
                            // printf("%d, %d\n", ue1_msc, ue2_msc);
                            // ListMSC[ue1_msc].ue_connects++;
                            // ListMSC[ue2_msc].ue_connects++;
                        }

                        else
                        {
                            connectBTStoBTS(ListBTS[state[ue1 - 1][0].connection].loc, ListBTS[state[ue2 - 1][0].connection].loc);
                            connectUEtoBTS(state[ue1 - 1][0], ListBTS[state[ue1 - 1][0].connection].loc, 1);
                            connectUEtoBTS(state[ue2 - 1][0], ListBTS[state[ue2 - 1][0].connection].loc, 1);
                        }
                        if (frame_tracker - currFrame == 1)
                        {
                            // printProgress(1);
                            // printf("Connection Established... \n");
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
                                // printf("Disconnecting... \n");
                                state[ue1 - 1][0].flag = 1;
                                state[ue2 - 1][0].flag = 1;
                                state[ue1 - 1][0].mscId = -1;

                                state[ue2 - 1][0].mscId = -1;
                                currFrame = 0;
                                ue1 = -1;
                                ue2 = -1;
                                progress = 0;
                                // ListMSC[ue1_msc].ue_connects--;
                                // ListMSC[ue2_msc].ue_connects--;

                                // printf("Disconnected successfully... \n");
                            }
                        }
                    }

                    // else run the progress bar
                    else
                    {
                        // need to find a solution for progress bar
                        // float value = get_progress(progress);
                        // printProgress(value);
                        // progress++;
                        // fflush(stdout);
                    }
                }
                if (ue1 == -1 && ue2 == -1)
                {
                    remConnections[rem] = position;
                    rem++;
                }
                position++;
                current = current->next;
            }
            for (int i = 0; i < rem; i++)
            {
                delete_node(list, remConnections[i]);
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
                    // printf("Success!! \n");
                }
                // printf("Recharge successful for %d on EID %s \n", ue_recharge, EIDMapping[ue_x - 1].value);
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
                int ans = 1;

                // iterate throgh the nodes
                Node *current = list->head;

                while (current != NULL)
                {
                    // if current nodes either value is there flip the flag
                    if (current->data.x == i + 1 || current->data.y == i + 1)
                    {
                        ans = 0;
                    }
                    current = current->next;
                }
                if (ans == 1)
                    connectUEtoBTS(state[i][0], ListBTS[state[i][0].connection].loc, 0);
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
                // the 4th id is for user movable
                if (i != 4)
                {
                    cdire[i] = newDirection(state[i][0], cdire[i]);
                }
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
                // if (state[i][0].flag == 1)

                // the 4th id is for user movable
                if (i != 4)
                {
                    state[i][0] = update(state[i][0], cdire[i]);
                }
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

    // De-Initialization
    //--------------------------------------------------------------------------------------
    UnloadTexture(background); // Unload background texture
    UnloadTexture(midground);  // Unload midground texture
    UnloadTexture(foreground); // Unload foreground texture

    CloseWindow();
    system("clear");

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
