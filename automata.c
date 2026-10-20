#include "raylib.h"
#include <stdlib.h>
#include <time.h>

#define CELL_SIZE 6
#define MAX_COLS 400
#define MAX_ROWS 240

int cols, rows;
int screenWidth, screenHeight;

unsigned char world[MAX_ROWS][MAX_COLS];
unsigned char nextWorld[MAX_ROWS][MAX_COLS];

unsigned char age[MAX_ROWS][MAX_COLS];
unsigned char nextAge[MAX_ROWS][MAX_COLS];

unsigned char deadCells[MAX_ROWS][MAX_COLS];

int generation = 0;
int population = 0;
int births = 0;
int deaths = 0;

int paused = 0;
int autoSeed = 0;

float updateSpeed = 0.10f;
float timer = 0.0f;

int wrapX(int x)
{
    if (x < 0)
        return cols - 1;

    if (x >= cols)
        return 0;

    return x;
}

int wrapY(int y)
{
    if (y < 0)
        return rows - 1;

    if (y >= rows)
        return 0;

    return y;
}

void clearWorld(void)
{
    for (int y = 0; y < rows; y++)
    {
        for (int x = 0; x < cols; x++)
        {
            world[y][x] = 0;
            nextWorld[y][x] = 0;

            age[y][x] = 0;
            nextAge[y][x] = 0;

            deadCells[y][x] = 0;
        }
    }

    generation = 0;
    population = 0;
    births = 0;
    deaths = 0;
}

void countPopulation(void)
{
    population = 0;

    for (int y = 0; y < rows; y++)
    {
        for (int x = 0; x < cols; x++)
        {
            if (world[y][x])
                population++;
        }
    }
}

void randomizeWorld(void)
{
    clearWorld();

    for (int y = 0; y < rows; y++)
    {
        for (int x = 0; x < cols; x++)
        {
            if (rand() % 100 < 22)
            {
                world[y][x] = 1;
                age[y][x] = 1;
            }
        }
    }

    countPopulation();
}

void spawnGlider(int x, int y)
{
    int glider[5][2] = {
        {1, 0},
        {2, 1},
        {0, 2},
        {1, 2},
        {2, 2}
    };

    for (int i = 0; i < 5; i++)
    {
        int cellX = wrapX(x + glider[i][0]);
        int cellY = wrapY(y + glider[i][1]);

        world[cellY][cellX] = 1;
        age[cellY][cellX] = 1;
        deadCells[cellY][cellX] = 0;
    }

    countPopulation();
}

void handleMouse(void)
{
    Vector2 mouse = GetMousePosition();

    int x = (int)mouse.x / CELL_SIZE;
    int y = (int)mouse.y / CELL_SIZE;

    if (x < 0 || x >= cols || y < 0 || y >= rows)
        return;

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
    {
        world[y][x] = 1;
        age[y][x] = 1;
        deadCells[y][x] = 0;
    }

    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
    {
        world[y][x] = 0;
        age[y][x] = 0;
        deadCells[y][x] = 0;
    }
}

int countNeighbours(int x, int y)
{
    int count = 0;

    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            if (dx == 0 && dy == 0)
                continue;

            int nx = wrapX(x + dx);
            int ny = wrapY(y + dy);

            count += world[ny][nx];
        }
    }

    return count;
}

void updateWorld(void)
{
    births = 0;
    deaths = 0;

    for (int y = 0; y < rows; y++)
    {
        for (int x = 0; x < cols; x++)
        {
            int neighbours = countNeighbours(x, y);

            if (world[y][x])
            {
                if (neighbours == 2 || neighbours == 3)
                {
                    nextWorld[y][x] = 1;

                    if (age[y][x] < 30)
                        nextAge[y][x] = age[y][x] + 1;
                    else
                        nextAge[y][x] = 30;
                }
                else
                {
                    nextWorld[y][x] = 0;
                    nextAge[y][x] = 0;

                    deadCells[y][x] = 3;
                    deaths++;
                }
            }
            else
            {
                if (neighbours == 3)
                {
                    nextWorld[y][x] = 1;
                    nextAge[y][x] = 1;

                    deadCells[y][x] = 0;
                    births++;
                }
                else
                {
                    nextWorld[y][x] = 0;
                    nextAge[y][x] = 0;
                }
            }
        }
    }

    for (int y = 0; y < rows; y++)
    {
        for (int x = 0; x < cols; x++)
        {
            world[y][x] = nextWorld[y][x];
            age[y][x] = nextAge[y][x];

            if (deadCells[y][x] > 0 && !world[y][x])
                deadCells[y][x]--;
        }
    }

    generation++;
    countPopulation();

    if (autoSeed && population < 15)
    {
        for (int i = 0; i < 8; i++)
            spawnGlider(rand() % cols, rand() % rows);
    }
}

void drawWorld(void)
{
    ClearBackground(RAYWHITE);

    for (int y = 0; y < rows; y++)
    {
        for (int x = 0; x < cols; x++)
        {
            int drawX = x * CELL_SIZE;
            int drawY = y * CELL_SIZE;

            if (world[y][x])
            {
                int shade = 8 + age[y][x] * 2;

                if (shade > 65)
                    shade = 65;

                Color cellColor = {
                    (unsigned char)shade,
                    (unsigned char)shade,
                    (unsigned char)shade,
                    255
                };

                DrawRectangle(
                    drawX,
                    drawY,
                    CELL_SIZE - 1,
                    CELL_SIZE - 1,
                    cellColor
                );
            }
            else if (deadCells[y][x] > 0)
            {
                DrawRectangle(
                    drawX,
                    drawY,
                    CELL_SIZE - 1,
                    CELL_SIZE - 1,
                    (Color){165, 165, 165, 255}
                );
            }
        }
    }

    DrawRectangle(0, 0, 450, 75, RAYWHITE);

    DrawText("CELLULAR LIFE", 12, 8, 20, BLACK);

    DrawText(
        TextFormat("GENERATION: %d", generation),
        12, 35, 14, DARKGRAY
    );

    DrawText(
        TextFormat("POPULATION: %d", population),
        190, 35, 14, DARKGRAY
    );

    DrawText(
        TextFormat("BIRTHS: %d  DEATHS: %d", births, deaths),
        12, 55, 12, DARKGRAY
    );

    if (paused)
        DrawText("PAUSED", 340, 35, 14, RED);

    if (autoSeed)
        DrawText("AUTO-SEED", 340, 55, 12, DARKGRAY);

    DrawRectangle(
        0,
        screenHeight - 25,
        screenWidth,
        25,
        RAYWHITE
    );

    DrawText(
        "LEFT CLICK: DRAW | RIGHT CLICK: ERASE | SPACE: PAUSE | "
        "N: STEP | R: RANDOM | C: CLEAR | 1: GLIDER | "
        "A: AUTO-SEED | UP/DOWN: SPEED | ESC: EXIT",
        10,
        screenHeight - 19,
        10,
        DARKGRAY
    );
}

int main(void)
{
    srand((unsigned int)time(NULL));

    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(1920, 1080, "Cellular Life");

    if (!IsWindowReady())
        return 1;

    SetTargetFPS(60);

    screenWidth = GetScreenWidth();
    screenHeight = GetScreenHeight();

    cols = screenWidth / CELL_SIZE;
    rows = screenHeight / CELL_SIZE;

    if (cols > MAX_COLS)
        cols = MAX_COLS;

    if (rows > MAX_ROWS)
        rows = MAX_ROWS;

    randomizeWorld();

    while (!WindowShouldClose())
    {
        handleMouse();

        if (IsKeyPressed(KEY_ESCAPE))
            break;

        if (IsKeyPressed(KEY_SPACE))
            paused = !paused;

        if (IsKeyPressed(KEY_R))
            randomizeWorld();

        if (IsKeyPressed(KEY_C))
            clearWorld();

        if (IsKeyPressed(KEY_N))
            updateWorld();

        if (IsKeyPressed(KEY_ONE))
        {
            Vector2 mouse = GetMousePosition();

            spawnGlider(
                (int)mouse.x / CELL_SIZE,
                (int)mouse.y / CELL_SIZE
            );
        }

        if (IsKeyPressed(KEY_A))
            autoSeed = !autoSeed;

        if (IsKeyPressed(KEY_UP))
        {
            updateSpeed *= 0.8f;

            if (updateSpeed < 0.01f)
                updateSpeed = 0.01f;
        }

        if (IsKeyPressed(KEY_DOWN))
        {
            updateSpeed *= 1.25f;

            if (updateSpeed > 1.0f)
                updateSpeed = 1.0f;
        }

        if (!paused)
        {
            timer += GetFrameTime();

            if (timer >= updateSpeed)
            {
                updateWorld();
                timer = 0.0f;
            }
        }

        BeginDrawing();
        drawWorld();
        EndDrawing();
    }

    CloseWindow();

    return 0;
}