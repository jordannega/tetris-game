#include <raylib.h>
#include <array>
#include <algorithm>
#include <random>
#include <cstring>
using namespace std;

enum Block { BarBlock, BoxBlock, TBlock, LBlock, JBlock, ZBlock, SBlock };
enum Orientation { Up, Right, Down, Left };

const int BOARD_WIDTH  = 300;
const int BOARD_HEIGHT = 600;
const int ROWS = 20;
const int COLS = 10;
const int CELL_WIDTH  = BOARD_WIDTH / COLS;
const int CELL_HEIGHT = BOARD_HEIGHT / ROWS;
const int INFO_AREA_WIDTH = 250;

const Color WINDOW_BG_COLOR    = WHITE;
const Color GRID_LINE_COLOR    = LIGHTGRAY;

// Color per block type (indexed by Block enum)
const Color BLOCK_COLORS[7] = { SKYBLUE, YELLOW, PURPLE, ORANGE, BLUE, GREEN, RED };

// ---------- Piece shape tables ----------
struct Cell { int dx, dy; };

// PIECES[block][orientation][cellIndex] -> relative cell
const Cell PIECES[7][4][4] = {
    // BarBlock
    {{{0,0},{0,1},{0,2},{0,3}},
     {{0,0},{1,0},{2,0},{3,0}},
     {{0,0},{0,1},{0,2},{0,3}},
     {{0,0},{1,0},{2,0},{3,0}}},

    // BoxBlock
    {{{0,0},{1,0},{0,1},{1,1}},
     {{0,0},{1,0},{0,1},{1,1}},
     {{0,0},{1,0},{0,1},{1,1}},
     {{0,0},{1,0},{0,1},{1,1}}},

    // TBlock (origin = center of 3-wide bar, stem goes down)
    {{{-1,0},{0,0},{1,0},{0,1}},
     {{0,-1},{0,0},{0,1},{1,0}},
     {{-1,0},{0,0},{1,0},{0,-1}},
     {{0,-1},{0,0},{0,1},{-1,0}}},

    // LBlock
    {{{0,-1},{0,0},{0,1},{1,1}},
     {{-1,0},{0,0},{1,0},{1,-1}},
     {{-1,-1},{0,-1},{0,0},{0,1}},
     {{-1,1},{-1,0},{0,0},{1,0}}},

    // JBlock
    {{{0,-1},{0,0},{0,1},{-1,1}},
     {{-1,-1},{-1,0},{0,0},{1,0}},
     {{1,-1},{0,-1},{0,0},{0,1}},
     {{-1,0},{0,0},{1,0},{1,1}}},

    // ZBlock
    {{{-1,0},{0,0},{0,1},{1,1}},
     {{1,0},{1,1},{0,1},{0,2}},
     {{-1,0},{0,0},{0,1},{1,1}},
     {{1,0},{1,1},{0,1},{0,2}}},

    // SBlock
    {{{1,0},{0,0},{0,1},{-1,1}},
     {{1,0},{1,1},{0,1},{0,2}},
     {{1,0},{0,0},{0,1},{-1,1}},
     {{1,0},{1,1},{0,1},{0,2}}}
};

// SRS wall-kick offsets for rotations (Up->Right, Right->Down, Down->Left, Left->Up)
const int KICKS[4][5][2] = {
    {{0,0},{-1,0},{-1,-1},{0,2},{-1,2}},   // Up -> Right
    {{0,0},{1,0},{1,1},{0,-2},{1,-2}},     // Right -> Down
    {{0,0},{1,0},{1,-1},{0,2},{1,2}},      // Down -> Left
    {{0,0},{-1,0},{-1,1},{0,-2},{-1,-2}}   // Left -> Up
};

// ---------- Game state ----------
struct ActiveBlock {
    Block block;
    Orientation orientation;
    int x;
    int y;
    Color color;
};

int  cellInfo [ROWS][COLS];   // 1 if occupied
int  cellColor[ROWS][COLS];   // Block enum (valid only if occupied)

int  score = 0;
int  level = 1;
bool gameOver = false;
bool paused   = false;

Block nextBlock;
Block heldBlock;
bool  hasHeld = false;

// 7-bag randomizer
std::array<Block, 7> bag;
int bagIndex = 7;

// ---------- Forward declarations ----------
void init();
void initCells();
Block chooseRandomBlock();
void  spawnBlock(ActiveBlock &ab);
void  resetGame(ActiveBlock &ab);

bool isValidPosition(ActiveBlock ab);
bool canBlockGoDown (ActiveBlock ab);
bool canBlockGoLeft (ActiveBlock ab);
bool canBlockGoRight(ActiveBlock ab);
void tryRotate      (ActiveBlock &ab);

void lockActiveBlock(ActiveBlock ab);
int  clearFullRows();
bool isRowFull(int row);
void clearRow(int row);
void moveRowsDown(int row, int rowsCleared);
bool isGameOver();

void drawGrid();
void drawLockedCells();
void drawScore();
void drawNextBlock();
void drawHeldBlock();
void drawActiveBlock(ActiveBlock ab);
void drawGhostBlock(ActiveBlock ab);
void drawPieceAt(Block block, Orientation ori, int px, int py, Color color, int cellW, int cellH);

void instantDrop(ActiveBlock &ab);
void playGame(ActiveBlock &ab);

// ---------- Helpers ----------
int findMiddle(Block block, Orientation ori) {
    // Return a spawn x such that piece stays roughly centered.
    // Using the piece table: find min/max dx of orientation 0 (Up)
    int minDx = 0, maxDx = 0;
    for (auto &c : PIECES[block][ori]) {
        if (c.dx < minDx) minDx = c.dx;
        if (c.dx > maxDx) maxDx = c.dx;
    }
    int width = maxDx - minDx + 1;
    return (COLS - width) / 2 - minDx;
}

// ---------- Randomizer ----------
Block chooseRandomBlock() {
    if (bagIndex >= 7) {
        bag = { BarBlock, BoxBlock, TBlock, LBlock, JBlock, ZBlock, SBlock };
        static std::mt19937 g(std::random_device{}());
        std::shuffle(bag.begin(), bag.end(), g);
        bagIndex = 0;
    }
    return bag[bagIndex++];
}

// ---------- Init / reset ----------
void init() {
    InitWindow(BOARD_WIDTH + INFO_AREA_WIDTH, BOARD_HEIGHT, "TETRIS");
    SetTargetFPS(60);
}

void initCells() {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++) {
            cellInfo[r][c]  = 0;
            cellColor[r][c] = -1;
        }
}

void spawnBlock(ActiveBlock &ab) {
    ab.block       = nextBlock;
    nextBlock      = chooseRandomBlock();
    ab.orientation = Up;
    ab.x           = findMiddle(ab.block, Up);
    ab.y           = 0;
    ab.color       = BLOCK_COLORS[ab.block];
}

void resetGame(ActiveBlock &ab) {
    initCells();
    score = 0;
    level = 1;
    gameOver = false;
    paused   = false;
    hasHeld  = false;
    bagIndex = 7;
    nextBlock = chooseRandomBlock();
    spawnBlock(ab);
}

// ---------- Collision / movement ----------
bool isValidPosition(ActiveBlock ab) {
    for (auto &c : PIECES[ab.block][ab.orientation]) {
        int nx = ab.x + c.dx;
        int ny = ab.y + c.dy;
        if (nx < 0 || nx >= COLS) return false;
        if (ny < 0 || ny >= ROWS) return false;
        if (cellInfo[ny][nx])     return false;
    }
    return true;
}

bool canBlockGoDown(ActiveBlock ab) {
    for (auto &c : PIECES[ab.block][ab.orientation]) {
        int nx = ab.x + c.dx;
        int ny = ab.y + c.dy + 1;
        if (ny >= ROWS)        return false;
        if (cellInfo[ny][nx])  return false;
    }
    return true;
}

bool canBlockGoLeft(ActiveBlock ab) {
    for (auto &c : PIECES[ab.block][ab.orientation]) {
        int nx = ab.x + c.dx - 1;
        if (nx < 0) return false;
        if (cellInfo[ab.y + c.dy][nx]) return false;
    }
    return true;
}

bool canBlockGoRight(ActiveBlock ab) {
    for (auto &c : PIECES[ab.block][ab.orientation]) {
        int nx = ab.x + c.dx + 1;
        if (nx >= COLS) return false;
        if (cellInfo[ab.y + c.dy][nx]) return false;
    }
    return true;
}

void tryRotate(ActiveBlock &ab) {
    if (ab.block == BoxBlock) return;

    Orientation newOri = Orientation((ab.orientation + 1) % 4);

    for (int k = 0; k < 5; k++) {
        ActiveBlock test = ab;
        test.orientation = newOri;
        test.x = ab.x + KICKS[ab.orientation][k][0];
        test.y = ab.y + KICKS[ab.orientation][k][1];

        if (isValidPosition(test)) {
            ab = test;
            return;
        }
    }
}

// ---------- Locking / row clearing ----------
void lockActiveBlock(ActiveBlock ab) {
    for (auto &c : PIECES[ab.block][ab.orientation]) {
        int nx = ab.x + c.dx;
        int ny = ab.y + c.dy;
        if (ny >= 0 && ny < ROWS && nx >= 0 && nx < COLS) {
            cellInfo [ny][nx] = 1;
            cellColor[ny][nx] = ab.block;
        }
    }
}

bool isRowFull(int row) {
    for (int c = 0; c < COLS; c++)
        if (cellInfo[row][c] == 0) return false;
    return true;
}

void clearRow(int row) {
    for (int c = 0; c < COLS; c++) {
        cellInfo [row][c] = 0;
        cellColor[row][c] = -1;
    }
}

void moveRowsDown(int row, int rowsCleared) {
    int dst = row + rowsCleared;
    if (dst >= ROWS) return;
    for (int c = 0; c < COLS; c++) {
        cellInfo [dst][c] = cellInfo [row][c];
        cellColor[dst][c] = cellColor[row][c];
        cellInfo [row][c] = 0;
        cellColor[row][c] = -1;
    }
}

int clearFullRows() {
    int completed = 0;
    for (int row = ROWS - 1; row >= 0; row--) {
        if (isRowFull(row)) {
            clearRow(row);
            completed++;
        } else if (completed > 0) {
            moveRowsDown(row, completed);
        }
    }
    return completed;
}

bool isGameOver() {
    for (int c = 0; c < COLS; c++)
        if (cellInfo[0][c] == 1) return true;
    return false;
}

// ---------- Drawing ----------
void drawGrid() {
    for (int row = 0; row <= ROWS; row++)
        DrawLine(0, row * CELL_HEIGHT, BOARD_WIDTH, row * CELL_HEIGHT, GRID_LINE_COLOR);
    for (int col = 0; col <= COLS; col++)
        DrawLine(col * CELL_WIDTH, 0, col * CELL_WIDTH, BOARD_HEIGHT, GRID_LINE_COLOR);
}

void drawLockedCells() {
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (cellInfo[r][c])
                DrawRectangle(c * CELL_WIDTH, r * CELL_HEIGHT,
                              CELL_WIDTH, CELL_HEIGHT,
                              BLOCK_COLORS[cellColor[r][c]]);
}

void drawScore() {
    DrawText("SCORE", BOARD_WIDTH + 40, 30, 25, BLACK);
    DrawText(TextFormat("%i", score), BOARD_WIDTH + 40, 65, 30, RED);

    DrawText("LEVEL", BOARD_WIDTH + 40, 110, 25, BLACK);
    DrawText(TextFormat("%i", level), BOARD_WIDTH + 40, 145, 30, MAROON);
}

// Draw an arbitrary piece shape at a given pixel origin using a given cell size
void drawPieceAt(Block block, Orientation ori, int px, int py,
                 Color color, int cellW, int cellH) {
    int minDx = 0, minDy = 0;
    for (auto &c : PIECES[block][ori]) {
        if (c.dx < minDx) minDx = c.dx;
        if (c.dy < minDy) minDy = c.dy;
    }
    for (auto &c : PIECES[block][ori]) {
        int cx = px + (c.dx - minDx) * cellW;
        int cy = py + (c.dy - minDy) * cellH;
        DrawRectangle(cx, cy, cellW, cellH, color);
    }
}

void drawNextBlock() {
    DrawText("NEXT", BOARD_WIDTH + 40, 200, 25, BLACK);
    drawPieceAt(nextBlock, Up, BOARD_WIDTH + 60, 240, BLOCK_COLORS[nextBlock],
                CELL_WIDTH, CELL_HEIGHT);
}

void drawHeldBlock() {
    DrawText("HOLD", BOARD_WIDTH + 40, 380, 25, BLACK);
    if (hasHeld) {
        drawPieceAt(heldBlock, Up, BOARD_WIDTH + 60, 420, BLOCK_COLORS[heldBlock],
                    CELL_WIDTH, CELL_HEIGHT);
    }
}

void drawActiveBlock(ActiveBlock ab) {
    for (auto &c : PIECES[ab.block][ab.orientation]) {
        int cx = (ab.x + c.dx) * CELL_WIDTH;
        int cy = (ab.y + c.dy) * CELL_HEIGHT;
        DrawRectangle(cx, cy, CELL_WIDTH, CELL_HEIGHT, ab.color);
        DrawRectangleLines(cx, cy, CELL_WIDTH, CELL_HEIGHT, BLACK);
    }
}

void drawGhostBlock(ActiveBlock ab) {
    if (ab.block == BoxBlock && false) {} // keep simple
    ActiveBlock ghost = ab;
    while (canBlockGoDown(ghost)) ghost.y++;

    Color gc = ab.color;
    gc.a = 90;
    for (auto &c : PIECES[ghost.block][ghost.orientation]) {
        int cx = (ghost.x + c.dx) * CELL_WIDTH;
        int cy = (ghost.y + c.dy) * CELL_HEIGHT;
        DrawRectangle(cx, cy, CELL_WIDTH, CELL_HEIGHT, gc);
        DrawRectangleLines(cx, cy, CELL_WIDTH, CELL_HEIGHT, Fade(BLACK, 0.4f));
    }
}

// ---------- Drop / game loop ----------
void instantDrop(ActiveBlock &ab) {
    int dropped = 0;
    while (canBlockGoDown(ab)) { ab.y++; dropped++; }
    score += dropped * 2;

    lockActiveBlock(ab);

    if (isGameOver()) { gameOver = true; return; }

    int rowsCleared = clearFullRows();
    if (rowsCleared == 1) score += 100;
    if (rowsCleared == 2) score += 300;
    if (rowsCleared == 3) score += 500;
    if (rowsCleared >= 4) score += 800;

    // level progression
    int newLevel = 1 + score / 2000;
    if (newLevel > level) level = newLevel;

    spawnBlock(ab);
}

void playGame(ActiveBlock &ab) {
    // ---- Drawing ----
    drawGrid();
    drawLockedCells();
    drawScore();
    drawNextBlock();
    drawHeldBlock();

    if (gameOver) {
        DrawText("GAME OVER", 20, BOARD_HEIGHT / 2 - 40, 40, RED);
        DrawText("Press R to restart", 30, BOARD_HEIGHT / 2 + 10, 20, DARKGRAY);
        return;
    }

    if (paused) {
        DrawText("PAUSED", 60, BOARD_HEIGHT / 2 - 30, 40, DARKGRAY);
        return;
    }

    drawGhostBlock(ab);
    drawActiveBlock(ab);

    // ---- Gravity ----
    float gameSpeed = 0.6f - 0.05f * (level - 1);
    if (gameSpeed < 0.05f) gameSpeed = 0.05f;

    static float fallDelay = 0;
    fallDelay += GetFrameTime();

    if (fallDelay > gameSpeed) {
        if (canBlockGoDown(ab)) {
            ab.y++;
        } else {
            lockActiveBlock(ab);
            if (isGameOver()) { gameOver = true; return; }

            int rowsCleared = clearFullRows();
            if (rowsCleared == 1) score += 100;
            if (rowsCleared == 2) score += 300;
            if (rowsCleared == 3) score += 500;
            if (rowsCleared >= 4) score += 800;

            int newLevel = 1 + score / 2000;
            if (newLevel > level) level = newLevel;

            spawnBlock(ab);
        }
        fallDelay = 0;
    }

    // ---- Input ----
    if (IsKeyPressed(KEY_LEFT)  && canBlockGoLeft(ab))  ab.x--;
    if (IsKeyPressed(KEY_RIGHT) && canBlockGoRight(ab)) ab.x++;

    if (IsKeyPressed(KEY_UP)) tryRotate(ab);

    if (IsKeyPressed(KEY_SPACE)) { instantDrop(ab); return; }

    if (IsKeyPressed(KEY_C)) {
        if (!hasHeld) {
            heldBlock = ab.block;
            hasHeld   = true;
            spawnBlock(ab);
        } else {
            Block tmp    = heldBlock;
            heldBlock    = ab.block;
            ab.block     = tmp;
            ab.color     = BLOCK_COLORS[ab.block];
            ab.orientation = Up;
            ab.x = findMiddle(ab.block, Up);
            ab.y = 0;
        }
    }
}

// ---------- Main ----------
int main() {
    init();
    initCells();

    ActiveBlock activeBlock;
    nextBlock = chooseRandomBlock();
    spawnBlock(activeBlock);

    while (!WindowShouldClose()) {
        // Global keys
        if (IsKeyPressed(KEY_P) && !gameOver) paused = !paused;
        if (IsKeyPressed(KEY_R) && gameOver) resetGame(activeBlock);

        BeginDrawing();
            ClearBackground(WINDOW_BG_COLOR);
            playGame(activeBlock);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}cp
