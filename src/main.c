#include "wasm4.h"
#include <stdlib.h>
#include <string.h>

#define GRID_WIDTH 10
#define WALLS_WIDTH 10
#define CELL_SIZE (SCREEN_SIZE - WALLS_WIDTH * 2) / GRID_WIDTH

#define EMPTY 0
#define WALL 1
#define SNAKE 2
#define APPLE 3

#define GRID_DIRECTION(grid, x, y) ((grid[x][y] & (uint8_t)0b00011100u) >> 2)

#define SET_GRID_DIRECTION(grid, x, y, direction)                                                  \
    (grid[x][y] = (uint8_t)((grid[x][y] & 0b11100011) | ((direction & 0b111) << 2)))

#define GRID_CONTENT(grid, x, y) (grid[x][y] & (uint8_t)0b00000011u)

#define SET_GRID_CONTENT(grid, x, y, content)                                                      \
    (grid[x][y] = (uint8_t)((grid[x][y] & 0b11111100) | (content & 0b11)))

#define IS_FORBIDDEN(grid, x, y) ((grid[x][y] & (uint8_t)0b00100000u) >> 5)
#define SET_FORBIDDEN(grid, x, y) (grid[x][y] |= (uint8_t)0b00100000u)

#define UP 1
#define DOWN 2
#define LEFT 3
#define RIGHT 4

//#define DEBUG

struct snake {
    int8_t head_x;
    int8_t head_y;
    int8_t tail_x;
    int8_t tail_y;
    uint8_t length;
    uint8_t current_length;
    uint8_t direction;
};

#define CURRENT_VERSION 2
struct highscore {
    uint8_t version;
    uint8_t scores[5];
    char names[5][6];
};

typedef uint8_t grid_t[GRID_WIDTH][GRID_WIDTH];

void update_snake(struct snake *snake, grid_t grid) {
    if (snake->current_length < snake->length) {
        snake->current_length++;
    } else {
        int8_t holdx = snake->tail_x;
        int8_t holdy = snake->tail_y;
        switch (GRID_DIRECTION(grid, snake->tail_x, snake->tail_y)) {
            case UP:
                snake->tail_y--;
                break;
            case DOWN:
                snake->tail_y++;
                break;
            case LEFT:
                snake->tail_x--;
                break;
            case RIGHT:
                snake->tail_x++;
                break;
            default:
#ifdef DEBUG
                trace("Wtf");
#endif
                return;
        }
        SET_GRID_CONTENT(grid, holdx, holdy, EMPTY);
        SET_GRID_DIRECTION(grid, holdx, holdy, EMPTY);
    }

    SET_GRID_DIRECTION(grid, snake->head_x, snake->head_y, snake->direction);
    switch (snake->direction) {
        case UP:
            snake->head_y--;
            break;
        case DOWN:
            snake->head_y++;
            break;
        case LEFT:
            snake->head_x--;
            break;
        case RIGHT:
            snake->head_x++;
            break;
    }
}

void put_snake_head(const struct snake *snake, grid_t grid) {
    SET_GRID_CONTENT(grid, snake->head_x, snake->head_y, SNAKE);
}

int check_collision(struct snake *snake, const grid_t grid, uint8_t *apple_count,
                    uint8_t *wall_counter, uint8_t *score) {
    if (snake->head_x < 0 || snake->head_x >= GRID_WIDTH || snake->head_y < 0 ||
        snake->head_y >= GRID_WIDTH) {
        return 1;
    }
    if (GRID_CONTENT(grid, snake->head_x, snake->head_y) == WALL) {
#ifdef DEBUG
        trace("Collision with wall");
#endif
        return 1;
    }
    if (GRID_CONTENT(grid, snake->head_x, snake->head_y) == SNAKE) {
#ifdef DEBUG
        trace("Collision with snake");
#endif
        return 1;
    }

    if (GRID_CONTENT(grid, snake->head_x, snake->head_y) == APPLE) {
#ifdef DEBUG
        trace("Collision with apple");
#endif
        snake->length++;
        (*apple_count)--;
        (*score)++;
        (*wall_counter)++;
    }
    return 0;
}

int head_range(const struct snake *snake, int8_t x, int8_t y) {
#define HEAD_RANGE 2
    if (snake->head_x - HEAD_RANGE <= x && x <= snake->head_x + HEAD_RANGE &&
        snake->head_y - HEAD_RANGE <= y && y <= snake->head_y + HEAD_RANGE) {
        return 1;
    }
    return 0;
}

int place_apples(grid_t grid, uint8_t *apple_count) {
    for (int i = (uint8_t)(rand() % 5 + 1); i > 0; i--) {
        int x = rand() % GRID_WIDTH;
        int y = rand() % GRID_WIDTH;
        int count = 0;
        do {
            x += 1;
            if (x >= GRID_WIDTH) {
                x = 0;
                y += 1;
                if (y >= GRID_WIDTH) {
                    y = 0;
                }
            }
            count++;
        } while (GRID_CONTENT(grid, x, y) != EMPTY && count < GRID_WIDTH * GRID_WIDTH);
        if (count >= GRID_WIDTH * GRID_WIDTH) {
            return 1; // no empty space found
        }
        (*apple_count)++;
        SET_GRID_CONTENT(grid, x, y, APPLE);
    }
    return 0;
}

int place_wall(grid_t grid, const struct snake *snake) {
    int8_t x = rand() % GRID_WIDTH;
    int8_t y = rand() % GRID_WIDTH;

    int8_t count = 0;
    while ((GRID_CONTENT(grid, x, y) != EMPTY || IS_FORBIDDEN(grid, x, y) ||
            head_range(snake, x, y)) &&
           count < GRID_WIDTH * GRID_WIDTH) {
        x += 1;
        if (x >= GRID_WIDTH) {
            x = 0;
            y += 1;
            if (y >= GRID_WIDTH) {
                y = 0;
            }
        }
        count++;
    }
    if (count >= GRID_WIDTH * GRID_WIDTH) {
#ifdef DEBUG
        trace("No empty space found");
#endif
        return 1; // no empty space found
    }

    SET_GRID_CONTENT(grid, x, y, WALL);
    for (int8_t i = -1; i <= 1; i++) {
        for (int8_t j = -1; j <= 1; j++) {
            if (x + i >= 0 && x + i < GRID_WIDTH && y + j >= 0 && y + j < GRID_WIDTH) {
                SET_FORBIDDEN(grid, x + i, y + j);
            }
        }
    }
    if (y == 0 || y == GRID_WIDTH - 1) {
        if (x > 1) {
            SET_FORBIDDEN(grid, x - 2, y);
        }
        if (x < GRID_WIDTH - 2) {
            SET_FORBIDDEN(grid, x + 2, y);
        }
    }
    if (x == 0 || x == GRID_WIDTH - 1) {
        if (y > 1) {
            SET_FORBIDDEN(grid, x, y - 2);
        }
        if (y < GRID_WIDTH - 2) {
            SET_FORBIDDEN(grid, x, y + 2);
        }
    }
    return 0;
}

void put_highscore(struct highscore *highscore, uint8_t score, const char *name) {
    for (int i = 0; i < 5; i++) {
        if (highscore->scores[i] < score) {
            for (int j = 4; j > i; j--) {
                highscore->scores[j] = highscore->scores[j - 1];
                memcpy(highscore->names[j], highscore->names[j - 1], 6);
            }
            highscore->scores[i] = score;
            memcpy(highscore->names[i], name, 6);
            break;
        }
    }
}

int is_highscore(const struct highscore *highscore, uint8_t score) {
    for (int i = 0; i < 5; i++) {
        if (highscore->scores[i] < score) {
            return 1;
        }
    }
    return 0;
}

void load_highscore(struct highscore *highscore) {
    diskr(highscore, sizeof(struct highscore));
    if (highscore->version != CURRENT_VERSION) {
        highscore->version = CURRENT_VERSION;
        for (int i = 0; i < 5; i++) {
            highscore->scores[i] = 0;
            memcpy(highscore->names[i], "AAAAA", sizeof(highscore->names[i]));
        }
    }
}

void save_highscore(const struct highscore *highscore) {
    diskw(highscore, sizeof(struct highscore));
}

void draw_grid(const grid_t grid, const struct snake *snake) {
    *DRAW_COLORS = 2 | 2 << 2;
    rect(0, 0, SCREEN_SIZE, WALLS_WIDTH);
    rect(0, SCREEN_SIZE - WALLS_WIDTH, SCREEN_SIZE, WALLS_WIDTH);
    rect(0, WALLS_WIDTH, WALLS_WIDTH, SCREEN_SIZE - WALLS_WIDTH * 2);
    rect(SCREEN_SIZE - WALLS_WIDTH, WALLS_WIDTH, WALLS_WIDTH, SCREEN_SIZE - WALLS_WIDTH * 2);

    for (int x = 0; x < GRID_WIDTH; x++) {
        for (int y = 0; y < GRID_WIDTH; y++) {
            uint8_t content = GRID_CONTENT(grid, x, y);
            *DRAW_COLORS = (uint16_t)((content + 1) | (content + 1) << 4);
            int32_t gx = x * CELL_SIZE + WALLS_WIDTH;
            int32_t gy = y * CELL_SIZE + WALLS_WIDTH;
            if (x == snake->head_x && y == snake->head_y) {
                oval(gx, gy, CELL_SIZE, CELL_SIZE);
                switch (snake->direction) {
                    case UP:
                        rect(gx, gy + CELL_SIZE / 2 + 1, CELL_SIZE, CELL_SIZE / 2);
                        break;
                    case DOWN:
                        rect(gx, gy, CELL_SIZE, CELL_SIZE / 2);
                        break;
                    case LEFT:
                        rect(gx + CELL_SIZE / 2 + 1, gy, CELL_SIZE / 2, CELL_SIZE);
                        break;
                    case RIGHT:
                        rect(gx, gy, CELL_SIZE / 2, CELL_SIZE);
                        break;
                }
            } else if (content == APPLE) {
                uint32_t diameter = CELL_SIZE / 2;
                gx += CELL_SIZE / 4 + 1;
                gy += CELL_SIZE / 4 + 1;
                oval(gx, gy, diameter, diameter);
            } else {
                rect(gx, gy, CELL_SIZE, CELL_SIZE);
            }

#ifdef DEBUG
            if (IS_FORBIDDEN(grid, x, y)) {
                *DRAW_COLORS = 3;
                text("X", gx + CELL_SIZE / 2 - 4, gy + CELL_SIZE / 2 - 4);
            }
#endif
        }
    }
}

void draw_number(uint8_t number, int32_t x, int32_t y) {
    char buffer[4];
    for (uint8_t i = 0; i < 3; i++) {
        buffer[2 - i] = '0' + number % 10;
        number /= 10;
    }
    buffer[3] = '\0';
    text(buffer, x, y);
}

grid_t grid;
struct snake snake;
struct highscore highscore;

uint8_t apple_count;
uint8_t status;
uint8_t previous_gamepad;
int frame_count = 0;
uint8_t direction = 0;
uint8_t score = 0;
uint8_t wall_counter = 0;
uint8_t cursor = 0;
uint8_t grid_filled = 0;
char name[6] = "AAAAA";

void start() {
    PALETTE[0] = 0xaad751; // grass green
    PALETTE[1] = 0x578a34; // wall green
    PALETTE[2] = 0x4e7cf6; // snake blue
    PALETTE[3] = 0xe7471d; // apple red
    status = 0;
    apple_count = 0;
    snake.head_x = 0;
    snake.head_y = 0;
    snake.tail_x = 0;
    snake.tail_y = 0;
    snake.length = 2;
    snake.current_length = 1;
    snake.direction = RIGHT;
    direction = RIGHT;
    score = 0;
    for (int i = 0; i < GRID_WIDTH; i++) {
        for (int j = 0; j < GRID_WIDTH; j++) {
            grid[i][j] = EMPTY;
        }
    }
    SET_GRID_CONTENT(grid, snake.head_x, snake.head_y, SNAKE);
    SET_GRID_DIRECTION(grid, snake.head_x, snake.head_y, snake.direction);
    wall_counter = 0;
    SET_FORBIDDEN(grid, 1, 0);
    SET_FORBIDDEN(grid, 0, 1);
    SET_FORBIDDEN(grid, GRID_WIDTH - 2, 0);
    SET_FORBIDDEN(grid, GRID_WIDTH - 1, 1);
    SET_FORBIDDEN(grid, GRID_WIDTH - 2, GRID_WIDTH - 1);
    SET_FORBIDDEN(grid, GRID_WIDTH - 1, GRID_WIDTH - 2);
    SET_FORBIDDEN(grid, 0, GRID_WIDTH - 2);
    SET_FORBIDDEN(grid, 1, GRID_WIDTH - 1);
    load_highscore(&highscore);
}

void update() {
    uint8_t gamepad = *GAMEPAD1;
    uint8_t pressed = gamepad & (gamepad ^ previous_gamepad);
    previous_gamepad = gamepad;
    frame_count++;

    if (status != 3) {
        draw_grid(grid, &snake);
    }
    switch (status) {
        case 0: // waiting for start
            if (pressed & BUTTON_1) {
                status = 1;
            }
            *DRAW_COLORS = 3;
            text("Press 1 to start", 16, 30);
            text("Hi-scores", 45, 50);
            for (int i = 0; i < 5; i++) {
#define LEFTOFFSET 38
                text(highscore.names[i], LEFTOFFSET, 60 + i * 10);
                draw_number(highscore.scores[i], LEFTOFFSET + 60, 60 + i * 10);
            }
            break;
        case 1: // game running
            if (!grid_filled && wall_counter >= 2) {
                grid_filled += place_wall(grid, &snake);
                wall_counter = 0;
            }
            if (grid_filled < 2 && apple_count == 0) {
                grid_filled += place_apples(grid, &apple_count);
            }

            if (pressed & BUTTON_UP && snake.direction != DOWN) {
                direction = UP;
            } else if (pressed & BUTTON_DOWN && snake.direction != UP) {
                direction = DOWN;
            } else if (pressed & BUTTON_LEFT && snake.direction != RIGHT) {
                direction = LEFT;
            } else if (pressed & BUTTON_RIGHT && snake.direction != LEFT) {
                direction = RIGHT;
            }

            if (frame_count > 12) {
                snake.direction = direction;
                update_snake(&snake, grid);
                if (check_collision(&snake, grid, &apple_count, &wall_counter, &score)) {
                    status = 2; // game over
                } else {
                    put_snake_head(&snake, grid);
                }
                frame_count = 0;
            }
            draw_grid(grid, &snake);
            break;
        case 2: // game over
            if (pressed & BUTTON_1) {
                if (is_highscore(&highscore, score)) {
                    status = 3;
                } else {
                    start();
                }
            }
            *DRAW_COLORS = 4;
            text("Game Over", 45, 70);
            break;
        case 3:
            if (pressed & BUTTON_LEFT) {
                cursor = (cursor + 4) % 5;
            } else if (pressed & BUTTON_RIGHT) {
                cursor = (cursor + 1) % 5;
            } else if (gamepad & BUTTON_DOWN && frame_count > 7) {
                frame_count = 0;
                name[cursor] = name[cursor] == ' ' ? '}' : name[cursor] - 1;
            } else if (gamepad & BUTTON_UP && frame_count > 7) {
                frame_count = 0;
                name[cursor] = name[cursor] == '}' ? ' ' : name[cursor] + 1;
            } else if (pressed & BUTTON_1) {
                put_highscore(&highscore, score, name);
                save_highscore(&highscore);
                start();
            }
            *DRAW_COLORS = 3;
            text("Enter your name", 16, 30);
            *DRAW_COLORS = 2;
            rect(16 + cursor * 8, 50, 7, 7);
            *DRAW_COLORS = 4;
            text(name, 16, 50);
            *DRAW_COLORS = 3;
            text("Press 1 to save", 16, 70);
            text("L/R: cursor", 16, 90);
            text("U/D:letter", 16, 110);
            break;
        default:
            status = 0;
            break;
    }
    *DRAW_COLORS = 4;
    text("P:", 1, 1);
    draw_number(score, 18, 1);
    text("Hi:", 112, 1);
    if (score > highscore.scores[0]) {
        draw_number(score, 136, 1);
    } else {
        draw_number(highscore.scores[0], 136, 1);
    }
}
