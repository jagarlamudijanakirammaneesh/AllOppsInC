#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define CELL_SIZE 16
#define TEAM_SIZE 600
#define MAX_GENERATION 5000

#define EMPTY 0
#define RED 1
#define BLUE 2
#define GREEN 3

typedef struct {
    int team;
    int runner;
    float health;
    float aggression;
    float fear;
    float pride;
} Cell;

static Cell *grid, *nextGrid;

static int cols, rows;
static int generation;
static int population[4];

static int paused;
static int gameOver;
static int winner;
static int speed = 1;

static int suddenDeath;
static int suddenDeathTicks;

static int finalAssault;
static int weakTeam;
static int strongTeam;
static int runnerAssignmentDone;

static float accumulator;

/* --------------------------------------------------
   COLORS AND GRID
-------------------------------------------------- */

static Color teamColor(int team)
{
    if (team == RED) return (Color){240, 65, 75, 255};
    if (team == BLUE) return (Color){65, 145, 255, 255};
    if (team == GREEN) return (Color){70, 220, 120, 255};

    return (Color){12, 14, 20, 255};
}

static int indexOf(int x, int y)
{
    return y * cols + x;
}

static int valid(int x, int y)
{
    return x >= 0 && x < cols && y >= 0 && y < rows;
}

/* --------------------------------------------------
   CELL CREATION
-------------------------------------------------- */

static Cell createCell(int team)
{
    Cell c = {0};

    c.team = team;

    if (team != EMPTY) {
        c.health = 100.0f;
        c.aggression = GetRandomValue(35, 85) / 100.0f;
        c.fear = GetRandomValue(10, 60) / 100.0f;
        c.pride = GetRandomValue(20, 80) / 100.0f;
        c.runner = 0;
    }

    return c;
}

/* --------------------------------------------------
   POPULATION STATISTICS
-------------------------------------------------- */

static void updatePopulation(void)
{
    population[RED] = 0;
    population[BLUE] = 0;
    population[GREEN] = 0;

    for (int i = 0; i < cols * rows; i++) {
        int team = grid[i].team;

        if (team >= RED && team <= GREEN)
            population[team]++;
    }
}

static int livingTeams(void)
{
    int count = 0;

    for (int t = RED; t <= GREEN; t++) {
        if (population[t] > 0)
            count++;
    }

    return count;
}

static int findWinner(void)
{
    if (population[RED] > 0 &&
        population[BLUE] == 0 &&
        population[GREEN] == 0)
        return RED;

    if (population[BLUE] > 0 &&
        population[RED] == 0 &&
        population[GREEN] == 0)
        return BLUE;

    if (population[GREEN] > 0 &&
        population[RED] == 0 &&
        population[BLUE] == 0)
        return GREEN;

    return EMPTY;
}

/* --------------------------------------------------
   FAIR STARTING FORMATION

   Red: upper-left
   Blue: upper-right
   Green: lower-center

   Each formation contains exactly 600 cells.
-------------------------------------------------- */

static void resetBattle(void)
{
    size_t total = (size_t)cols * rows;

    memset(grid, 0, sizeof(Cell) * total);
    memset(nextGrid, 0, sizeof(Cell) * total);

    int formationWidth = 30;
    int formationHeight = 20;

    float radiusX =
        (cols - formationWidth - 4) / 1.7320508f;

    float radiusY =
        (rows - formationHeight - 4) / 1.5f;

    float radius = fminf(radiusX, radiusY);

    int centerX = cols / 2;
    int centerY = rows / 2;

    int dx = (int)roundf(radius * 0.8660254f);
    int topOffset = (int)roundf(radius * 0.5f);
    int bottomOffset = (int)roundf(radius);

    int startX[4] = {
        0,
        centerX - dx - formationWidth / 2,
        centerX + dx - formationWidth / 2,
        centerX - formationWidth / 2
    };

    int startY[4] = {
        0,
        centerY - topOffset - formationHeight / 2,
        centerY - topOffset - formationHeight / 2,
        centerY + bottomOffset - formationHeight / 2
    };

    for (int team = RED; team <= GREEN; team++) {
        int sx = startX[team];
        int sy = startY[team];

        if (sx < 1) sx = 1;
        if (sy < 1) sy = 1;

        if (sx + formationWidth >= cols - 1)
            sx = cols - formationWidth - 2;

        if (sy + formationHeight >= rows - 1)
            sy = rows - formationHeight - 2;

        for (int y = 0; y < formationHeight; y++) {
            for (int x = 0; x < formationWidth; x++) {
                int gx = sx + x;
                int gy = sy + y;

                if (valid(gx, gy))
                    grid[indexOf(gx, gy)] = createCell(team);
            }
        }
    }

    generation = 0;
    paused = 0;
    gameOver = 0;
    winner = EMPTY;
    speed = 1;
    accumulator = 0.0f;

    suddenDeath = 0;
    suddenDeathTicks = 0;

    finalAssault = 0;
    weakTeam = 0;
    strongTeam = 0;
    runnerAssignmentDone = 0;

    updatePopulation();
}

/* --------------------------------------------------
   TEAM CENTERS
-------------------------------------------------- */

static void getCenters(int cx[4], int cy[4])
{
    long sumX[4] = {0};
    long sumY[4] = {0};
    int count[4] = {0};

    for (int i = 0; i < cols * rows; i++) {
        int team = grid[i].team;

        if (team == EMPTY)
            continue;

        sumX[team] += i % cols;
        sumY[team] += i / cols;
        count[team]++;
    }

    for (int t = RED; t <= GREEN; t++) {
        if (count[t] > 0) {
            cx[t] = (int)(sumX[t] / count[t]);
            cy[t] = (int)(sumY[t] / count[t]);
        } else {
            cx[t] = cols / 2;
            cy[t] = rows / 2;
        }
    }
}

/* --------------------------------------------------
   FINAL ASSAULT

   Triggers when only two teams remain and the weaker
   army has 50% or less of the stronger army's population.

   10% of the weaker army are assigned persistent runner
   roles. The remaining 90% fight.
-------------------------------------------------- */

static void updateFinalAssault(void)
{
    if (livingTeams() != 2)
        return;

    int first = 0;
    int second = 0;

    for (int t = RED; t <= GREEN; t++) {
        if (population[t] <= 0)
            continue;

        if (first == 0)
            first = t;
        else
            second = t;
    }

    if (first == 0 || second == 0)
        return;

    int weak = population[first] <= population[second]
               ? first : second;

    int strong = weak == first ? second : first;

    /* Updated threshold: 50%. */
    if (population[weak] > population[strong] * 0.50f)
        return;

    weakTeam = weak;
    strongTeam = strong;
    finalAssault = 1;

    if (!runnerAssignmentDone) {
        /* Updated runner percentage: 10%. */
        int needed =
            (int)ceilf(population[weak] * 0.10f);

        if (needed < 1)
            needed = 1;

        int total = cols * rows;
        int start = GetRandomValue(0, total - 1);

        for (int j = 0; j < total && needed > 0; j++) {
            int i = (start + j) % total;

            if (grid[i].team == weak) {
                grid[i].runner = 1;
                needed--;
            }
        }

        runnerAssignmentDone = 1;
    }
}

/* --------------------------------------------------
   FIND NEAREST ENEMY
-------------------------------------------------- */

static float nearestEnemyDistance(
    int x, int y, int team,
    int *enemyX, int *enemyY)
{
    float best = 1000000.0f;

    *enemyX = x;
    *enemyY = y;

    for (int radius = 1; radius <= 12; radius += 3) {
        int found = 0;

        for (int dy = -radius; dy <= radius; dy++) {
            for (int dx = -radius; dx <= radius; dx++) {
                int nx = x + dx;
                int ny = y + dy;

                if (!valid(nx, ny))
                    continue;

                int other = grid[indexOf(nx, ny)].team;

                if (other == EMPTY || other == team)
                    continue;

                float distance =
                    sqrtf((float)(dx * dx + dy * dy));

                if (distance < best) {
                    best = distance;
                    *enemyX = nx;
                    *enemyY = ny;
                    found = 1;
                }
            }
        }

        if (found)
            break;
    }

    return best;
}

/* --------------------------------------------------
   MOVEMENT

   Normal soldiers advance toward enemy armies.

   Strong army:
   - Pursues the weaker army.
   - Does not run away.

   Weak army:
   - 10% attempt to escape.
   - Remaining 90% advance and fight.
-------------------------------------------------- */

static void moveArmies(void)
{
    int total = cols * rows;

    int cx[4], cy[4];
    getCenters(cx, cy);

    int start = GetRandomValue(0, total - 1);
    int moves = total / 6;

    for (int n = 0; n < moves; n++) {
        int i = (start + n * 7919) % total;

        if (grid[i].team == EMPTY)
            continue;

        int team = grid[i].team;
        int x = i % cols;
        int y = i / cols;

        int isRunner =
            finalAssault &&
            team == weakTeam &&
            grid[i].runner;

        int targetX = cx[team];
        int targetY = cy[team];

        if (isRunner) {
            int enemyX, enemyY;

            nearestEnemyDistance(
                x, y, team, &enemyX, &enemyY
            );

            targetX = enemyX;
            targetY = enemyY;
        } else {
            int bestDistance = 1000000;

            for (int other = RED; other <= GREEN; other++) {
                if (other == team || population[other] == 0)
                    continue;

                int distance =
                    abs(cx[other] - x) +
                    abs(cy[other] - y);

                if (distance < bestDistance) {
                    bestDistance = distance;
                    targetX = cx[other];
                    targetY = cy[other];
                }
            }
        }

        int bestX = x;
        int bestY = y;

        float bestScore =
            isRunner ? -1.0e9f : 1.0e9f;

        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0)
                    continue;

                int nx = x + dx;
                int ny = y + dy;

                if (!valid(nx, ny))
                    continue;

                if (grid[indexOf(nx, ny)].team != EMPTY)
                    continue;

                int allies = 0;
                int enemies = 0;

                for (int yy = -2; yy <= 2; yy++) {
                    for (int xx = -2; xx <= 2; xx++) {
                        int ax = nx + xx;
                        int ay = ny + yy;

                        if (!valid(ax, ay))
                            continue;

                        int near = grid[indexOf(ax, ay)].team;

                        if (near == team)
                            allies++;
                        else if (near != EMPTY)
                            enemies++;
                    }
                }

                float oldDistance =
                    (float)(abs(targetX - x) +
                            abs(targetY - y));

                float newDistance =
                    (float)(abs(targetX - nx) +
                            abs(targetY - ny));

                float score;

                if (isRunner) {
                    /* Runners seek distance and open space. */
                    score = (newDistance - oldDistance) * 3.0f;
                    score -= enemies * 2.0f;
                    score += allies * 0.1f;

                    if (nx <= 1 || ny <= 1 ||
                        nx >= cols - 2 || ny >= rows - 2)
                        score -= 2.0f;

                    if (score > bestScore) {
                        bestScore = score;
                        bestX = nx;
                        bestY = ny;
                    }
                } else {
                    /* Fighters and pursuers move toward enemies. */
                    score = (newDistance - oldDistance) * 2.0f;
                    score -= enemies * 1.5f;
                    score += allies * 0.2f;

                    score += GetRandomValue(-10, 10) / 100.0f;

                    if (score < bestScore) {
                        bestScore = score;
                        bestX = nx;
                        bestY = ny;
                    }
                }
            }
        }

        if (bestX == x && bestY == y)
            continue;

        int chance = isRunner ? 95 : 65;

        if (finalAssault && team == strongTeam)
            chance = 95;

        if (suddenDeath)
            chance = 90;

        if (GetRandomValue(0, 99) >= chance)
            continue;

        int destination = indexOf(bestX, bestY);

        if (grid[destination].team == EMPTY &&
            grid[i].team == team) {

            grid[destination] = grid[i];
            grid[i] = createCell(EMPTY);
        }
    }
}

/* --------------------------------------------------
   COMBAT
-------------------------------------------------- */

static void combat(void)
{
    int total = cols * rows;

    memcpy(nextGrid, grid, sizeof(Cell) * (size_t)total);

    for (int i = 0; i < total; i++) {
        Cell attacker = grid[i];

        if (attacker.team == EMPTY)
            continue;

        /* Assigned runners focus on escaping. */
        if (finalAssault &&
            attacker.team == weakTeam &&
            attacker.runner)
            continue;

        int x = i % cols;
        int y = i / cols;

        int range = suddenDeath ? 2 : 1;
        int targets[24];
        int count = 0;

        for (int dy = -range; dy <= range; dy++) {
            for (int dx = -range; dx <= range; dx++) {
                if (dx == 0 && dy == 0)
                    continue;

                int nx = x + dx;
                int ny = y + dy;

                if (!valid(nx, ny))
                    continue;

                int ni = indexOf(nx, ny);

                if (grid[ni].team != EMPTY &&
                    grid[ni].team != attacker.team &&
                    count < 24) {

                    targets[count++] = ni;
                }
            }
        }

        if (count == 0)
            continue;

        int target =
            targets[GetRandomValue(0, count - 1)];

        float chance = 0.30f + attacker.aggression * 0.25f;

        if (finalAssault && attacker.team == strongTeam)
            chance += 0.18f;

        if (finalAssault &&
            attacker.team == weakTeam &&
            !attacker.runner)
            chance += 0.10f;

        if (suddenDeath)
            chance += 0.12f;

        if (chance > 0.90f)
            chance = 0.90f;

        if (GetRandomValue(0, 999) >=
            (int)(chance * 1000))
            continue;

        if (nextGrid[target].team == EMPTY ||
            nextGrid[target].team == attacker.team)
            continue;

        float damage =
            18.0f + attacker.aggression * 16.0f;

        if (finalAssault && attacker.team == strongTeam)
            damage += 10.0f;

        if (finalAssault &&
            attacker.team == weakTeam &&
            !attacker.runner)
            damage += 5.0f;

        nextGrid[target].health -= damage;

        if (nextGrid[target].health <= 0.0f)
            nextGrid[target] = createCell(EMPTY);
    }

    Cell *temp = grid;
    grid = nextGrid;
    nextGrid = temp;
}

/* --------------------------------------------------
   SUDDEN DEATH
-------------------------------------------------- */

static void closeArena(void)
{
    if (!suddenDeath)
        return;

    int margin = suddenDeathTicks / 150;

    int maximum =
        (cols < rows ? cols : rows) / 2 - 2;

    if (margin > maximum)
        margin = maximum;

    for (int y = 0; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            int edge = x;

            if (cols - 1 - x < edge)
                edge = cols - 1 - x;

            if (y < edge)
                edge = y;

            if (rows - 1 - y < edge)
                edge = rows - 1 - y;

            if (edge >= margin)
                continue;

            int i = indexOf(x, y);

            if (grid[i].team == EMPTY)
                continue;

            int nx = x;
            int ny = y;

            if (abs(cols / 2 - x) >= abs(rows / 2 - y))
                nx += (cols / 2 > x) - (cols / 2 < x);
            else
                ny += (rows / 2 > y) - (rows / 2 < y);

            if (valid(nx, ny) &&
                grid[indexOf(nx, ny)].team == EMPTY) {

                grid[indexOf(nx, ny)] = grid[i];
                grid[i] = createCell(EMPTY);
            }
        }
    }
}

/* --------------------------------------------------
   SIMULATION TICK
-------------------------------------------------- */

static void evolve(void)
{
    updatePopulation();
    updateFinalAssault();

    moveArmies();
    combat();

    updatePopulation();
    updateFinalAssault();

    generation++;

    if (generation >= MAX_GENERATION)
        suddenDeath = 1;

    if (suddenDeath) {
        suddenDeathTicks++;
        closeArena();
        updatePopulation();
    }

    if (livingTeams() <= 1) {
        winner = findWinner();
        gameOver = 1;
    }
}

/* --------------------------------------------------
   MANUAL CELL PLACEMENT
-------------------------------------------------- */

static void placeCell(int mouseX, int mouseY, int team)
{
    if (gameOver || suddenDeath)
        return;

    int x = mouseX / CELL_SIZE;
    int y = mouseY / CELL_SIZE;

    if (!valid(x, y))
        return;

    int i = indexOf(x, y);

    if (grid[i].team != EMPTY)
        return;

    if (population[team] >= TEAM_SIZE)
        return;

    grid[i] = createCell(team);

    updatePopulation();
}

/* --------------------------------------------------
   MAIN
-------------------------------------------------- */

int main(void)
{
    SetConfigFlags(FLAG_FULLSCREEN_MODE);
    InitWindow(0, 0, "Cellular Automata Battle");

    if (!IsWindowReady())
        return 1;

    SetTargetFPS(60);
    SetRandomSeed((unsigned int)GetTime());

    cols = GetScreenWidth() / CELL_SIZE;
    rows = GetScreenHeight() / CELL_SIZE;

    if (cols < 40 || rows < 25) {
        CloseWindow();
        return 1;
    }

    size_t total = (size_t)cols * rows;

    grid = calloc(total, sizeof(Cell));
    nextGrid = calloc(total, sizeof(Cell));

    if (!grid || !nextGrid) {
        free(grid);
        free(nextGrid);
        CloseWindow();
        return 1;
    }

    resetBattle();

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_ESCAPE))
            break;

        if (IsKeyPressed(KEY_SPACE) && !gameOver)
            paused = !paused;

        if (IsKeyPressed(KEY_R))
            resetBattle();

        if (IsKeyPressed(KEY_UP) && speed < 20)
            speed++;

        if (IsKeyPressed(KEY_DOWN) && speed > 1)
            speed--;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
            placeCell(GetMouseX(), GetMouseY(), RED);

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
            placeCell(GetMouseX(), GetMouseY(), BLUE);

        if (IsMouseButtonPressed(MOUSE_BUTTON_MIDDLE))
            placeCell(GetMouseX(), GetMouseY(), GREEN);

        if (!paused && !gameOver) {
            accumulator += GetFrameTime();

            float delay = 0.12f / speed;

            while (accumulator >= delay && !gameOver) {
                evolve();
                accumulator -= delay;
            }
        } else {
            accumulator = 0.0f;
        }

        BeginDrawing();
        ClearBackground((Color){12, 14, 20, 255});

        for (int y = 0; y < rows; y++) {
            for (int x = 0; x < cols; x++) {
                Cell c = grid[indexOf(x, y)];

                if (c.team == EMPTY)
                    continue;

                Color color = teamColor(c.team);

                if (c.health < 70.0f)
                    color = Fade(color, 0.85f);

                DrawRectangle(
                    x * CELL_SIZE,
                    y * CELL_SIZE,
                    CELL_SIZE - 1,
                    CELL_SIZE - 1,
                    color
                );

                /* White center = assigned runner. */
                if (finalAssault &&
                    c.team == weakTeam &&
                    c.runner) {

                    DrawRectangle(
                        x * CELL_SIZE + 5,
                        y * CELL_SIZE + 5,
                        5, 5,
                        RAYWHITE
                    );
                }
            }
        }

        DrawRectangle(
            12, 12, 530, 118,
            (Color){0, 0, 0, 210}
        );

        DrawText(
            TextFormat("GENERATION: %d", generation),
            24, 20, 20, RAYWHITE
        );

        DrawText(
            TextFormat("RED: %d", population[RED]),
            24, 48, 18, teamColor(RED)
        );

        DrawText(
            TextFormat("BLUE: %d", population[BLUE]),
            175, 48, 18, teamColor(BLUE)
        );

        DrawText(
            TextFormat("GREEN: %d", population[GREEN]),
            330, 48, 18, teamColor(GREEN)
        );

        DrawText(
            TextFormat(
                "SPEED: %d | SPACE pause | R restart | ESC quit",
                speed
            ),
            24, 77, 14, RAYWHITE
        );

        DrawText(
            "Equal starts | No regeneration | Sudden Death at 5000",
            24, 101, 13, RAYWHITE
        );

        if (finalAssault && !gameOver) {
            const char *message =
                weakTeam == RED
                ? "RED: 10% RUNNERS / 90% FIGHTERS"
                : weakTeam == BLUE
                ? "BLUE: 10% RUNNERS / 90% FIGHTERS"
                : "GREEN: 10% RUNNERS / 90% FIGHTERS";

            DrawRectangle(
                GetScreenWidth() / 2 - 270,
                140, 540, 34,
                (Color){100, 25, 25, 230}
            );

            DrawText(
                message,
                GetScreenWidth() / 2 -
                    MeasureText(message, 18) / 2,
                148, 18, RAYWHITE
            );
        }

        if (suddenDeath && !gameOver) {
            DrawRectangle(
                GetScreenWidth() / 2 - 240,
                12, 480, 40,
                (Color){145, 20, 20, 230}
            );

            DrawText(
                "SUDDEN DEATH - ARENA CLOSING",
                GetScreenWidth() / 2 - 215,
                22, 20, RAYWHITE
            );
        }

        if (paused && !gameOver) {
            DrawText(
                "PAUSED",
                GetScreenWidth() / 2 - 80,
                GetScreenHeight() / 2 - 30,
                40, RAYWHITE
            );
        }

        if (gameOver) {
            const char *message =
                winner == RED ? "RED WINS!" :
                winner == BLUE ? "BLUE WINS!" :
                winner == GREEN ? "GREEN WINS!" :
                "NO WINNER";

            int width = MeasureText(message, 54);

            DrawRectangle(
                GetScreenWidth() / 2 - width / 2 - 30,
                GetScreenHeight() / 2 - 60,
                width + 60, 120,
                (Color){0, 0, 0, 230}
            );

            DrawText(
                message,
                GetScreenWidth() / 2 - width / 2,
                GetScreenHeight() / 2 - 25,
                54,
                winner ? teamColor(winner) : RAYWHITE
            );

            DrawText(
                "Press R to restart",
                GetScreenWidth() / 2 - 100,
                GetScreenHeight() / 2 + 35,
                20, RAYWHITE
            );
        }

        EndDrawing();
    }

    free(grid);
    free(nextGrid);

    CloseWindow();
    return 0;
}