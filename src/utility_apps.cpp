#include "utility_apps.h"
#include "tools_screen.h"
#include "usb_sd.h"
#include <Arduino.h>
#include <LilyGoLib.h>
#include <SD.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

static lv_obj_t *s_calc_screen;
static lv_obj_t *s_calc_value;
static char s_calc_entry[24] = "0";
static double s_calc_accumulator = 0;
static char s_calc_operator = 0;
static bool s_calc_new_entry = true;

static lv_obj_t *s_editor_screen;
static lv_obj_t *s_editor_textarea;
static lv_obj_t *s_editor_keyboard;
static lv_obj_t *s_editor_status;
static char s_editor_path[96] = "/Notes/note.txt";

static lv_obj_t *s_explorer_screen;
static lv_obj_t *s_explorer_list;
static lv_obj_t *s_explorer_status;
static char s_explorer_path[96] = "/";
static char s_explorer_entries[40][96];
static bool s_explorer_is_dir[40];
static int s_explorer_count;

static lv_obj_t *s_snake_screen;
static lv_obj_t *s_snake_cells[12][12];
static lv_obj_t *s_snake_status;
static int s_snake_x[144], s_snake_y[144], s_snake_length;
static int s_snake_food_x, s_snake_food_y, s_snake_dx, s_snake_dy;
static bool s_snake_running;
static lv_coord_t s_snake_origin_x;

static lv_obj_t *s_tetris_screen;
static lv_obj_t *s_tetris_cells[12][10];
static lv_obj_t *s_tetris_status;
static uint8_t s_tetris_board[12][10];
static int s_tetris_piece, s_tetris_rotation, s_tetris_x, s_tetris_y, s_tetris_score;
static bool s_tetris_running;
static lv_coord_t s_tetris_origin_x;

static lv_obj_t *s_pacman_screen;
static lv_obj_t *s_pacman_board;
static lv_obj_t *s_pacman_status;
static bool s_pacman_pellets[15][15];
static int s_pacman_x, s_pacman_y, s_pacman_ghost_x, s_pacman_ghost_y;
static int s_pacman_dx, s_pacman_dy, s_pacman_score;
static bool s_pacman_running;

static lv_timer_t *s_snake_timer;
static lv_timer_t *s_tetris_timer;
static lv_timer_t *s_pacman_timer;

static void on_back(lv_event_t *)
{
    s_snake_running = false;
    s_tetris_running = false;
    s_pacman_running = false;
    if (s_pacman_timer) lv_timer_pause(s_pacman_timer);
    tools_screen_show();
}

static lv_obj_t *make_button(lv_obj_t *parent, const char *text,
                             int width, int height, int x, int y,
                             lv_event_cb_t callback, void *user_data = nullptr)
{
    lv_obj_t *button = lv_obj_create(parent);
    lv_obj_set_size(button, width, height);
    lv_obj_set_pos(button, x, y);
    lv_obj_set_style_bg_color(button, lv_color_make(0x20, 0x20, 0x20), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(button, lv_color_make(0x55, 0x55, 0x55), LV_PART_MAIN);
    lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(button, 6, LV_PART_MAIN);
    lv_obj_set_style_pad_all(button, 0, LV_PART_MAIN);
    lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *label = lv_label_create(button);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_center(label);
    lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
    if (callback) lv_obj_add_event_cb(button, callback, LV_EVENT_CLICKED, user_data);
    return button;
}

static lv_obj_t *new_screen(const char *title)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(screen, 0, LV_PART_MAIN);
    make_button(screen, "BACK", 64, 38, 122, 54, on_back);
    lv_obj_t *heading = lv_label_create(screen);
    lv_label_set_text(heading, title);
    lv_obj_set_style_text_color(heading, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(heading, &lv_font_montserrat_24, LV_PART_MAIN);
    lv_obj_align(heading, LV_ALIGN_TOP_MID, 0, 12);
    return screen;
}

static void calc_refresh()
{
    lv_label_set_text(s_calc_value, s_calc_entry);
}

static void calc_apply_pending()
{
    double value = strtod(s_calc_entry, nullptr);
    if (s_calc_operator == '+') s_calc_accumulator += value;
    else if (s_calc_operator == '-') s_calc_accumulator -= value;
    else if (s_calc_operator == '*') s_calc_accumulator *= value;
    else if (s_calc_operator == '/') {
        if (value == 0) {
            snprintf(s_calc_entry, sizeof(s_calc_entry), "Error");
            s_calc_operator = 0;
            return;
        }
        s_calc_accumulator /= value;
    }
    snprintf(s_calc_entry, sizeof(s_calc_entry), "%.10g", s_calc_accumulator);
}

static void on_calc_key(lv_event_t *event)
{
    const char *key = (const char *)lv_event_get_user_data(event);
    if (!key) return;
    if (!strcmp(key, "C")) {
        strcpy(s_calc_entry, "0");
        s_calc_accumulator = 0;
        s_calc_operator = 0;
        s_calc_new_entry = true;
    } else if (!strcmp(key, "=")) {
        if (s_calc_operator) {
            calc_apply_pending();
            s_calc_operator = 0;
            s_calc_new_entry = true;
        }
    } else if (!strcmp(key, "+") || !strcmp(key, "-") ||
               !strcmp(key, "*") || !strcmp(key, "/")) {
        if (!s_calc_new_entry && s_calc_operator) calc_apply_pending();
        else s_calc_accumulator = strtod(s_calc_entry, nullptr);
        s_calc_operator = key[0];
        s_calc_new_entry = true;
    } else if (!strcmp(key, "+/-")) {
        if (s_calc_entry[0] == '-') memmove(s_calc_entry, s_calc_entry + 1, strlen(s_calc_entry));
        else if (strcmp(s_calc_entry, "0")) {
            size_t len = strlen(s_calc_entry);
            if (len + 1 < sizeof(s_calc_entry)) {
                memmove(s_calc_entry + 1, s_calc_entry, len + 1);
                s_calc_entry[0] = '-';
            }
        }
    } else if (!strcmp(key, ".")) {
        if (s_calc_new_entry) strcpy(s_calc_entry, "0");
        if (!strchr(s_calc_entry, '.')) strlcat(s_calc_entry, ".", sizeof(s_calc_entry));
        s_calc_new_entry = false;
    } else {
        if (s_calc_new_entry || !strcmp(s_calc_entry, "Error")) strcpy(s_calc_entry, "");
        if (strlen(s_calc_entry) < sizeof(s_calc_entry) - 1) strlcat(s_calc_entry, key, sizeof(s_calc_entry));
        if (!s_calc_entry[0]) strcpy(s_calc_entry, "0");
        s_calc_new_entry = false;
    }
    calc_refresh();
}

static bool storage_available()
{
    return instance.isCardReady() && !usb_sd_is_running();
}

static void on_editor_textarea(lv_event_t *)
{
    lv_obj_clear_flag(s_editor_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void on_editor_keyboard(lv_event_t *event)
{
    lv_event_code_t code = lv_event_get_code(event);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL)
        lv_obj_add_flag(s_editor_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void on_editor_save(lv_event_t *)
{
    if (!storage_available()) {
        lv_label_set_text(s_editor_status, "SD unavailable / USB mounted");
        return;
    }
    if (!strncmp(s_editor_path, "/Notes/", 7)) SD.mkdir("/Notes");
    File file = SD.open(s_editor_path, FILE_WRITE);
    if (!file) {
        lv_label_set_text(s_editor_status, "Save failed");
        return;
    }
    const char *text = lv_textarea_get_text(s_editor_textarea);
    size_t written = file.write((const uint8_t *)text, strlen(text));
    file.close();
    lv_label_set_text(s_editor_status, written == strlen(text) ? "Saved" : "Save failed");
}

static void editor_load_path(const char *path)
{
    snprintf(s_editor_path, sizeof(s_editor_path), "%s", path ? path : "/Notes/note.txt");
    lv_label_set_text(s_editor_status, s_editor_path);
    lv_textarea_set_text(s_editor_textarea, "");
    if (!storage_available()) {
        lv_label_set_text(s_editor_status, "Draft only - SD unavailable / USB mounted");
        return;
    }
    File file = SD.open(s_editor_path, FILE_READ);
    if (!file) return;
    static char content[2048];
    size_t count = file.read((uint8_t *)content, sizeof(content) - 1);
    content[count] = '\0';
    file.close();
    lv_textarea_set_text(s_editor_textarea, content);
}

static void explorer_refresh();

static void on_explorer_up(lv_event_t *)
{
    if (!strcmp(s_explorer_path, "/")) return;
    char *slash = strrchr(s_explorer_path, '/');
    if (slash == s_explorer_path) s_explorer_path[1] = '\0';
    else if (slash) *slash = '\0';
    explorer_refresh();
}

static void on_explorer_entry(lv_event_t *event)
{
    int index = (int)(intptr_t)lv_event_get_user_data(event);
    if (index < 0 || index >= s_explorer_count) return;
    if (s_explorer_is_dir[index]) {
        snprintf(s_explorer_path, sizeof(s_explorer_path), "%s", s_explorer_entries[index]);
        explorer_refresh();
    } else {
        const char *ext = strrchr(s_explorer_entries[index], '.');
        if (ext && (!strcasecmp(ext, ".txt") || !strcasecmp(ext, ".md") ||
                    !strcasecmp(ext, ".log") || !strcasecmp(ext, ".csv"))) {
            text_editor_screen_show_path(s_explorer_entries[index]);
        } else {
            lv_label_set_text(s_explorer_status, "Text files only");
        }
    }
}

static void explorer_refresh()
{
    lv_obj_clean(s_explorer_list);
    s_explorer_count = 0;
    lv_label_set_text(s_explorer_status, s_explorer_path);
    if (!storage_available()) {
        lv_label_set_text(s_explorer_status, "SD unavailable / USB mounted");
        return;
    }
    File directory = SD.open(s_explorer_path);
    if (!directory || !directory.isDirectory()) {
        lv_label_set_text(s_explorer_status, "Cannot open folder");
        return;
    }
    File entry = directory.openNextFile();
    while (entry && s_explorer_count < 40) {
        const char *name = entry.name();
        bool is_dir = entry.isDirectory();
        char full_path[96];
        if (name[0] == '/') snprintf(full_path, sizeof(full_path), "%s", name);
        else if (!strcmp(s_explorer_path, "/")) snprintf(full_path, sizeof(full_path), "/%s", name);
        else snprintf(full_path, sizeof(full_path), "%s/%s", s_explorer_path, name);
        snprintf(s_explorer_entries[s_explorer_count], sizeof(s_explorer_entries[0]), "%s", full_path);
        s_explorer_is_dir[s_explorer_count] = is_dir;
        const char *display = strrchr(full_path, '/');
        display = display ? display + 1 : full_path;
        lv_obj_t *button = lv_btn_create(s_explorer_list);
        lv_obj_set_size(button, lv_pct(100), 38);
        lv_obj_set_style_bg_color(button, lv_color_make(0x18, 0x18, 0x18), LV_PART_MAIN);
        lv_obj_set_style_radius(button, 4, LV_PART_MAIN);
        lv_obj_add_event_cb(button, on_explorer_entry, LV_EVENT_CLICKED,
                            (void *)(intptr_t)s_explorer_count);
        lv_obj_t *label = lv_label_create(button);
        lv_label_set_text_fmt(label, "%s %s", is_dir ? "[+]" : "[ ]", display);
        lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_16, LV_PART_MAIN);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 4, 0);
        lv_obj_add_flag(label, LV_OBJ_FLAG_EVENT_BUBBLE);
        s_explorer_count++;
        entry.close();
        entry = directory.openNextFile();
    }
    entry.close();
    directory.close();
    if (s_explorer_count == 0) lv_label_set_text(s_explorer_status, "Empty folder");
}

static void snake_draw()
{
    for (int y = 0; y < 12; y++) for (int x = 0; x < 12; x++)
        lv_obj_set_style_bg_color(s_snake_cells[y][x], lv_color_make(0x08, 0x08, 0x08), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_snake_cells[s_snake_food_y][s_snake_food_x],
                              lv_color_make(0xFF, 0x55, 0x33), LV_PART_MAIN);
    for (int i = 0; i < s_snake_length; i++)
        lv_obj_set_style_bg_color(s_snake_cells[s_snake_y[i]][s_snake_x[i]],
                                  i == 0 ? lv_color_make(0x00, 0xDD, 0x88) : lv_color_make(0x00, 0x88, 0x55), LV_PART_MAIN);
}

static void snake_spawn_food()
{
    bool occupied;
    do {
        occupied = false;
        s_snake_food_x = random(0, 12);
        s_snake_food_y = random(0, 12);
        for (int i = 0; i < s_snake_length; i++)
            if (s_snake_x[i] == s_snake_food_x && s_snake_y[i] == s_snake_food_y) occupied = true;
    } while (occupied);
}

static void snake_tick(lv_timer_t *)
{
    if (!s_snake_running || lv_screen_active() != s_snake_screen) return;
    int nx = s_snake_x[0] + s_snake_dx, ny = s_snake_y[0] + s_snake_dy;
    bool eating = nx == s_snake_food_x && ny == s_snake_food_y;
    if (nx < 0 || nx >= 12 || ny < 0 || ny >= 12) s_snake_running = false;
    for (int i = 0; s_snake_running && i < s_snake_length - (eating ? 0 : 1); i++)
        if (s_snake_x[i] == nx && s_snake_y[i] == ny) s_snake_running = false;
    if (!s_snake_running) {
        lv_label_set_text(s_snake_status, "Game over - BACK to exit");
        return;
    }
    if (eating && s_snake_length < 144) s_snake_length++;
    for (int i = s_snake_length - 1; i > 0; i--) { s_snake_x[i] = s_snake_x[i - 1]; s_snake_y[i] = s_snake_y[i - 1]; }
    s_snake_x[0] = nx; s_snake_y[0] = ny;
    if (eating) snake_spawn_food();
    lv_label_set_text_fmt(s_snake_status, "Length %d", s_snake_length);
    snake_draw();
}

static void on_snake_gesture(lv_event_t *event)
{
    if (!s_snake_running) return;
    lv_indev_t *indev = lv_event_get_indev(event);
    if (!indev) return;
    lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    int dx = direction == LV_DIR_LEFT ? -1 : direction == LV_DIR_RIGHT ? 1 : 0;
    int dy = direction == LV_DIR_TOP ? -1 : direction == LV_DIR_BOTTOM ? 1 : 0;
    if ((dx || dy) && (dx != -s_snake_dx || dy != -s_snake_dy)) {
        s_snake_dx = dx;
        s_snake_dy = dy;
    }
}

static bool tetris_collides(int piece, int rotation, int px, int py)
{
    static const int8_t shapes[7][4][2] = {
        {{0,1},{1,1},{2,1},{3,1}}, {{1,0},{2,0},{1,1},{2,1}},
        {{1,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{0,1},{1,1}},
        {{0,0},{1,0},{1,1},{2,1}}, {{0,0},{0,1},{1,1},{2,1}},
        {{2,0},{0,1},{1,1},{2,1}},
    };
    for (int i = 0; i < 4; i++) {
        int x = shapes[piece][i][0], y = shapes[piece][i][1];
        for (int r = 0; r < rotation; r++) { int next_x = 3 - y; y = x; x = next_x; }
        x += px; y += py;
        if (x < 0 || x >= 10 || y >= 12) return true;
        if (y >= 0 && s_tetris_board[y][x]) return true;
    }
    return false;
}

static void tetris_draw()
{
    static const uint32_t colors[] = {0x000000,0x33CCFF,0xFFCC33,0xCC66FF,0x44DD66,0xFF5555,0x5599FF,0xFF9933};
    for (int y = 0; y < 12; y++) for (int x = 0; x < 10; x++) {
        uint8_t value = s_tetris_board[y][x];
        lv_color_t color = lv_color_make((colors[value] >> 16) & 255, (colors[value] >> 8) & 255, colors[value] & 255);
        lv_obj_set_style_bg_color(s_tetris_cells[y][x], color, LV_PART_MAIN);
    }
    if (!s_tetris_running) return;
    static const int8_t shapes[7][4][2] = {
        {{0,1},{1,1},{2,1},{3,1}}, {{1,0},{2,0},{1,1},{2,1}},
        {{1,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{0,1},{1,1}},
        {{0,0},{1,0},{1,1},{2,1}}, {{0,0},{0,1},{1,1},{2,1}},
        {{2,0},{0,1},{1,1},{2,1}},
    };
    for (int i = 0; i < 4; i++) {
        int x = shapes[s_tetris_piece][i][0], y = shapes[s_tetris_piece][i][1];
        for (int r = 0; r < s_tetris_rotation; r++) { int next_x = 3 - y; y = x; x = next_x; }
        x += s_tetris_x; y += s_tetris_y;
        if (y >= 0 && y < 12 && x >= 0 && x < 10)
            lv_obj_set_style_bg_color(s_tetris_cells[y][x], lv_color_make(0xFF, 0xFF, 0xFF), LV_PART_MAIN);
    }
}

static void tetris_spawn()
{
    s_tetris_piece = random(0, 7); s_tetris_rotation = 0; s_tetris_x = 3; s_tetris_y = -1;
    if (tetris_collides(s_tetris_piece, s_tetris_rotation, s_tetris_x, s_tetris_y)) {
        s_tetris_running = false;
        lv_label_set_text_fmt(s_tetris_status, "Game over - score %d", s_tetris_score);
    }
}

static void tetris_lock()
{
    static const int8_t shapes[7][4][2] = {
        {{0,1},{1,1},{2,1},{3,1}}, {{1,0},{2,0},{1,1},{2,1}},
        {{1,0},{0,1},{1,1},{2,1}}, {{1,0},{2,0},{0,1},{1,1}},
        {{0,0},{1,0},{1,1},{2,1}}, {{0,0},{0,1},{1,1},{2,1}},
        {{2,0},{0,1},{1,1},{2,1}},
    };
    for (int i = 0; i < 4; i++) {
        int x = shapes[s_tetris_piece][i][0], y = shapes[s_tetris_piece][i][1];
        for (int r = 0; r < s_tetris_rotation; r++) { int next_x = 3 - y; y = x; x = next_x; }
        x += s_tetris_x; y += s_tetris_y;
        if (y >= 0 && y < 12) s_tetris_board[y][x] = (uint8_t)(s_tetris_piece + 1);
    }
    for (int y = 11; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < 10; x++) if (!s_tetris_board[y][x]) full = false;
        if (full) {
            for (int row = y; row > 0; row--) memcpy(s_tetris_board[row], s_tetris_board[row - 1], 10);
            memset(s_tetris_board[0], 0, 10);
            s_tetris_score += 100;
            y++;
        }
    }
    tetris_spawn();
    if (s_tetris_running) lv_label_set_text_fmt(s_tetris_status, "Score %d", s_tetris_score);
}

static void tetris_tick(lv_timer_t *)
{
    if (!s_tetris_running || lv_screen_active() != s_tetris_screen) return;
    if (!tetris_collides(s_tetris_piece, s_tetris_rotation, s_tetris_x, s_tetris_y + 1)) s_tetris_y++;
    else tetris_lock();
    tetris_draw();
}

static void on_tetris_gesture(lv_event_t *event)
{
    if (!s_tetris_running) return;
    lv_indev_t *indev = lv_event_get_indev(event);
    if (!indev) return;
    lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    if (direction == LV_DIR_LEFT && !tetris_collides(s_tetris_piece, s_tetris_rotation, s_tetris_x - 1, s_tetris_y)) s_tetris_x--;
    else if (direction == LV_DIR_RIGHT && !tetris_collides(s_tetris_piece, s_tetris_rotation, s_tetris_x + 1, s_tetris_y)) s_tetris_x++;
    else if (direction == LV_DIR_TOP && !tetris_collides(s_tetris_piece, (s_tetris_rotation + 1) % 4, s_tetris_x, s_tetris_y)) s_tetris_rotation = (s_tetris_rotation + 1) % 4;
    else if (direction == LV_DIR_BOTTOM) {
        if (!tetris_collides(s_tetris_piece, s_tetris_rotation, s_tetris_x, s_tetris_y + 1)) s_tetris_y++;
    }
    tetris_draw();
}

static const char s_pacman_maze[15][16] = {
    "###############", "#.....#.......#", "#.###.#.###.#.#",
    "#o#...#...#...#", "#.#.#####.#.#.#", "#.............#",
    "###.#.###.#.###", "#...#...#...#.#", "#.#####.#.###.#",
    "#.......#.....#", "#.###.#.#####.#", "#...#.#...#...#",
    "#.#.#.###.#.#.#", "#.....#......o#", "###############",
};

static void pacman_draw_shape(lv_layer_t *layer, const lv_area_t *area,
                              lv_color_t color, int32_t radius)
{
    lv_draw_rect_dsc_t draw_dsc;
    lv_draw_rect_dsc_init(&draw_dsc);
    draw_dsc.bg_color = color;
    draw_dsc.bg_opa = LV_OPA_COVER;
    draw_dsc.border_width = 0;
    draw_dsc.radius = radius;
    lv_draw_rect(layer, &draw_dsc, area);
}

static void pacman_draw_event(lv_event_t *event)
{
    lv_layer_t *layer = lv_event_get_layer(event);
    if (!layer) return;

    lv_area_t board_area;
    lv_obj_get_coords(s_pacman_board, &board_area);
    for (int y = 0; y < 15; y++) {
        for (int x = 0; x < 15; x++) {
            lv_area_t cell_area = {
                board_area.x1 + x * 19,
                board_area.y1 + y * 19,
                board_area.x1 + (x + 1) * 19 - 1,
                board_area.y1 + (y + 1) * 19 - 1,
            };
            if (s_pacman_maze[y][x] == '#') {
                pacman_draw_shape(layer, &cell_area, lv_color_make(0x00, 0x55, 0xCC), 3);
            } else if (s_pacman_pellets[y][x]) {
                int32_t size = s_pacman_maze[y][x] == 'o' ? 9 : 4;
                int32_t center_x = board_area.x1 + x * 19 + 9;
                int32_t center_y = board_area.y1 + y * 19 + 9;
                lv_area_t pellet_area = {
                    center_x - size / 2,
                    center_y - size / 2,
                    center_x + (size - 1) / 2,
                    center_y + (size - 1) / 2,
                };
                pacman_draw_shape(layer, &pellet_area, lv_color_make(0xFF, 0xDD, 0x55), LV_RADIUS_CIRCLE);
            }
        }
    }

    int32_t player_x = board_area.x1 + s_pacman_x * 19 + 2;
    int32_t player_y = board_area.y1 + s_pacman_y * 19 + 2;
    lv_area_t player_area = {player_x, player_y, player_x + 14, player_y + 14};
    pacman_draw_shape(layer, &player_area, lv_color_make(0xFF, 0xEE, 0x00), LV_RADIUS_CIRCLE);

    int32_t ghost_x = board_area.x1 + s_pacman_ghost_x * 19 + 2;
    int32_t ghost_y = board_area.y1 + s_pacman_ghost_y * 19 + 2;
    lv_area_t ghost_area = {ghost_x, ghost_y, ghost_x + 14, ghost_y + 14};
    pacman_draw_shape(layer, &ghost_area, lv_color_make(0xFF, 0x55, 0xAA), 5);
}

static bool pacman_cell_open(int x, int y)
{
    return x >= 0 && x < 15 && y >= 0 && y < 15 && s_pacman_maze[y][x] != '#';
}

static void pacman_move_ghost()
{
    int dx = (s_pacman_x > s_pacman_ghost_x) - (s_pacman_x < s_pacman_ghost_x);
    int dy = (s_pacman_y > s_pacman_ghost_y) - (s_pacman_y < s_pacman_ghost_y);
    bool horizontal_first = abs(s_pacman_x - s_pacman_ghost_x) >=
                            abs(s_pacman_y - s_pacman_ghost_y);
    for (int attempt = 0; attempt < 2; attempt++) {
        bool horizontal = (attempt == 0) == horizontal_first;
        int next_x = s_pacman_ghost_x + (horizontal ? dx : 0);
        int next_y = s_pacman_ghost_y + (horizontal ? 0 : dy);
        if ((horizontal ? dx : dy) != 0 && pacman_cell_open(next_x, next_y)) {
            s_pacman_ghost_x = next_x;
            s_pacman_ghost_y = next_y;
            return;
        }
    }
}

static void pacman_tick(lv_timer_t *timer)
{
    if (lv_screen_active() != s_pacman_screen || !s_pacman_running) {
        lv_timer_pause(timer);
        return;
    }

    int old_x = s_pacman_x, old_y = s_pacman_y;
    int old_ghost_x = s_pacman_ghost_x, old_ghost_y = s_pacman_ghost_y;
    bool pellet_eaten = false;
    int next_x = s_pacman_x + s_pacman_dx;
    int next_y = s_pacman_y + s_pacman_dy;
    if (pacman_cell_open(next_x, next_y)) {
        s_pacman_x = next_x;
        s_pacman_y = next_y;
        if (s_pacman_pellets[next_y][next_x]) {
            s_pacman_pellets[next_y][next_x] = false;
            s_pacman_score += s_pacman_maze[next_y][next_x] == 'o' ? 50 : 10;
            pellet_eaten = true;
        }
    }
    pacman_move_ghost();

    if (s_pacman_x == s_pacman_ghost_x && s_pacman_y == s_pacman_ghost_y) {
        s_pacman_running = false;
        lv_label_set_text_fmt(s_pacman_status, "Caught - score %d", s_pacman_score);
        lv_timer_pause(timer);
    } else if (pellet_eaten) {
        lv_label_set_text_fmt(s_pacman_status, "Score %d", s_pacman_score);
    }

    if (s_pacman_x != old_x || s_pacman_y != old_y ||
        s_pacman_ghost_x != old_ghost_x || s_pacman_ghost_y != old_ghost_y) {
        lv_obj_invalidate(s_pacman_board);
    }
}

static void on_pacman_gesture(lv_event_t *event)
{
    if (!s_pacman_running) return;
    lv_indev_t *indev = lv_event_get_indev(event);
    if (!indev) return;
    lv_dir_t direction = lv_indev_get_gesture_dir(indev);
    s_pacman_dx = direction == LV_DIR_LEFT ? -1 : direction == LV_DIR_RIGHT ? 1 : 0;
    s_pacman_dy = direction == LV_DIR_TOP ? -1 : direction == LV_DIR_BOTTOM ? 1 : 0;
}

static void on_pacman_screen_unload(lv_event_t *)
{
    s_pacman_running = false;
    if (s_pacman_timer) lv_timer_pause(s_pacman_timer);
}

static void add_swipe_hint(lv_obj_t *screen, const char *text);

static void pacman_screen_create()
{
    s_pacman_screen = new_screen("PAC-MAN");
    s_pacman_status = lv_label_create(s_pacman_screen);
    lv_obj_set_style_text_color(s_pacman_status, lv_color_make(0xFF, 0xDD, 0x55), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_pacman_status, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(s_pacman_status, LV_ALIGN_TOP_MID, 0, 62);

    s_pacman_board = lv_obj_create(s_pacman_screen);
    lv_obj_set_size(s_pacman_board, 285, 285);
    lv_obj_set_pos(s_pacman_board,
                   (lv_display_get_horizontal_resolution(lv_display_get_default()) - 285) / 2,
                   92);
    lv_obj_set_style_bg_opa(s_pacman_board, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(s_pacman_board, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_pacman_board, 0, LV_PART_MAIN);
    lv_obj_clear_flag(s_pacman_board, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(s_pacman_board, pacman_draw_event, LV_EVENT_DRAW_MAIN, nullptr);
    lv_obj_add_event_cb(s_pacman_board, on_pacman_gesture, LV_EVENT_GESTURE, nullptr);
    lv_obj_add_event_cb(s_pacman_screen, on_pacman_screen_unload, LV_EVENT_SCREEN_UNLOAD_START, nullptr);

    add_swipe_hint(s_pacman_screen, "Swipe to move");
    s_pacman_timer = lv_timer_create(pacman_tick, 420, nullptr);
    lv_timer_pause(s_pacman_timer);
}

static void add_swipe_hint(lv_obj_t *screen, const char *text)
{
    lv_obj_t *hint = lv_label_create(screen);
    lv_label_set_text(hint, text);
    lv_obj_set_style_text_color(hint, lv_color_make(0x77, 0x77, 0x77), LV_PART_MAIN);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -18);
}

void utility_apps_create()
{
    s_calc_screen = new_screen("CALCULATOR");
    s_calc_value = lv_label_create(s_calc_screen);
    lv_obj_set_size(s_calc_value, 300, 72);
    lv_obj_set_style_text_color(s_calc_value, lv_color_make(0x00, 0xDD, 0x88), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_calc_value, &lv_font_montserrat_32, LV_PART_MAIN);
    lv_obj_set_style_text_align(s_calc_value, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_label_set_text(s_calc_value, "0");
    lv_obj_align(s_calc_value, LV_ALIGN_TOP_MID, 0, 96);
    static const char *keys[20] = {"C","+/-","/","*","7","8","9","-","4","5","6","+","1","2","3","=","0","0",".","="};
    static const int grid_keys[20] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19};
    for (int row = 0; row < 5; row++) for (int col = 0; col < 4; col++) {
        int index = grid_keys[row * 4 + col];
        if (row == 4 && col == 1) continue;
        if (row == 4 && col == 3) continue;
        const char *key = keys[index];
        int x = 66 + col * 80, y = 150 + row * 46;
        int height = row == 3 && col == 3 ? 86 : 40;
        make_button(s_calc_screen, key, col == 0 && row == 4 ? 152 : 72, height, x, y, on_calc_key, (void *)key);
    }

    s_editor_screen = new_screen("TEXT EDITOR");
    make_button(s_editor_screen, "SAVE", 72, 38, 250, 54, on_editor_save);
    s_editor_status = lv_label_create(s_editor_screen);
    lv_obj_set_width(s_editor_status, 300);
    lv_obj_set_style_text_color(s_editor_status, lv_color_make(0x99, 0x99, 0x99), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_editor_status, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(s_editor_status, s_editor_path);
    lv_obj_align(s_editor_status, LV_ALIGN_TOP_MID, 0, 98);
    s_editor_textarea = lv_textarea_create(s_editor_screen);
    lv_obj_set_size(s_editor_textarea, 410, 132);
    lv_obj_align(s_editor_textarea, LV_ALIGN_TOP_MID, 0, 120);
    lv_textarea_set_one_line(s_editor_textarea, false);
    lv_textarea_set_max_length(s_editor_textarea, 2047);
    lv_textarea_set_placeholder_text(s_editor_textarea, "Write a note...");
    lv_obj_add_event_cb(s_editor_textarea, on_editor_textarea, LV_EVENT_CLICKED, nullptr);
    s_editor_keyboard = lv_keyboard_create(s_editor_screen);
    lv_keyboard_set_textarea(s_editor_keyboard, s_editor_textarea);
    lv_obj_set_size(s_editor_keyboard, 440, 190);
    lv_obj_align(s_editor_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s_editor_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_editor_keyboard, on_editor_keyboard, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(s_editor_keyboard, on_editor_keyboard, LV_EVENT_CANCEL, nullptr);

    s_explorer_screen = new_screen("FILE EXPLORER");
    make_button(s_explorer_screen, "UP", 54, 38, 188, 54, on_explorer_up);
    make_button(s_explorer_screen, "REFRESH", 78, 38, 246, 54, [](lv_event_t *) { explorer_refresh(); });
    s_explorer_status = lv_label_create(s_explorer_screen);
    lv_obj_set_style_text_color(s_explorer_status, lv_color_make(0x99, 0x99, 0x99), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_explorer_status, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_label_set_text(s_explorer_status, "/");
    lv_obj_align(s_explorer_status, LV_ALIGN_TOP_MID, 0, 98);
    s_explorer_list = lv_obj_create(s_explorer_screen);
    lv_obj_set_size(s_explorer_list, 410, 300);
    lv_obj_align(s_explorer_list, LV_ALIGN_TOP_MID, 0, 122);
    lv_obj_set_style_bg_color(s_explorer_list, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_explorer_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(s_explorer_list, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_row(s_explorer_list, 4, LV_PART_MAIN);
    lv_obj_set_flex_flow(s_explorer_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_scroll_dir(s_explorer_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_explorer_list, LV_SCROLLBAR_MODE_AUTO);

    s_snake_screen = new_screen("SNAKE");
    s_snake_status = lv_label_create(s_snake_screen);
    lv_obj_set_style_text_color(s_snake_status, lv_color_make(0x00, 0xDD, 0x88), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_snake_status, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(s_snake_status, LV_ALIGN_TOP_MID, 0, 62);
    s_snake_origin_x = (lv_display_get_horizontal_resolution(lv_display_get_default()) - 288) / 2;
    for (int y = 0; y < 12; y++) for (int x = 0; x < 12; x++) {
        lv_obj_t *cell = lv_obj_create(s_snake_screen);
        s_snake_cells[y][x] = cell;
        lv_obj_set_size(cell, 24, 24);
        lv_obj_set_pos(cell, s_snake_origin_x + x * 24, 90 + y * 24);
        lv_obj_set_style_bg_color(cell, lv_color_make(0x08, 0x08, 0x08), LV_PART_MAIN);
        lv_obj_set_style_border_color(cell, lv_color_make(0x22, 0x22, 0x22), LV_PART_MAIN);
        lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN);
        lv_obj_set_style_pad_all(cell, 0, LV_PART_MAIN);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    }
    add_swipe_hint(s_snake_screen, "Swipe to steer");
    lv_obj_add_event_cb(s_snake_screen, on_snake_gesture, LV_EVENT_GESTURE, nullptr);
    s_snake_timer = lv_timer_create(snake_tick, 360, nullptr);

    s_tetris_screen = new_screen("TETRIS");
    s_tetris_status = lv_label_create(s_tetris_screen);
    lv_obj_set_style_text_color(s_tetris_status, lv_color_make(0xFF, 0xCC, 0x33), LV_PART_MAIN);
    lv_obj_set_style_text_font(s_tetris_status, &lv_font_montserrat_16, LV_PART_MAIN);
    lv_obj_align(s_tetris_status, LV_ALIGN_TOP_MID, 0, 62);
    s_tetris_origin_x = (lv_display_get_horizontal_resolution(lv_display_get_default()) - 240) / 2;
    for (int y = 0; y < 12; y++) for (int x = 0; x < 10; x++) {
        lv_obj_t *cell = lv_obj_create(s_tetris_screen);
        s_tetris_cells[y][x] = cell;
        lv_obj_set_size(cell, 24, 24);
        lv_obj_set_pos(cell, s_tetris_origin_x + x * 24, 90 + y * 24);
        lv_obj_set_style_bg_color(cell, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_border_color(cell, lv_color_make(0x33, 0x33, 0x33), LV_PART_MAIN);
        lv_obj_set_style_border_width(cell, 1, LV_PART_MAIN);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
    }
    add_swipe_hint(s_tetris_screen, "Swipe sideways to move, up to rotate, down to drop");
    lv_obj_add_event_cb(s_tetris_screen, on_tetris_gesture, LV_EVENT_GESTURE, nullptr);
    s_tetris_timer = lv_timer_create(tetris_tick, 520, nullptr);

}

void calculator_screen_show()
{
    strcpy(s_calc_entry, "0"); s_calc_accumulator = 0; s_calc_operator = 0; s_calc_new_entry = true;
    calc_refresh(); lv_scr_load(s_calc_screen);
}

void text_editor_screen_show()
{
    text_editor_screen_show_path("/Notes/note.txt");
}

void text_editor_screen_show_path(const char *path)
{
    editor_load_path(path);
    lv_obj_add_flag(s_editor_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_scr_load(s_editor_screen);
}

void file_explorer_screen_show()
{
    strcpy(s_explorer_path, "/");
    explorer_refresh();
    lv_scr_load(s_explorer_screen);
}

void snake_screen_show()
{
    s_snake_running = true; s_snake_length = 3; s_snake_dx = 1; s_snake_dy = 0;
    s_snake_x[0] = 5; s_snake_y[0] = 6; s_snake_x[1] = 4; s_snake_y[1] = 6; s_snake_x[2] = 3; s_snake_y[2] = 6;
    snake_spawn_food(); snake_draw(); lv_label_set_text(s_snake_status, "Length 3"); lv_scr_load(s_snake_screen);
}

void tetris_screen_show()
{
    memset(s_tetris_board, 0, sizeof(s_tetris_board)); s_tetris_score = 0; s_tetris_running = true;
    tetris_spawn(); tetris_draw(); lv_label_set_text(s_tetris_status, "Score 0"); lv_scr_load(s_tetris_screen);
}

void pacman_screen_show()
{
    if (!s_pacman_screen) pacman_screen_create();
    for (int y = 0; y < 15; y++) {
        for (int x = 0; x < 15; x++) {
            s_pacman_pellets[y][x] = s_pacman_maze[y][x] == '.' || s_pacman_maze[y][x] == 'o';
        }
    }
    s_pacman_x = 1;
    s_pacman_y = 1;
    s_pacman_ghost_x = 13;
    s_pacman_ghost_y = 13;
    s_pacman_dx = 0;
    s_pacman_dy = 0;
    s_pacman_score = 0;
    s_pacman_running = true;
    lv_label_set_text(s_pacman_status, "Score 0");
    lv_obj_invalidate(s_pacman_board);
    lv_scr_load(s_pacman_screen);
    lv_timer_resume(s_pacman_timer);
}
