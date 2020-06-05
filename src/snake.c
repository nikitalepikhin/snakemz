/*******************************************************************
  
  SNAKE GAME by Nikita Lepikhin @lepiknik
  Project for MicroZed based MZ_APO board

  snake.c      - main file

 *******************************************************************/

#define _POSIX_C_SOURCE 200112L

#define UP_KEY 'w'
#define RIGHT_KEY 'd'
#define DOWN_KEY 's'
#define LEFT_KEY 'a'
#define ESCAPE 0x1B
#define RETURN 0xA

#define BOARD_W 46
#define BOARD_H 25

#define EMPTY 0
#define SNAKE 1
#define FOOD 2

#define MAX_SNAKE_LEN 100

#define COLOR_BLUE 0x023F
#define COLOR_RED 0xD800
#define COLOR_WHITE 0xFFFF
#define COLOR_GREEN 0x0500
#define COLOR_BLACK 0x0000

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "font_types.h"
#include "mzapo_parlcd.h"
#include "mzapo_phys.h"
#include "mzapo_regs.h"

unsigned short *fb;

typedef struct {
    unsigned int x;
    unsigned int y;
    unsigned char move_type;
} snake_piece;

typedef struct {
    unsigned int snake_len;
    snake_piece *snake;
    unsigned int food_x;
    unsigned int food_y;
} board;

board gameboard;
unsigned char *mem_base;
unsigned char *parlcd_mem_base;
struct timespec loop_delay;
font_descriptor_t *fdes;
char difficulty = 0;
char snake_color = 2;

void update_led_red() {
    *(volatile uint32_t *)(mem_base + SPILED_REG_LED_RGB1_o) = 0xFF0000;
    *(volatile uint32_t *)(mem_base + SPILED_REG_LED_RGB2_o) = 0xFF0000;
}

void update_led_blue() {
    *(volatile uint32_t *)(mem_base + SPILED_REG_LED_RGB1_o) = 0xFF;
    *(volatile uint32_t *)(mem_base + SPILED_REG_LED_RGB2_o) = 0xFF;
}

void update_led_green() {
    *(volatile uint32_t *)(mem_base + SPILED_REG_LED_RGB1_o) = 0xFF00;
    *(volatile uint32_t *)(mem_base + SPILED_REG_LED_RGB2_o) = 0xFF00;
}

void update_led_off() {
    *(volatile uint32_t *)(mem_base + SPILED_REG_LED_RGB1_o) = 0x00;
    *(volatile uint32_t *)(mem_base + SPILED_REG_LED_RGB2_o) = 0x00;
}

void init_frame_buffer() {
    for (int ptr = 0; ptr < 320 * 480; ptr++) {
        fb[ptr] = 0u;
    }
}

void update_display() {
    parlcd_write_cmd(parlcd_mem_base, 0x2c);
    for (int ptr = 0; ptr < 480 * 320; ptr++) {
        parlcd_write_data(parlcd_mem_base, fb[ptr]);
    }
}

void draw_pixel(int x, int y, unsigned short color) {
    if (x >= 0 && x < 480 && y >= 0 && y < 320) {
        fb[x + 480 * y] = color;
    }
}

int char_width(font_descriptor_t *fdes, int ch) {
    int width = 0;
    if ((ch >= fdes->firstchar) && (ch - fdes->firstchar < fdes->size)) {
        ch -= fdes->firstchar;
        if (!fdes->width) {
            width = fdes->maxwidth;
        } else {
            width = fdes->width[ch];
        }
    }
    return width;
}

void draw_char(int x, int y, font_descriptor_t *fdes, char ch, short color, int scale) {
    uint16_t mask = 0x8000;
    mask >>= (16 - char_width(fdes, ch));
    uint16_t font_bits;
    uint16_t result;

    for (int i = 0; i < fdes->height; i++) {
        font_bits = fdes->bits[(ch - fdes->firstchar) * (fdes->height) + i];
        font_bits >>= (16 - char_width(fdes, ch));
        for (int j = 0; j < char_width(fdes, ch); j++) {
            result = font_bits & mask;
            if (result == mask) {
                for (int v = 0; v < scale; v++) {
                    for (int h = 0; h < scale; h++) {
                        draw_pixel(x + j * scale + h, y + i * scale + v, color);
                    }
                }
            }
            font_bits <<= 1;
        }
    }
}

/* Draws game view by adding corresponding values into LCD frame buffer */
void draw_game_view() {
    // upper border
    for (int i = 0; i < 480 * 10; i++) {
        fb[i] = COLOR_BLUE;
    }
    // side borders
    for (int i = 480 * 10; i < 480 * 260; i++) {
        if (i % 480 < 10 || (i % 480 >= 470 && i % 480 < 480)) {
            fb[i] = COLOR_BLUE;
        }
    }
    // lower border
    for (int i = 480 * 260; i < 480 * 270; i++) {
        fb[i] = COLOR_BLUE;
    }
    // points label
    char *str = "SCORE:\0";
    char *ch = str;
    int x = 10;
    int scale = 3;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 272, fdes, *ch, COLOR_BLUE, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
}

/* Draws color selection menu by adding corresponding values into LCD frame buffer */
void draw_color_selector_menu(int selected) {
    char *str = "SNAKE COLOR\0";
    char *ch = str;
    int x = 0;
    int scale = 5;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 30, fdes, *ch, COLOR_GREEN, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
    int y_select_start, y_select_end;
    switch (selected) {
        case 0:
            y_select_start = 113;
            y_select_end = 166;
            break;
        case 1:
            y_select_start = 166;
            y_select_end = 219;
            break;
        case 2:
            y_select_start = 219;
            y_select_end = 271;
            break;
    }
    for (int i = 480 * 113; i < 480 * 271; i++) {
        fb[i] = COLOR_BLACK;
    }
    for (int i = 480 * y_select_start; i < 480 * y_select_end; i++) {
        fb[i] = COLOR_BLUE;
    }
    str = "Red\0";
    ch = str;
    x = 150;
    scale = 3;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 115, fdes, *ch, selected == 0 ? COLOR_WHITE : COLOR_BLUE, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
    str = "Blue\0";
    ch = str;
    x = 150;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 168, fdes, *ch, selected == 1 ? COLOR_WHITE : COLOR_BLUE, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
    str = "White\0";
    ch = str;
    x = 150;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 221, fdes, *ch, selected == 2 ? COLOR_WHITE : COLOR_BLUE, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
}

/* Starts color selection menu */
void start_color_selector_menu() {
    init_frame_buffer();
    int r;
    char ch;
    int selected = 0;
    int diff_picked = 0;
    draw_color_selector_menu(selected);
    update_display();
    while (1) {
        r = read(0, &ch, 1);
        if (r == 1) {
            if (ch == 'w') {
                selected = selected == 0 ? 0 : selected - 1;
            } else if (ch == 's') {
                selected = selected == 2 ? 2 : selected + 1;
            } else if (ch == RETURN) {
                snake_color = selected;
                diff_picked = 1;
            }
        }
        draw_color_selector_menu(selected);
        update_display();
        if (diff_picked == 1) return;
    }
}

/* Draws difficulty selection menu by adding corresponding values into LCD frame buffer */
void draw_difficulty_selector_menu(int selected) {
    char *str = "DIFFICULTY\0";
    char *ch = str;
    int x = 50;
    int scale = 5;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 30, fdes, *ch, COLOR_GREEN, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
    int y_select_start, y_select_end;
    switch (selected) {
        case 0:
            y_select_start = 113;
            y_select_end = 166;
            break;
        case 1:
            y_select_start = 166;
            y_select_end = 219;
            break;
        case 2:
            y_select_start = 219;
            y_select_end = 271;
            break;
    }
    for (int i = 480 * 113; i < 480 * 271; i++) {
        fb[i] = COLOR_BLACK;
    }
    for (int i = 480 * y_select_start; i < 480 * y_select_end; i++) {
        fb[i] = COLOR_BLUE;
    }
    str = "Normal\0";
    ch = str;
    x = 150;
    scale = 3;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 115, fdes, *ch, selected == 0 ? COLOR_WHITE : COLOR_BLUE, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
    str = "Hard\0";
    ch = str;
    x = 150;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 168, fdes, *ch, selected == 1 ? COLOR_WHITE : COLOR_BLUE, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
}

/* Starts difficulty selection menu */
void start_difficulty_selector_menu() {
    init_frame_buffer();
    int r;
    char ch;
    int selected = 0;
    int diff_picked = 0;
    draw_difficulty_selector_menu(selected);
    update_display();
    while (1) {
        r = read(0, &ch, 1);
        if (r == 1) {
            if (ch == 'w') {
                selected = selected == 0 ? 0 : selected - 1;
            } else if (ch == 's') {
                selected = selected == 1 ? 1 : selected + 1;
            } else if (ch == RETURN) {
                difficulty = selected;
                diff_picked = 1;
            }
        }
        draw_difficulty_selector_menu(selected);
        update_display();
        if (diff_picked == 1) return;
    }
}

/* Draws options menu by adding corresponding values into LCD frame buffer */
void draw_options_menu(int selected) {
    char *str = "OPTIONS\0";
    char *ch = str;
    int x = 80;
    int scale = 5;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 30, fdes, *ch, COLOR_GREEN, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
    int y_select_start, y_select_end;
    switch (selected) {
        case 0:
            y_select_start = 113;
            y_select_end = 166;
            break;
        case 1:
            y_select_start = 166;
            y_select_end = 219;
            break;
        case 2:
            y_select_start = 219;
            y_select_end = 271;
            break;
    }
    for (int i = 480 * 113; i < 480 * 271; i++) {
        fb[i] = COLOR_BLACK;
    }
    for (int i = 480 * y_select_start; i < 480 * y_select_end; i++) {
        fb[i] = COLOR_BLUE;
    }
    str = "Difficulty\0";
    ch = str;
    x = 150;
    scale = 3;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 115, fdes, *ch, selected == 0 ? COLOR_WHITE : COLOR_BLUE, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
    str = "Color\0";
    ch = str;
    x = 150;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 168, fdes, *ch, selected == 1 ? COLOR_WHITE : COLOR_BLUE, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
    str = "Back\0";
    ch = str;
    x = 150;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 221, fdes, *ch, selected == 2 ? COLOR_WHITE : COLOR_BLUE, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
}

/* Draws main game menu by adding corresponding values into LCD frame buffer */
void draw_main_menu(int selected) {
    char *str = "SNAKE\0";
    char *ch = str;
    int x = 127;
    int scale = 5;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 30, fdes, *ch, COLOR_RED, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
    int y_select_start, y_select_end;
    switch (selected) {
        case 0:
            y_select_start = 113;
            y_select_end = 166;
            break;
        case 1:
            y_select_start = 166;
            y_select_end = 219;
            break;
        case 2:
            y_select_start = 219;
            y_select_end = 271;
            break;
    }
    for (int i = 480 * 113; i < 480 * 271; i++) {
        fb[i] = COLOR_BLACK;
    }
    for (int i = 480 * y_select_start; i < 480 * y_select_end; i++) {
        fb[i] = COLOR_BLUE;
    }
    str = "Play\0";
    ch = str;
    x = 150;
    scale = 3;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 115, fdes, *ch, selected == 0 ? COLOR_WHITE : COLOR_BLUE, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
    str = "Options\0";
    ch = str;
    x = 150;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 168, fdes, *ch, selected == 1 ? COLOR_WHITE : COLOR_BLUE, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
    str = "Exit\0";
    ch = str;
    x = 150;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 221, fdes, *ch, selected == 2 ? COLOR_WHITE : COLOR_BLUE, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
}

/* Starts options menu */
void start_options_menu() {
    init_frame_buffer();
    int r;
    char ch;
    int selected = 0;
    draw_options_menu(selected);
    update_display();
    while (1) {
        r = read(0, &ch, 1);
        if (r == 1) {
            if (ch == 'w') {
                selected = selected == 0 ? 0 : selected - 1;
            } else if (ch == 's') {
                selected = selected == 2 ? 2 : selected + 1;
            } else if (ch == RETURN) {
                switch (selected) {
                    case 0:
                        start_difficulty_selector_menu();
                        break;
                    case 1:
                        start_color_selector_menu();
                        break;
                    case 2:
                        return;
                }
            }
            init_frame_buffer();
            draw_options_menu(selected);
            update_display();
        }
    }
}

/* Updates score GUI by adding corresponding values into LCD frame buffer */
void update_score() {
    for (int ptr = 480 * 270; ptr < 480 * 320; ptr++) {
        if (ptr % 480 >= 200) {
            fb[ptr] = COLOR_BLACK;
        }
    }
    char str[5];
    sprintf(str, "%d", gameboard.snake_len - 3);
    char *ch = str;
    int x = 200;
    int scale = 3;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 272, fdes, *ch, COLOR_BLUE, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
}

/* Clears board part of the game view by adding corresponding values into LCD frame buffer */
void clear_board() {
    for (int ptr = 480 * 10; ptr < 480 * 260; ptr++) {
        if (ptr % 480 < 470 && ptr % 480 >= 10) {
            fb[ptr] = COLOR_BLACK;
        }
    }
}

/* Calculates index in frame buffer of the top left pixel of the square (x, y) on the board */
unsigned int convert_board_coord_to_fb_idx(unsigned int x, unsigned int y) {
    return 480 * 10 + y * 10 * 480 + 10 + x * 10;
}

void draw_snake_and_food() {
    unsigned int conv, x_conv, y_conv, color;
    // pick snake color
    if (snake_color == 0)
        color = COLOR_RED;
    else if (snake_color == 1)
        color = COLOR_BLUE;
    else
        color = COLOR_WHITE;
    // draw snake
    for (int i = 0; i < gameboard.snake_len; i++) {
        conv = convert_board_coord_to_fb_idx(gameboard.snake[i].x, gameboard.snake[i].y);
        x_conv = conv % 480;
        y_conv = conv / 480;
        for (int y = 0; y < 10; y++) {
            for (int x = 0; x < 10; x++) {
                draw_pixel(x_conv + x, y_conv + y, color);
            }
        }
    }
    // draw food
    conv = convert_board_coord_to_fb_idx(gameboard.food_x, gameboard.food_y);
    x_conv = conv % 480;
    y_conv = conv / 480;
    for (int y = 0; y < 10; y++) {
        for (int x = 0; x < 10; x++) {
            draw_pixel(x_conv + x, y_conv + y, color == COLOR_RED ? COLOR_WHITE : COLOR_RED);
        }
    }
}

/* Initializes snake */
void init_snake() {
    time_t t;
    srand((unsigned)time(&t));
    unsigned char tail = rand() % 20;
    unsigned char height = rand() % 24;
    gameboard.snake_len = 3;
    for (int i = 0; i < gameboard.snake_len; i++) {
        gameboard.snake[0].x = tail + i;
        gameboard.snake[0].y = height;
        gameboard.snake[0].move_type = RIGHT_KEY;
    }
}

/* Checks for self-collision */
int check_self_collision() {
    for (int i = 0; i < gameboard.snake_len; i++) {
        for (int j = 0; j < gameboard.snake_len; j++) {
            if (i != j && gameboard.snake[i].x == gameboard.snake[j].x && gameboard.snake[i].y == gameboard.snake[j].y) {
                printf("\ngame over: self-collision detected\n");
                return 1;
            }
        }
    }
    return 0;
}

/* Checks for wall collision */
int check_wall_collision() {
    for (int i = 0; i < gameboard.snake_len; i++) {
        if (gameboard.snake[i].x < 0 || gameboard.snake[i].x >= BOARD_W || gameboard.snake[i].y < 0 || gameboard.snake[i].y >= BOARD_H) {
            printf("\ngame over: wall collision detected\n");
            return 1;
        }
    }
    return 0;
}

/* Moves snake by updating corresponding values inside array of snake_pieces */
int move_snake(char move_type) {
    // middle pieces move
    for (int i = gameboard.snake_len; i > 0; i--) {
        gameboard.snake[i].move_type = gameboard.snake[i - 1].move_type;
        gameboard.snake[i].x = gameboard.snake[i - 1].x;
        gameboard.snake[i].y = gameboard.snake[i - 1].y;
    }
    // head piece moves
    if (move_type == UP_KEY) {
        gameboard.snake[0].move_type = UP_KEY;
        gameboard.snake[0].y -= 1;
        if (gameboard.snake[0].y == -1 && difficulty == 0) gameboard.snake[0].y = BOARD_H - 1;
    } else if (move_type == DOWN_KEY) {
        gameboard.snake[0].move_type = DOWN_KEY;
        gameboard.snake[0].y += 1;
        if (difficulty == 0) gameboard.snake[0].y %= BOARD_H;
    } else if (move_type == RIGHT_KEY) {
        gameboard.snake[0].move_type = RIGHT_KEY;
        gameboard.snake[0].x += 1;
        if (difficulty == 0) gameboard.snake[0].x %= BOARD_W;
    } else if (move_type == LEFT_KEY) {
        gameboard.snake[0].move_type = LEFT_KEY;
        gameboard.snake[0].x -= 1;
        if (gameboard.snake[0].x == -1 && difficulty == 0) gameboard.snake[0].x = BOARD_W - 1;
    }
    // check for collisions
    if (check_self_collision() == 1) {
        return 1;
    }
    if (difficulty == 1 && check_wall_collision() == 1) {
        return 1;
    }
    // found food
    if (gameboard.food_x == gameboard.snake[0].x && gameboard.food_y == gameboard.snake[0].y) {
        gameboard.snake_len += 1;
        printf("\nfound food at X=%d Y=%d\n", gameboard.food_x, gameboard.food_y);
        printf("current score: %d\n", gameboard.snake_len - 3);
        gameboard.snake[gameboard.snake_len].move_type = gameboard.snake[gameboard.snake_len - 1].move_type;
        if (gameboard.snake[gameboard.snake_len].move_type == UP_KEY) {
            gameboard.snake[gameboard.snake_len].x = gameboard.snake[gameboard.snake_len - 1].x;
            gameboard.snake[gameboard.snake_len].y = gameboard.snake[gameboard.snake_len - 1].y + 1;
        } else if (gameboard.snake[gameboard.snake_len].move_type == DOWN_KEY) {
            gameboard.snake[gameboard.snake_len].x = gameboard.snake[gameboard.snake_len - 1].x;
            gameboard.snake[gameboard.snake_len].y = gameboard.snake[gameboard.snake_len - 1].y - 1;
        } else if (gameboard.snake[gameboard.snake_len].move_type == RIGHT_KEY) {
            gameboard.snake[gameboard.snake_len].x = gameboard.snake[gameboard.snake_len - 1].x - 1;
            gameboard.snake[gameboard.snake_len].y = gameboard.snake[gameboard.snake_len - 1].y;
        } else if (gameboard.snake[gameboard.snake_len].move_type == LEFT_KEY) {
            gameboard.snake[gameboard.snake_len].x = gameboard.snake[gameboard.snake_len - 1].x + 1;
            gameboard.snake[gameboard.snake_len].y = gameboard.snake[gameboard.snake_len - 1].y;
        }
        gameboard.food_x = rand() % BOARD_W;
        gameboard.food_y = rand() % BOARD_H;
        update_score();
        update_led_green();
        if (gameboard.snake_len == MAX_SNAKE_LEN) {
            printf("\nmax level reached\n");
            return 2;
        }
    }
    return 0;
}

/* Draws end game screen by adding corresponding values into LCD frame buffer */
void draw_end_game_screen() {
    if (gameboard.snake_len == MAX_SNAKE_LEN)
        update_led_green();
    else
        update_led_red();
    init_frame_buffer();
    char *str = "GAME\0";
    char *ch = str;
    int x = 5;
    int scale = 8;
    int color = gameboard.snake_len == MAX_SNAKE_LEN ? COLOR_GREEN : COLOR_RED;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 20, fdes, *ch, color, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
    x = 150;
    str = gameboard.snake_len == MAX_SNAKE_LEN ? "WON\0" : "OVER\0";
    ch = str;
    for (int i = 0; i < strlen(str); i++) {
        draw_char(x, 130, fdes, *ch, color, scale);
        x += scale * char_width(fdes, *ch);
        ch++;
    }
}

/* Starts game loop and other game logic */
void start_game() {
    draw_game_view();
    update_led_blue();
    loop_delay.tv_nsec = 150 * 1000 * 1000;
    gameboard.snake = malloc(sizeof(snake_piece) * MAX_SNAKE_LEN);
    gameboard.food_x = rand() % BOARD_W;
    gameboard.food_y = rand() % BOARD_H;
    init_snake();
    update_score();
    int r, moveres = 0;
    char ch;
    char move_type = gameboard.snake[0].move_type;

    while (1) {
        update_led_blue();
        // read a control character
        r = read(0, &ch, 1);
        if (r == 1) {
            if (ch == UP_KEY || ch == DOWN_KEY || ch == RIGHT_KEY || ch == LEFT_KEY) {
                if (!((ch == UP_KEY && gameboard.snake[0].move_type == DOWN_KEY) || (ch == DOWN_KEY && gameboard.snake[0].move_type == UP_KEY) ||
                      (ch == LEFT_KEY && gameboard.snake[0].move_type == RIGHT_KEY) || (ch == RIGHT_KEY && gameboard.snake[0].move_type == LEFT_KEY))) {
                    moveres = move_snake(ch);
                    move_type = ch;
                    if (moveres == 1) {
                        update_led_red();
                        ch = ESCAPE;
                    } else if (moveres == 2) {
                        update_led_green();
                        ch = ESCAPE;
                    }
                }
            }
        } else {
            moveres = move_snake(move_type);
            if (moveres == 1) {
                update_led_red();
                ch = ESCAPE;
            } else if (moveres == 2) {
                update_led_green();
                ch = ESCAPE;
            }
        }
        clear_board();
        draw_snake_and_food();
        update_display();

        if (ch == ESCAPE) {
            draw_end_game_screen();
            update_display();
            sleep(3);
            break;
        }

        clock_nanosleep(CLOCK_MONOTONIC, 0, &loop_delay, NULL);
    }
}

/* Starts main menu */
void start_main_menu() {
    init_frame_buffer();
    update_led_off();
    int r;
    char ch;
    int selected = 0;
    draw_main_menu(selected);
    update_display();
    while (1) {
        // read control character
        r = read(0, &ch, 1);
        if (r == 1) {
            if (ch == 'w') {
                selected = selected == 0 ? 0 : selected - 1;
            } else if (ch == 's') {
                selected = selected == 2 ? 2 : selected + 1;
            } else if (ch == RETURN) {
                switch (selected) {
                    case 0:
                        start_game();
                        break;
                    case 1:
                        start_options_menu();
                        break;
                    case 2:
                        return;
                }
            }
            init_frame_buffer();
            draw_main_menu(selected);
            update_display();
            update_led_off();
        }
    }
}

int main(int argc, char *argv[]) {
    printf("hello\n");
    // set new console settings so that it does not wait for enter or new input
    static struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;

    newt.c_lflag &= ~(ICANON);
    newt.c_cc[VMIN] = 0;
    newt.c_cc[VTIME] = 0;

    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    printf("terminal settings set\n");

    // main program below
    uint32_t val_line = 0xE0000000;
    int i;
    fb = (unsigned short *)malloc(320 * 480 * 2);
    fdes = &font_winFreeSystem14x16;

    // setup memory mapping
    mem_base = map_phys_address(SPILED_REG_BASE_PHYS, SPILED_REG_SIZE, 0);

    // if mapping fails exit with error code
    if (mem_base == NULL)
        exit(1);

    loop_delay.tv_sec = 0;
    loop_delay.tv_nsec = 20 * 1000 * 1000;

    // led strip loading animation
    for (i = 0; i < 33; i++) {
        *(volatile uint32_t *)(mem_base + SPILED_REG_LED_LINE_o) = val_line;
        val_line >>= 1;
        // printf("LED val 0x%x\n", *(volatile uint32_t *)(mem_base + SPILED_REG_LED_LINE_o));
        clock_nanosleep(CLOCK_MONOTONIC, 0, &loop_delay, NULL);
    }

    // init display
    parlcd_mem_base = map_phys_address(PARLCD_REG_BASE_PHYS, PARLCD_REG_SIZE, 0);
    if (parlcd_mem_base == NULL)
        exit(1);
    parlcd_hx8357_init(parlcd_mem_base);
    init_frame_buffer();
    update_display();

    // launch game
    start_main_menu();

    // before quitting
    init_frame_buffer();
    update_display();
    update_led_off();

    // set old console settings back
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    printf("terminal settings restored\n");
    free(fb);
    printf("goodbye\n");
    return 0;
}
