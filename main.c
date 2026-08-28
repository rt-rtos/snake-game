#ifndef _WIN32
    #define _DEFAULT_SOURCE
#endif

#include "colors.h"
#include <stdlib.h>
#include <time.h>
#ifdef _WIN32
    #define NOMINMAX
    #include <windows.h>
    #define usleep(x) Sleep((x)/1000)  // Convert microseconds to milliseconds
    #include "PDCurses/curses.h"  // Use local PDCurses headers
#else
    #include <unistd.h>
    #include <ncurses.h> // Linux/Mac
#endif


#define NOMINMAX
#define NO_MOUSE
#define ROWS 20
#define COLS 40
#define MAX_SNAKE_LEN (ROWS * COLS)
#define MAX_FOOD 5
#define MAX_TEMP_FOOD 3
#define TEMP_FOOD_DURATION 500  // frames before temp food disappears
#define DELAY 10000.0f  // microseconds per frame (lower = faster) - reduced for responsive input
#define MOVEMENT_FRAME_INTERVAL 10  // Snake moves every N frames
#define MAX_DEATH_ITEMS 4


// Directions
typedef enum {
    UP,
    DOWN,
    LEFT,
    RIGHT
} Direction;

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    Point pos;
    int timeLeft;
    //int blinkCounter;
    int foodType;    // 0=normal, 1=double points, 2=triple points
    char symbol;     // Different symbols for different types
} TempFood;

typedef struct {
    Point body[MAX_SNAKE_LEN];
    int length;
    int direction;
} Snake;

typedef struct {
    Point pos;
    int active;
    char symbol;
} SpecialItem;
// Initialize special item
SpecialItem specialItem = {{0, 0}, 0, '$'};

typedef struct {
    Point pos;
    int active;
    char symbol;
} DeathItem;
// Multiple death items
DeathItem deathItems[MAX_DEATH_ITEMS];

// Shrink potion - rare "get out of jail" item that trims the snake by
// SHRINK_AMOUNT segments (never below 3).
#define SHRINK_AMOUNT 3
typedef struct {
    Point pos;
    int active;
    char symbol;
} ShrinkPotion;
ShrinkPotion shrinkPotion = {{0, 0}, 0, '-'};

// Runtime config picked in the pre-game options screen.
typedef struct {
    int wraparound;   // 0 = walls kill, 1 = wrap around
    int speedIndex;   // 1..5, indexes into speedIntervals[]
    int useArrows;    // 0 = WASD, 1 = arrow keys
} Config;
Config config = {0, 3, 0};

// Movement interval per speed setting. Lower = snake steps more often.
// Index 0 unused so the numbers on screen line up with the array.
static const int speedIntervals[] = {0, 18, 14, 10, 7, 5};
static const char *speedNames[]   = {"", "Very Slow", "Slow", "Normal", "Fast", "Very Fast"};

Snake snake;
Point food[MAX_FOOD];
int foodCount = 0;
TempFood tempFood[MAX_TEMP_FOOD];
int tempFoodCount = 0;
int gameOver = 0;
int paused = 0;
int refreshCounter = 0;
int tempFoodSpawnCounter = 0;
int movementFrameCounter = 0;  // Counter for movement timing
int nextDirection = RIGHT;  // Buffer for next direction to prevent double-turns
int speedBoostTimer = 0;  // Timer for speed boost duration
int speedBoostActive = 0; // Flag for whether speed boost is active
WINDOW *gameWin = NULL;

// Function declarations
int checkCollision(Point next);
int checkSnakeOverlap(Point p);
void placeFood();
void resetGame();

void initSnake() {
    snake.length = 3;
    snake.direction = RIGHT;
    nextDirection = RIGHT;  // Initialize next direction
    // Clear the entire body array so future growth never picks up stale
    // positions left over from a previous game or previous larger length.
    for (int i = 0; i < MAX_SNAKE_LEN; i++) {
        snake.body[i].x = 0;
        snake.body[i].y = 0;
    }
    int startX = COLS / 2;
    int startY = ROWS / 2;
    for (int i = 0; i < snake.length; i++) {
        snake.body[i].x = startX - i;
        snake.body[i].y = startY;
    }
}

void resetGame() {
    // Reset all game state variables
    gameOver = 0;
    refreshCounter = 0;
    movementFrameCounter = 0;
    nextDirection = RIGHT;
    speedBoostTimer = 0;
    speedBoostActive = 0;
    foodCount = 0;
    tempFoodCount = 0;
    tempFoodSpawnCounter = 0;

    // Deactivate special items
    specialItem.active = 0;
    shrinkPotion.active = 0;
    paused = 0;

    // Reset death items
    for (int i = 0; i < MAX_DEATH_ITEMS; ++i) {
        deathItems[i].active = 0;
        deathItems[i].pos.x = 0;
        deathItems[i].pos.y = 0;
        deathItems[i].symbol = 'X';
    }
    
    // Clear input buffer to prevent stale inputs
    flushinp();

    // Clear all windows
    werase(gameWin);
    clear();
    
    // Reset snake
    initSnake();
    
    // Place initial food
    placeFood();
    
    // Refresh to show cleared state
    wrefresh(gameWin);
    refresh();
}

// True if p is occupied by the snake or any existing item on the board.
// Used by every spawner so nothing lands on top of anything else.
int cellIsOccupied(Point p) {
    if (checkSnakeOverlap(p)) return 1;
    for (int i = 0; i < foodCount; i++)
        if (food[i].x == p.x && food[i].y == p.y) return 1;
    for (int i = 0; i < tempFoodCount; i++)
        if (tempFood[i].pos.x == p.x && tempFood[i].pos.y == p.y) return 1;
    if (specialItem.active &&
        specialItem.pos.x == p.x && specialItem.pos.y == p.y) return 1;
    if (shrinkPotion.active &&
        shrinkPotion.pos.x == p.x && shrinkPotion.pos.y == p.y) return 1;
    for (int i = 0; i < MAX_DEATH_ITEMS; i++)
        if (deathItems[i].active &&
            deathItems[i].pos.x == p.x && deathItems[i].pos.y == p.y) return 1;
    return 0;
}

void placeFood() {
    if (foodCount >= MAX_FOOD) return;

    Point newPos;
    int attempts = 0;
    do {
        newPos.x = rand() % COLS;
        newPos.y = rand() % ROWS;
        attempts++;
    } while (cellIsOccupied(newPos) && attempts < 100);

    if (attempts < 100) {
        food[foodCount] = newPos;
        foodCount++;
    }
}

void placeTempFood() {
    if (tempFoodCount >= MAX_TEMP_FOOD) return;

    Point newPos;
    int attempts = 0;
    do {
        newPos.x = rand() % COLS;
        newPos.y = rand() % ROWS;
        attempts++;
    } while (cellIsOccupied(newPos) && attempts < 100);

    if (attempts >= 100) return;

    tempFood[tempFoodCount].pos = newPos;

    // Random type (70% normal, 20% double, 10% triple).
    // Triple uses '#' - 'X' is reserved for death items so the two never
    // share a glyph.
    int typeRoll = rand() % 100;
    if (typeRoll < 70) {
        tempFood[tempFoodCount].foodType = 0;
        tempFood[tempFoodCount].symbol = 'T';
        tempFood[tempFoodCount].timeLeft = 800;
    } else if (typeRoll < 90) {
        tempFood[tempFoodCount].foodType = 1;
        tempFood[tempFoodCount].symbol = 'D';
        tempFood[tempFoodCount].timeLeft = 600;
    } else {
        tempFood[tempFoodCount].foodType = 2;
        tempFood[tempFoodCount].symbol = '#';
        tempFood[tempFoodCount].timeLeft = 400;
    }

    tempFoodCount++;
}

int checkCollision(Point next) {
    if (next.x < 0 || next.x >= COLS || next.y < 0 || next.y >= ROWS)
        return 1;
    // Check collision with body segments (excluding head at index 0)
    for (int i = 1; i < snake.length; i++)
        if (snake.body[i].x == next.x && snake.body[i].y == next.y)
            return 1;
    return 0;
}

// Check if a point overlaps with any part of the snake (including head)
int checkSnakeOverlap(Point p) {
    for (int i = 0; i < snake.length; i++) {
        if (snake.body[i].x == p.x && snake.body[i].y == p.y)
            return 1;
    }
    return 0;
}

void moveSnake() {
    // Apply the buffered direction at the start of movement
    snake.direction = nextDirection;
    
    Point next = snake.body[0];
    switch (snake.direction) {
        case UP: next.y--; break;
        case DOWN: next.y++; break;
        case LEFT: next.x--; break;
        case RIGHT: next.x++; break;
    }

    // Wraparound: wrap before collision check so the wall test never fires.
    // Self-collision still applies against the wrapped cell.
    if (config.wraparound) {
        if (next.x < 0)      next.x = COLS - 1;
        else if (next.x >= COLS) next.x = 0;
        if (next.y < 0)      next.y = ROWS - 1;
        else if (next.y >= ROWS) next.y = 0;
    }
/* ------------------ COLLISION FUNCTIONS ------------------*/
    if (checkCollision(next)) {
        gameOver = 1;
        return;
    }

    // Check if snake ate any food
    int ateFood = 0;
    int foodIndex = -1;
    int ateTempFood = 0;
    int tempFoodIndex = -1;
    int ateSpecialItem = 0;
    
    // Check regular food
    for (int i = 0; i < foodCount; i++) {
        if (next.x == food[i].x && next.y == food[i].y) {
            ateFood = 1;
            foodIndex = i;
            break;
        }
    }
    
    // Check temporary food
    for (int i = 0; i < tempFoodCount; i++) {
        if (next.x == tempFood[i].pos.x && next.y == tempFood[i].pos.y) {
            ateTempFood = 1;
            tempFoodIndex = i;
            break;
        }
    }
    
    // Check special item
    if (specialItem.active && next.x == specialItem.pos.x && next.y == specialItem.pos.y) {
        ateSpecialItem = 1;
        specialItem.active = 0; // Deactivate special item
    } else {
        ateSpecialItem = 0;
    }

    // Check shrink potion
    int ateShrinkPotion = 0;
    if (shrinkPotion.active &&
        next.x == shrinkPotion.pos.x && next.y == shrinkPotion.pos.y) {
        ateShrinkPotion = 1;
        shrinkPotion.active = 0;
    }

    // Check death items collision BEFORE moving - CHECK ALL SLOTS
    for (int i = 0; i < MAX_DEATH_ITEMS; ++i) {
        if (deathItems[i].active && next.x == deathItems[i].pos.x && next.y == deathItems[i].pos.y) {
            gameOver = 1;
            return;
        }
    }

/* ------------------ MOVE SNAKE BODY ------------------*/
    // Remember the tail before shifting; if we grow this frame, new segments
    // are seeded at this cell so they trail out naturally instead of
    // rendering at whatever stale coordinates were sitting in body[].
    Point oldTail = snake.body[snake.length - 1];

    for (int i = snake.length - 1; i > 0; i--)
        snake.body[i] = snake.body[i - 1];
    snake.body[0] = next;
/* ------------------ END OF COLLISION FUNCTIONS ------------------*/

    int growthAmount = 0;

    if (ateFood) {
        for (int i = foodIndex; i < foodCount - 1; i++) {
            food[i] = food[i + 1];
        }
        foodCount--;

        growthAmount += speedBoostActive ? 2 : 1;
        placeFood();
    }

    if (ateTempFood) {
        TempFood eatenFood = tempFood[tempFoodIndex];

        for (int i = tempFoodIndex; i < tempFoodCount - 1; i++) {
            tempFood[i] = tempFood[i + 1];
        }
        tempFoodCount--;

        switch (eatenFood.foodType) {
            case 0:
                growthAmount += 2;
                placeFood();
                break;
            case 1:
                growthAmount += 4;
                placeFood();
                placeFood();
                break;
            case 2:
                growthAmount += 6;
                placeFood();
                placeFood();
                placeFood();
                break;
        }
    }

    if (ateSpecialItem) {
        growthAmount += 2;
        placeFood();
        speedBoostActive = 1;
        speedBoostTimer = 750;
    }

    // Grow: append the requested number of segments at the pre-shift tail.
    // They overlap for one frame and unstack as the snake moves - the same
    // way canonical Snake implementations handle growth.
    if (growthAmount > 0) {
        int newLength = snake.length + growthAmount;
        if (newLength > MAX_SNAKE_LEN) newLength = MAX_SNAKE_LEN;
        for (int i = snake.length; i < newLength; i++) {
            snake.body[i] = oldTail;
        }
        snake.length = newLength;
    }

    // Shrink: trim tail segments. Growth-then-seed will overwrite the
    // trimmed slots if the snake grows again, so no cleanup needed here.
    if (ateShrinkPotion) {
        snake.length -= SHRINK_AMOUNT;
        if (snake.length < 3) snake.length = 3;
    }
}
/* ------------------ END OF FUNCTIONS ------------------*/
/* ------------------ GAME WINDOW FUNCTIONS ------------------ */
void initBorder() {
    // Create border window once
    gameWin = newwin(ROWS + 2, COLS + 2, 0, 0);
    box(gameWin, 0, 0);
    wrefresh(gameWin);
    refresh(); // Add this to ensure it's displayed
}

void drawInstructions() {
    // Instructions panel on the right side of the game
    int instructX = COLS + 5; // Start 3 spaces after the game border
    int startY = 2;
    
    apply_text_color(NULL);
    mvprintw(startY, instructX, "=== SNAKE GAME ===");
    mvprintw(startY + 2, instructX, "Controls:");
    mvprintw(startY + 3, instructX, "W/A/S/D - Move");
    
    mvprintw(startY + 5, instructX, "Items:");
    mvprintw(startY + 6, instructX, "* - Food (+1, +2 w/boost)");
    mvprintw(startY + 7, instructX, "T - Temp Food (+2)");
    mvprintw(startY + 8, instructX, "D - Double Food (+4)");
    mvprintw(startY + 9, instructX, "# - Triple Food (+6)");
    mvprintw(startY + 10, instructX, "$ - Speed + Double Points");
    mvprintw(startY + 11, instructX, "- - Shrink Potion (-3)");

    mvprintw(startY + 13, instructX, "Avoid:");
    mvprintw(startY + 14, instructX, "X - Death Item");
    mvprintw(startY + 15, instructX, config.wraparound ? "Self only (walls wrap)" : "Walls & Self");
    mvprintw(startY + 17, instructX, "P - Pause | Q - Quit");
    
    remove_all_colors(NULL);
}

void drawBoard() {
    // 1. Border.
    apply_border_color(gameWin);
    box(gameWin, 0, 0);
    remove_all_colors(gameWin);

    // 2. Blank the interior so last frame's positions don't ghost.
    for (int y = 1; y <= ROWS; y++) {
        for (int x = 1; x <= COLS; x++) {
            mvwaddch(gameWin, y, x, ' ');
        }
    }

    // 3. Snake body first, then head, so the head color isn't overwritten.
    apply_snake_body_color(gameWin);
    for (int i = 1; i < snake.length; i++) {
        mvwaddch(gameWin, snake.body[i].y + 1, snake.body[i].x + 1, 'o');
    }
    remove_all_colors(gameWin);
    apply_snake_head_color(gameWin);
    mvwaddch(gameWin, snake.body[0].y + 1, snake.body[0].x + 1, '@');
    remove_all_colors(gameWin);

    // 4. Regular food.
    apply_food_color(gameWin);
    for (int i = 0; i < foodCount; i++) {
        mvwaddch(gameWin, food[i].y + 1, food[i].x + 1, '*');
    }
    remove_all_colors(gameWin);
    // Draw temporary food with different colors
    for (int i = 0; i < tempFoodCount; i++) {
        // Different colors for different types
        switch (tempFood[i].foodType) {
            case 0: // Normal - magenta
                apply_temp_food_color(gameWin);
                break;
            case 1: // Double - yellow
                apply_warning_color(gameWin);
                break;
            case 2: // Triple - green
                apply_success_color(gameWin);
                break;
        }
        mvwaddch(gameWin, tempFood[i].pos.y + 1, tempFood[i].pos.x + 1, tempFood[i].symbol);
        remove_all_colors(gameWin);
    }
    apply_special_item_color(gameWin);
    if (specialItem.active) {
        mvwaddch(gameWin, specialItem.pos.y + 1, specialItem.pos.x + 1, specialItem.symbol);
    }
    remove_all_colors(gameWin);

    // Shrink potion - reuse the success color to read as "friendly".
    if (shrinkPotion.active) {
        apply_success_color(gameWin);
        mvwaddch(gameWin, shrinkPotion.pos.y + 1, shrinkPotion.pos.x + 1, shrinkPotion.symbol);
        remove_all_colors(gameWin);
    }

    // Draw death items if active - CHECK ALL SLOTS
    apply_death_item_color(gameWin);
    for (int i = 0; i < MAX_DEATH_ITEMS; ++i) {
        if (deathItems[i].active) {
            mvwaddch(gameWin, deathItems[i].pos.y + 1, deathItems[i].pos.x + 1, deathItems[i].symbol);
        }
    }
    remove_all_colors(gameWin);
    
    // Calculate score dynamically for display
    int score = snake.length - 3;
    if (speedBoostActive) {
        apply_success_color(NULL); // Green text for speed boost
        mvprintw(ROWS + 3, 0, "Score: %d | SPEED BOOST: %d frames", score, speedBoostTimer);
        remove_all_colors(NULL);
    } else {
        // Clear the line first to remove any leftover characters from speed boost message
        move(ROWS + 3, 0);
        clrtoeol();
        apply_text_color(NULL);
        mvprintw(ROWS + 3, 0, "Score: %d", score);
        remove_all_colors(NULL);
    }
    
    // PAUSED overlay drawn on top of the game window.
    if (paused) {
        apply_warning_color(gameWin);
        mvwprintw(gameWin, ROWS / 2, COLS / 2 - 3, "PAUSED");
        remove_all_colors(gameWin);
    }

    // Refresh border window first, then stdscr
    wrefresh(gameWin);

    // Draw instructions panel
    drawInstructions();

    refresh();
}
/* ------------------ END OF GAME WINDOW FUNCTIONS ------------------ */
void changeDirection(int input) {
    // Check against current direction to prevent 180-degree turns
    Direction newDir = nextDirection;  // Start with buffered direction

    switch (input) {
        case 'W': case 'w': case KEY_UP:
            if (snake.direction != DOWN) newDir = UP;
            break;
        case 'S': case 's': case KEY_DOWN:
            if (snake.direction != UP) newDir = DOWN;
            break;
        case 'A': case 'a': case KEY_LEFT:
            if (snake.direction != RIGHT) newDir = LEFT;
            break;
        case 'D': case 'd': case KEY_RIGHT:
            if (snake.direction != LEFT) newDir = RIGHT;
            break;
    }

    // Only update if it's a valid direction change
    nextDirection = newDir;
}

void updateTempFood() {
    for (int i = 0; i < tempFoodCount; i++) {
        tempFood[i].timeLeft--;
        //tempFood[i].blinkCounter++;
        
        // Remove expired temp food
        if (tempFood[i].timeLeft <= 0) {
            for (int j = i; j < tempFoodCount - 1; j++) {
                tempFood[j] = tempFood[j + 1];
            }
            tempFoodCount--;
            i--; // Adjust index after removal
        }
    }
}

void updateSpeedBoost() {
    if (speedBoostActive) {
        speedBoostTimer--;
        if (speedBoostTimer <= 0) {
            speedBoostActive = 0;
        }
    }
}

void trySpawnSpecialItem() {
    
    if (specialItem.active) return;
    
    // Calculate probability: starts at 0.1% at refresh 100, increases to 5% at refresh 1000
    // Formula: base probability + (refreshCounter / scaling factor)
    float baseProbability = 0.0001f; // 0.01%
    float scalingFactor = 100.0f;   // How fast probability increases
    float maxProbability = 0.005f;   // 0.5% maximum
    
    float currentProbability = baseProbability + (refreshCounter / scalingFactor);
    if (currentProbability > maxProbability) {
        currentProbability = maxProbability;
    }
    
    // Convert to percentage for rand() check (0-10000 for better precision)
    int probabilityThreshold = (int)(currentProbability * 10000);
    
    if (rand() % 10000 < probabilityThreshold) {
        // Spawn special item at random location (avoid snake body)
        specialItem.symbol = '$'; // Default symbol
        int attempts = 0;
        do {
            specialItem.pos.x = rand() % COLS;
            specialItem.pos.y = rand() % ROWS;
            attempts++;
        } while (checkSnakeOverlap(specialItem.pos) && attempts < 100);
        
        // Only activate if we found a valid position
        if (attempts < 100) {
            specialItem.active = 1;
        }
    }
}
void trySpawnTempFood() {
    // Only spawn if we have room for more temp food
    if (tempFoodCount >= MAX_TEMP_FOOD) return;

    // Probability ramps with a dedicated counter so temp-food spawns don't
    // reset the special-item and death-item probability curves.
    float baseProbability = 0.001f; // 0.1% base chance
    float timeFactor = tempFoodSpawnCounter * 0.00001f;
    float maxProbability = 0.02f;   // 2% maximum

    float currentProbability = baseProbability + timeFactor;
    if (currentProbability > maxProbability) {
        currentProbability = maxProbability;
    }

    int probabilityThreshold = (int)(currentProbability * 10000);

    if (rand() % 10000 < probabilityThreshold) {
        placeTempFood();
        tempFoodSpawnCounter = 0;
    }
}
void trySpawnShrinkPotion() {
    if (shrinkPotion.active) return;
    // Only appears once the snake is long enough for shrinking to matter.
    if (snake.length < 8) return;

    // Rare: ~0.03% base, ramps to ~0.3% max.
    float baseProbability = 0.0003f;
    float timeFactor = refreshCounter * 0.000002f;
    float maxProbability = 0.003f;

    float currentProbability = baseProbability + timeFactor;
    if (currentProbability > maxProbability) currentProbability = maxProbability;

    int probabilityThreshold = (int)(currentProbability * 10000);
    if (rand() % 10000 >= probabilityThreshold) return;

    Point p;
    int attempts = 0;
    do {
        p.x = rand() % COLS;
        p.y = rand() % ROWS;
        attempts++;
    } while (cellIsOccupied(p) && attempts < 100);

    if (attempts < 100) {
        shrinkPotion.pos = p;
        shrinkPotion.symbol = '-';
        shrinkPotion.active = 1;
    }
}

void trySpawnDeathItem() {
    // Count currently active death items
    int activeCount = 0;
    for (int i = 0; i < MAX_DEATH_ITEMS; ++i) {
        if (deathItems[i].active) activeCount++;
    }
    
    // Desired number of death items increases over time
    int desired = 1 + (refreshCounter / 250); // +1 every 250 frames
    if (desired > MAX_DEATH_ITEMS) desired = MAX_DEATH_ITEMS;

    // Only attempt periodically to avoid flooding
    if (activeCount >= desired) return;
    if (refreshCounter % 20 != 0) return; // spawn at most every 20 frames

    // Find a free slot
    int slot = -1;
    for (int i = 0; i < MAX_DEATH_ITEMS; ++i) {
        if (!deathItems[i].active) { slot = i; break; }
    }
    if (slot == -1) return; // no slot available

    // Attempt to find a safe position
    int attempts = 0;
    Point p;
    while (attempts < 100) {
        p.x = rand() % COLS;
        p.y = rand() % ROWS;
        attempts++;

        // Must not be out of bounds or on snake body
        if (p.x < 0 || p.x >= COLS || p.y < 0 || p.y >= ROWS) continue;
        if (checkSnakeOverlap(p)) continue;

        // Avoid regular food
        int bad = 0;
        for (int i = 0; i < foodCount; ++i) {
            if (food[i].x == p.x && food[i].y == p.y) { bad = 1; break; }
        }
        if (bad) continue;

        // Avoid temp food
        for (int i = 0; i < tempFoodCount && !bad; ++i) {
            if (tempFood[i].pos.x == p.x && tempFood[i].pos.y == p.y) { bad = 1; break; }
        }
        if (bad) continue;

        // Avoid special item
        if (specialItem.active && specialItem.pos.x == p.x && specialItem.pos.y == p.y) continue;

        // Avoid other death items - CHECK ALL SLOTS, not just up to deathItemCount
        for (int i = 0; i < MAX_DEATH_ITEMS && !bad; ++i) {
            if (deathItems[i].active && deathItems[i].pos.x == p.x && deathItems[i].pos.y == p.y) { 
                bad = 1; 
                break; 
            }
        }
        if (bad) continue;

        // Place it
        deathItems[slot].pos = p;
        deathItems[slot].symbol = 'X';
        deathItems[slot].active = 1;
        // deathItemCount is now computed dynamically, no need to increment
        break;
    }
}
// Blocking pre-game options screen. Returns 0 to start, 1 to quit.
int configMenu(void) {
    nodelay(stdscr, FALSE);   // blocking input for the menu
    int done = 0;
    int quit = 0;

    while (!done) {
        clear();
        apply_text_color(NULL);
        mvprintw(2, 5, "=== SNAKE - Options ===");

        mvprintw(5, 5, "[1] Walls:     %s", config.wraparound ? "WRAP" : "KILL");
        mvprintw(6, 5, "[2] Speed:     %s", speedNames[config.speedIndex]);
        mvprintw(7, 5, "[3] Controls:  %s", config.useArrows ? "ARROW KEYS" : "WASD");

        mvprintw(10, 5, "1/2/3 to cycle    ENTER to start    Q to quit");
        mvprintw(12, 5, "Tip: some terminals render slower - pick a lower");
        mvprintw(13, 5, "     speed if the game feels laggy.");
        remove_all_colors(NULL);
        refresh();

        int ch = getch();
        switch (ch) {
            case '1':
                config.wraparound = !config.wraparound;
                break;
            case '2':
                config.speedIndex++;
                if (config.speedIndex > 5) config.speedIndex = 1;
                break;
            case '3':
                config.useArrows = !config.useArrows;
                break;
            case '\n': case '\r': case KEY_ENTER:
                done = 1;
                break;
            case 'q': case 'Q':
                done = 1;
                quit = 1;
                break;
        }
    }

    clear();
    refresh();
    nodelay(stdscr, TRUE);    // back to non-blocking for the game
    return quit;
}

// True if the keypress matches whichever movement scheme is configured.
static int isMovementKey(int ch) {
    if (config.useArrows) {
        return ch == KEY_UP || ch == KEY_DOWN ||
               ch == KEY_LEFT || ch == KEY_RIGHT;
    }
    return ch == 'W' || ch == 'w' || ch == 'S' || ch == 's' ||
           ch == 'A' || ch == 'a' || ch == 'D' || ch == 'd';
}

int main() {

    srand(time(NULL));
    initscr();
    cbreak();
    noecho();
    nodelay(stdscr, TRUE);
    curs_set(0);
    keypad(stdscr, TRUE);
     if (!init_colors()) {
        printw("Colors not supported on this terminal\n");
        refresh();
        getch();
        endwin();
        return 1;
    }

    if (configMenu()) {
        endwin();
        return 0;
    }

    initBorder(); // Create border once
    initSnake();
    placeFood();

    // Main game restart loop
    while (true) {
        // Clear input buffer at start of each game
        flushinp();
        
        // Game play loop
        while (!gameOver) {
            int ch = getch();
            if (ch != ERR) {
                if (ch == 'q' || ch == 'Q') {
                    gameOver = 1; // Exit to game over screen
                    break;
                }
                if (ch == 'p' || ch == 'P') {
                    paused = !paused;
                } else if (!paused && isMovementKey(ch)) {
                    changeDirection(ch);
                }
            }

            if (paused) {
                // Freeze game state; keep drawing so the PAUSED overlay shows.
                drawBoard();
                usleep((unsigned int)(DELAY));
                continue;
            }

            // Increment frame counters
            refreshCounter++;
            tempFoodSpawnCounter++;
            movementFrameCounter++;

            // Movement interval from config, with speed boost speeding it up.
            int baseInterval = speedIntervals[config.speedIndex];
            int currentMovementInterval = baseInterval;
            if (speedBoostActive) {
                currentMovementInterval = baseInterval * 2 / 3;
                if (currentMovementInterval < 1) currentMovementInterval = 1;
            }

            // Only move snake at intervals
            if (movementFrameCounter >= currentMovementInterval) {
                moveSnake();
                movementFrameCounter = 0;
            }

            updateTempFood();       // Update temporary food timers
            updateSpeedBoost();     // Update speed boost timer
            trySpawnTempFood();     // Try to spawn temp food randomly
            trySpawnSpecialItem();  // Try to spawn special item
            trySpawnShrinkPotion(); // Try to spawn shrink potion
            trySpawnDeathItem();    // Try to spawn death item
            drawBoard();

            // Fixed delay for all frames - direction no longer affects delay
            if (snake.direction == UP || snake.direction == DOWN)
                usleep((unsigned int)(DELAY * 1.2));   // 20% saktare. Kompensation för terminaldelay
            else
                usleep((unsigned int)(DELAY));
        }
  
    // Game Over Screen with Restart Option
    while (true) {
        int finalScore = snake.length - 3;
        
        // Clear the game area and show game over message
        apply_error_color(NULL);
        mvprintw(ROWS / 2, COLS / 2 - 5, "GAME OVER!");
        remove_all_colors(NULL);
        
        apply_text_color(NULL);
        mvprintw(ROWS / 2 + 1, COLS / 2 - 7, "Final Score: %d", finalScore);
        mvprintw(ROWS / 2 + 3, COLS / 2 - 10, "Press R to Restart");
        mvprintw(ROWS / 2 + 4, COLS / 2 - 8, "Press Q to Quit");
        remove_all_colors(NULL);
        
        // Keep instructions visible during game over
        drawInstructions();
        
        refresh();
        
        // Wait for user input
        int ch = getch();
        if (ch == 'r' || ch == 'R') {
            // Clear input buffer before restart
            flushinp();
            // Re-open the options screen between rounds so speed / wrap /
            // control scheme can be tuned without quitting. Q here quits.
            if (configMenu()) {
                delwin(gameWin);
                endwin();
                return 0;
            }
            resetGame();
            // Do not re-initBorder - the existing gameWin is fine and
            // reallocating it here leaked the previous WINDOW every restart.
            break; // Exit game over loop to restart the game
        } else if (ch == 'q' || ch == 'Q') {
            // Clear buffers before exit
            flushinp();
            // Clean up and exit completely
            delwin(gameWin);
            endwin();
            return 0;
        }
        
        usleep(50000); // Small delay to prevent excessive CPU usage
    }
    // Continue to next iteration of main restart loop
    }
    
    // This point should never be reached due to the infinite restart loop,
    // but included for completeness
    delwin(gameWin);
    endwin();
    return 0;
}
