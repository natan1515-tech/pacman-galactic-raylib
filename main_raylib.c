/*
 * ============================================================
 *  PAC-MAN GALACTIC — Raylib Edition
 *  Tema espacial inspirado em Star Wars, com mapa Death Star,
 *  Jedi Pac, vilões espaciais, efeitos neon e HUD estilizado.
 * ============================================================
 */

#include "raylib.h"
#include "pacman.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TILE_SIZE 24
#define TOP_BAR   86
#define LEFT_PAD  28
#define SIDE_W    310
#define SCREEN_W  (LEFT_PAD * 2 + MAP_W * TILE_SIZE + SIDE_W)
#define SCREEN_H  (TOP_BAR + MAP_H * TILE_SIZE + 30)

typedef struct { float x, y, speed, radius; Color color; } Star;
typedef struct { Vector2 pos, vel; float life, maxLife, radius; Color color; int active; } Particle;
typedef struct { int x, y, type, timer, active; } BonusItem;

#define STAR_COUNT 120
#define PARTICLE_COUNT 260
static Star stars[STAR_COUNT];
static Particle particles[PARTICLE_COUNT];
static BonusItem g_bonus = {0};
static int g_bonus_spawn_timer = 0;
static int g_shield_timer = 0;
static int g_dash_timer = 0;
static int g_freeze_timer = 0;
static int g_multiplier_timer = 0;
static int g_ghost_release_timer = 7200; /* 2 minutos para liberar os reforcos */
static float g_screen_shake = 0.0f;
static Texture2D g_splash;
static int g_has_splash = 0;

static Color C_SPACE     = { 5, 7, 18, 255 };
static Color C_SPACE_2   = { 14, 18, 42, 255 };
static Color C_TRENCH    = { 38, 42, 58, 255 };
static Color C_WALL      = { 64, 76, 105, 255 };
static Color C_WALL_HI   = { 120, 158, 210, 255 };
static Color C_NEON_BLUE = { 70, 210, 255, 255 };
static Color C_NEON_RED  = { 255, 52, 70, 255 };
static Color C_GOLD      = { 255, 218, 75, 255 };
static Color C_TEXT      = { 240, 245, 255, 255 };
static Color C_DIM       = { 145, 158, 185, 255 };
static Color C_PANEL     = { 13, 18, 32, 235 };
static Color C_PELLET    = { 156, 230, 255, 255 };
static Color C_FORCE     = { 75, 255, 165, 255 };

/* Poderes extras */
static int g_force_blast_cd = 0;   /* cooldown do Pulso da Força */
static int g_force_slow_timer = 0; /* deixa os vilões mais lentos */
static int g_force_spawn_timer = 0;/* cria cristais extras no mapa */

static int cell_x(int x) { return LEFT_PAD + x * TILE_SIZE; }
static int cell_y(int y) { return TOP_BAR + y * TILE_SIZE; }

/* Stubs exigidos pelo model.c original de terminal */
void sleep_ms(int ms) { (void)ms; }
void enable_ansi(void) {}
int kbhit_custom(void) { return 0; }
int getch_custom(void) { return 0; }
int manhattan(int x1, int y1, int x2, int y2) { return abs(x1 - x2) + abs(y1 - y2); }

static void init_stars(void) {
    for (int i = 0; i < STAR_COUNT; i++) {
        stars[i].x = (float)GetRandomValue(0, SCREEN_W);
        stars[i].y = (float)GetRandomValue(0, SCREEN_H);
        stars[i].speed = (float)GetRandomValue(18, 95) / 10.0f;
        stars[i].radius = (float)GetRandomValue(1, 3);
        stars[i].color = (Color){ (unsigned char)GetRandomValue(130, 255), (unsigned char)GetRandomValue(150, 255), 255, (unsigned char)GetRandomValue(95, 210) };
    }
}

static void draw_space_background(void) {
    ClearBackground(C_SPACE);
    for (int y = 0; y < SCREEN_H; y += 3) {
        float t = (float)y / (float)SCREEN_H;
        Color c = ColorLerp(C_SPACE, C_SPACE_2, t);
        DrawRectangle(0, y, SCREEN_W, 3, c);
    }

    float dt = GetFrameTime();
    for (int i = 0; i < STAR_COUNT; i++) {
        stars[i].x -= stars[i].speed * dt * 10.0f;
        if (stars[i].x < 0) { stars[i].x = SCREEN_W; stars[i].y = (float)GetRandomValue(0, SCREEN_H); }
        DrawCircleV((Vector2){stars[i].x, stars[i].y}, stars[i].radius, stars[i].color);
    }
}


static Color bonus_color(int type) {
    switch (type) {
        case 0: return (Color){80, 190, 255, 255};   /* escudo */
        case 1: return (Color){255, 230, 80, 255};   /* dash */
        case 2: return (Color){120, 240, 255, 255};  /* congelar */
        default: return (Color){255, 120, 225, 255}; /* multiplicador */
    }
}

static const char *bonus_name(int type) {
    switch (type) {
        case 0: return "ESCUDO";
        case 1: return "DASH";
        case 2: return "FREEZE";
        default: return "X2";
    }
}

static void add_particle(float x, float y, Color color, int burst) {
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        if (!particles[i].active) {
            float ang = (float)GetRandomValue(0, 628) / 100.0f;
            float spd = (float)GetRandomValue(burst ? 90 : 25, burst ? 220 : 90) / 10.0f;
            particles[i].active = 1;
            particles[i].pos = (Vector2){x, y};
            particles[i].vel = (Vector2){cosf(ang) * spd, sinf(ang) * spd};
            particles[i].life = particles[i].maxLife = (float)GetRandomValue(30, burst ? 85 : 55) / 100.0f;
            particles[i].radius = (float)GetRandomValue(2, burst ? 6 : 4);
            particles[i].color = color;
            return;
        }
    }
}

static void burst_particles(float x, float y, Color color, int count) {
    for (int i = 0; i < count; i++) add_particle(x, y, color, 1);
}

static void update_particles(void) {
    float dt = GetFrameTime();
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        if (!particles[i].active) continue;
        particles[i].life -= dt;
        particles[i].pos.x += particles[i].vel.x * dt * 10.0f;
        particles[i].pos.y += particles[i].vel.y * dt * 10.0f;
        particles[i].vel.y += 8.0f * dt;
        if (particles[i].life <= 0.0f) particles[i].active = 0;
    }
    if (g_screen_shake > 0.0f) g_screen_shake -= dt * 18.0f;
    if (g_screen_shake < 0.0f) g_screen_shake = 0.0f;
}

static void draw_particles(void) {
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        if (!particles[i].active) continue;
        float a = particles[i].life / particles[i].maxLife;
        DrawCircleV(particles[i].pos, particles[i].radius * (0.6f + a), Fade(particles[i].color, a));
    }
}

static void spawn_bonus_item(GameModel *m) {
    for (int tries = 0; tries < 100; tries++) {
        int rx = GetRandomValue(2, MAP_W - 3);
        int ry = GetRandomValue(2, MAP_H - 3);
        if (m->grid[ry][rx] == TILE_EMPTY || m->grid[ry][rx] == TILE_PELLET) {
            g_bonus.x = rx;
            g_bonus.y = ry;
            g_bonus.type = GetRandomValue(0, 3);
            g_bonus.timer = 520;
            g_bonus.active = 1;
            burst_particles(cell_x(rx) + TILE_SIZE/2, cell_y(ry) + TILE_SIZE/2, bonus_color(g_bonus.type), 20);
            return;
        }
    }
}

static void draw_bonus_item(void) {
    if (!g_bonus.active) return;
    float cx = cell_x(g_bonus.x) + TILE_SIZE / 2.0f;
    float cy = cell_y(g_bonus.y) + TILE_SIZE / 2.0f;
    Color c = bonus_color(g_bonus.type);
    float pulse = 1.0f + 0.18f * sinf((float)GetTime() * 9.0f);
    DrawCircleGradient((int)cx, (int)cy, 18 * pulse, Fade(c, 0.75f), Fade(c, 0.0f));
    if (g_bonus.type == 0) {
        DrawCircleLines((int)cx, (int)cy, 10, c);
        DrawCircleLines((int)cx, (int)cy, 6, WHITE);
    } else if (g_bonus.type == 1) {
        DrawTriangle((Vector2){cx-8,cy+9}, (Vector2){cx+2,cy-10}, (Vector2){cx+5,cy-1}, c);
        DrawTriangle((Vector2){cx-1,cy+10}, (Vector2){cx+9,cy-9}, (Vector2){cx+10,cy+1}, WHITE);
    } else if (g_bonus.type == 2) {
        DrawText("❄", (int)cx - 8, (int)cy - 13, 22, c);
    } else {
        DrawText("x2", (int)cx - 11, (int)cy - 10, 20, c);
    }
}

static void apply_bonus(GameModel *m) {
    if (!g_bonus.active) return;
    if (m->pacman.x != g_bonus.x || m->pacman.y != g_bonus.y) return;
    Color c = bonus_color(g_bonus.type);
    burst_particles(cell_x(g_bonus.x) + TILE_SIZE/2, cell_y(g_bonus.y) + TILE_SIZE/2, c, 42);
    g_screen_shake = 3.5f;
    m->score += 250;
    if (g_bonus.type == 0) g_shield_timer = 420;
    else if (g_bonus.type == 1) g_dash_timer = 240;
    else if (g_bonus.type == 2) g_freeze_timer = 360;
    else g_multiplier_timer = 520;
    g_bonus.active = 0;
}

static void repel_with_shield(GameModel *m) {
    if (g_shield_timer <= 0) return;
    for (int i = 0; i < GHOSTS; i++) {
        Ghost *g = &m->ghosts[i];
        if (abs(g->e.x - m->pacman.x) + abs(g->e.y - m->pacman.y) <= 1) {
            burst_particles(cell_x(g->e.x) + TILE_SIZE/2, cell_y(g->e.y) + TILE_SIZE/2, bonus_color(0), 18);
            g->e.x = g->e.start_x;
            g->e.y = g->e.start_y;
            g->e.dx = 0; g->e.dy = 0;
            m->score += 100;
            g_screen_shake = 2.2f;
        }
    }
}

static void draw_power_auras(GameModel *m) {
    float cx = cell_x(m->pacman.x) + TILE_SIZE / 2.0f;
    float cy = cell_y(m->pacman.y) + TILE_SIZE / 2.0f;
    if (g_shield_timer > 0) {
        float r = 24 + 4 * sinf((float)GetTime() * 8.0f);
        DrawCircleLines((int)cx, (int)cy, r, bonus_color(0));
        DrawCircleGradient((int)cx, (int)cy, r + 5, Fade(bonus_color(0), 0.24f), Fade(bonus_color(0), 0.0f));
    }
    if (g_dash_timer > 0) {
        for (int i = 1; i <= 3; i++) DrawCircle((int)(cx - m->current_dx * i * 9), (int)(cy - m->current_dy * i * 9), 5 - i, Fade(bonus_color(1), 0.45f));
    }
    if (g_multiplier_timer > 0) DrawText("x2", (int)cx + 14, (int)cy - 26, 18, bonus_color(3));
}

static void draw_center_text(const char *text, int y, int size, Color color) {
    int w = MeasureText(text, size);
    DrawText(text, (SCREEN_W - w) / 2, y, size, color);
}

static void rebuild_graph(GameModel *m) {
    if (m->maze_graph) graph_free(m->maze_graph);
    m->maze_graph = graph_create(MAP_W * MAP_H);
    graph_build_from_map(m->maze_graph, m->grid);
}

static void recount_pellets(GameModel *m) {
    m->pellets_left = 0;
    m->total_pellets = 0;
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            if (m->grid[y][x] == TILE_PELLET || m->grid[y][x] == TILE_POWER) {
                m->pellets_left++;
                m->total_pellets++;
            }
        }
    }
}

static void apply_galactic_map(GameModel *m) {
    static const char *death_star[MAP_H] = {
        "############################",
        "#o......##....##......o....#",
        "#.####..##.##.##..####.###.#",
        "#......#...##...#..........#",
        "###.##.#.######.#.##.#######",
        "#...##...#....#...##.......#",
        "#.######.#.##.#.######.###.#",
        "#........#.##.#........#...#",
        "#.####.###.##.###.####.#.#.#",
        "#.#....#........#....#...#.#",
        "#.#.##.#.######.#.##.#####.#",
        "#...##...#    #...##.......#",
        "#####.##.#-##-#.##.#####.###",
        "T.....##.#GGGG#.##.....o...T",
        "#####.##.#GGGG#.##.#####.###",
        "#........#    #.............#",
        "#.####.##########.####.###.#",
        "#....#............#....#...#",
        "####.#.####..####.#.##.#.#.#",
        "#......#........#...##...#.#",
        "#.######.######.######.###.#",
        "#...........##.............#",
        "#.####.####.##.####.####.#.#",
        "#o..#.................#...o#",
        "###.#.##.########.##.#.####",
        "#...#.##....##....##.#....#",
        "#.###.#####.##.#####.###..#",
        "#..........................#",
        "#.##########.##.##########.#",
        "#............##............#",
        "############################"
    };

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            char c = death_star[y][x];
            switch (c) {
                case '#': m->grid[y][x] = TILE_WALL; break;
                case '.': m->grid[y][x] = TILE_PELLET; break;
                case 'o': m->grid[y][x] = TILE_POWER; break;
                case '-': m->grid[y][x] = TILE_DOOR; break;
                case 'T': m->grid[y][x] = TILE_TUNNEL; break;
                default:  m->grid[y][x] = TILE_EMPTY; break;
            }
        }
    }

    m->pacman = (Entity){14, 27, 0, 0, 14, 27};
    /* Vilões começam fora da caixinha para caçar pelo mapa inteiro */
    m->ghosts[0].e = (Entity){13, 11, -1, 0, 13, 11};
    m->ghosts[1].e = (Entity){14, 11, 1, 0, 14, 11};
    m->ghosts[2].e = (Entity){12, 15, 0, 1, 12, 15};
    m->ghosts[3].e = (Entity){15, 15, 0, 1, 15, 15};
    m->ghosts[0].symbol = 'V';
    m->ghosts[1].symbol = 'T';
    m->ghosts[2].symbol = 'G';
    m->ghosts[3].symbol = 'M';
    for (int i = 0; i < GHOSTS; i++) {
        m->ghosts[i].vulnerable = 0;
        m->ghosts[i].eaten = 0;
        m->ghosts[i].in_house = 0;
        m->ghosts[i].release_timer = 0;
    }
    m->ghosts[0].in_house = 0;
    m->ghosts[0].release_timer = 0;

    recount_pellets(m);
    rebuild_graph(m);
}

static void start_game(GameModel *m) {
    model_load_map(m);
    apply_galactic_map(m);
    m->score = 0;
    m->lives = START_LIVES;
    m->level = 1;
    m->state = STATE_PLAYING;
    m->running = 1;
    m->ready_timer = 35;
    g_force_blast_cd = 0;
    g_force_slow_timer = 0;
    g_force_spawn_timer = 0;
    g_bonus_spawn_timer = 0;
    g_bonus.active = 0;
    g_shield_timer = 0;
    g_dash_timer = 0;
    g_freeze_timer = 0;
    g_multiplier_timer = 0;
    g_ghost_release_timer = 7200;
    g_screen_shake = 0.0f;
}


static void draw_menu(GameModel *m) {
    (void)m;
    draw_space_background();

    if (g_has_splash) {
        float fade = (float)GetTime() / 1.2f;
        if (fade > 1.0f) fade = 1.0f;

        DrawTexturePro(
            g_splash,
            (Rectangle){0, 0, (float)g_splash.width, (float)g_splash.height},
            (Rectangle){0, 0, (float)SCREEN_W, (float)SCREEN_H},
            (Vector2){0, 0},
            0.0f,
            Fade(WHITE, fade)
        );

        /* Brilho/pisca extra para reforçar o comando de início */
        if (((int)(GetTime() * 2.0)) % 2 == 0) {
            const char *msg = "PRESSIONE ENTER PARA INICIAR";
            int size = 26;
            int w = MeasureText(msg, size);
            int y = SCREEN_H - 76;
            DrawRectangleRounded((Rectangle){SCREEN_W/2 - w/2 - 28, y - 14, w + 56, 54}, 0.35f, 16, (Color){0, 10, 25, 145});
            DrawText(msg, SCREEN_W/2 - w/2, y, size, C_GOLD);
        }
        return;
    }

    DrawCircleGradient(SCREEN_W/2, 178, 130, (Color){80,90,115,255}, (Color){22,24,36,0});
    DrawCircleLines(SCREEN_W/2, 178, 132, (Color){150,170,200,180});
    DrawCircleLines(SCREEN_W/2 + 24, 142, 28, (Color){180,200,225,170});

    draw_center_text("PAC-MAN GALACTIC", 76, 54, C_GOLD);
    draw_center_text("A ASCENSÃO DO DARTH PAC NO LABIRINTO", 140, 22, C_TEXT);

    DrawRectangleRounded((Rectangle){SCREEN_W/2 - 270, 252, 540, 210}, 0.12f, 16, C_PANEL);
    DrawRectangleLinesEx((Rectangle){SCREEN_W/2 - 270, 252, 540, 210}, 2, C_NEON_BLUE);
    draw_center_text("ENTER  - iniciar missao", 292, 28, C_TEXT);
    draw_center_text("WASD ou SETAS - mover Darth Pac", 335, 24, C_DIM);
    draw_center_text("SABRE VERDE = power-up da Forca", 372, 24, C_FORCE);
    draw_center_text("E = Pulso da Forca: deixa vilões vulneráveis", 405, 22, C_FORCE);
    draw_center_text("ESC - sair", 435, 22, C_DIM);

    draw_center_text("Tema espacial, vilões galácticos, mapa novo e efeitos neon", SCREEN_H - 70, 20, C_DIM);
}

static void draw_tile_floor(int sx, int sy, int x, int y) {
    Color base = ((x + y) % 2 == 0) ? (Color){17, 21, 35, 255} : (Color){12, 16, 28, 255};
    DrawRectangle(sx, sy, TILE_SIZE, TILE_SIZE, base);
    DrawRectangleLines(sx, sy, TILE_SIZE, TILE_SIZE, (Color){25, 34, 56, 80});
}

static void draw_map(GameModel *m) {
    Rectangle board = { LEFT_PAD - 10, TOP_BAR - 10, MAP_W * TILE_SIZE + 20, MAP_H * TILE_SIZE + 20 };
    DrawRectangleRounded(board, 0.02f, 8, (Color){6, 9, 20, 235});
    DrawRectangleLinesEx(board, 3, C_NEON_BLUE);

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            int sx = cell_x(x), sy = cell_y(y), t = m->grid[y][x];
            draw_tile_floor(sx, sy, x, y);

            if (t == TILE_WALL) {
                DrawRectangleRounded((Rectangle){sx + 1, sy + 1, TILE_SIZE - 2, TILE_SIZE - 2}, 0.22f, 8, C_TRENCH);
                DrawRectangleLinesEx((Rectangle){sx + 2, sy + 2, TILE_SIZE - 4, TILE_SIZE - 4}, 1, C_WALL);
                if ((x + y) % 3 == 0) DrawRectangle(sx + 5, sy + 5, TILE_SIZE - 10, 2, C_WALL_HI);
                if ((x * 7 + y) % 5 == 0) DrawCircle(sx + TILE_SIZE - 6, sy + 6, 2, C_NEON_BLUE);
            } else if (t == TILE_DOOR) {
                DrawRectangle(sx + 2, sy + TILE_SIZE/2 - 3, TILE_SIZE - 4, 6, C_NEON_RED);
            } else if (t == TILE_PELLET) {
                DrawCircleGradient(sx + TILE_SIZE/2, sy + TILE_SIZE/2, 4, WHITE, C_PELLET);
            } else if (t == TILE_POWER) {
                float pulse = 1.0f + 0.25f * sinf((float)GetTime() * 7.0f);
                DrawCircleGradient(sx + TILE_SIZE/2, sy + TILE_SIZE/2, 10 * pulse, C_FORCE, (Color){0,255,120,0});
                DrawRectangle(sx + TILE_SIZE/2 - 2, sy + 5, 4, TILE_SIZE - 10, C_FORCE);
                DrawCircle(sx + TILE_SIZE/2, sy + TILE_SIZE/2, 3, WHITE);
            } else if (t == TILE_TUNNEL) {
                DrawRectangle(sx, sy + 6, TILE_SIZE, TILE_SIZE - 12, (Color){35, 55, 95, 255});
                DrawRectangleLines(sx, sy + 6, TILE_SIZE, TILE_SIZE - 12, C_NEON_BLUE);
            }
        }
    }
}

static void draw_darth_pac(GameModel *m) {
    float cx = cell_x(m->pacman.x) + TILE_SIZE / 2.0f;
    float cy = cell_y(m->pacman.y) + TILE_SIZE / 2.0f;
    float r = TILE_SIZE * 0.44f;
    int dx = m->current_dx, dy = m->current_dy;
    if (dx == 0 && dy == 0) dx = 1;

    /* Aura vermelha estilo Lorde Sith */
    DrawCircleGradient((int)cx, (int)cy, r + 9, (Color){255, 20, 45, 100}, (Color){255, 20, 45, 0});

    /* Corpo/cabeça preta do Pac-Man Sith */
    DrawCircle((int)cx, (int)cy, r, (Color){12, 12, 18, 255});
    DrawCircleLines((int)cx, (int)cy, r, (Color){90, 95, 115, 255});

    /* Boca do Pac-Man */
    float mouth = 0.50f + 0.18f * sinf((float)GetTime() * 13.0f);
    Vector2 a = { cx, cy };
    Vector2 b = { cx + dx * r * 1.25f - dy * r * mouth, cy + dy * r * 1.25f + dx * r * mouth };
    Vector2 c = { cx + dx * r * 1.25f + dy * r * mouth, cy + dy * r * 1.25f - dx * r * mouth };
    DrawTriangle(a, b, c, C_SPACE);

    /* Capacete inspirado em Darth Vader */
    DrawCircle((int)cx, (int)(cy - 5), 7, (Color){22, 22, 30, 255});
    DrawRectangleRounded((Rectangle){cx - 9, cy - 3, 18, 13}, 0.30f, 8, (Color){18, 18, 25, 255});
    DrawTriangle((Vector2){cx - 9, cy + 2}, (Vector2){cx - 15, cy + 13}, (Vector2){cx - 3, cy + 13}, (Color){10,10,16,255});
    DrawTriangle((Vector2){cx + 9, cy + 2}, (Vector2){cx + 15, cy + 13}, (Vector2){cx + 3, cy + 13}, (Color){10,10,16,255});

    /* Olhos e respirador */
    DrawLineEx((Vector2){cx - 5, cy - 3}, (Vector2){cx - 1, cy - 2}, 2, C_NEON_RED);
    DrawLineEx((Vector2){cx + 1, cy - 2}, (Vector2){cx + 5, cy - 3}, 2, C_NEON_RED);
    DrawRectangle((int)(cx - 4), (int)(cy + 3), 8, 5, (Color){60, 65, 80, 255});
    DrawLine((int)cx, (int)(cy + 3), (int)cx, (int)(cy + 12), (Color){170, 175, 190, 255});

    /* Sabre vermelho */
    Vector2 hilt = { cx - dy * 5.0f, cy + dx * 5.0f };
    Vector2 tip  = { cx + dx * 23.0f - dy * 10.0f, cy + dy * 23.0f + dx * 10.0f };
    DrawLineEx(hilt, tip, 7, (Color){255, 35, 55, 80});
    DrawLineEx(hilt, tip, 3, C_NEON_RED);
    DrawCircleV(hilt, 3, (Color){200,200,210,255});
}

static Color villain_color(int i, int vulnerable) {
    if (vulnerable) return (Color){35, 95, 255, 255};
    switch (i) {
        case 0: return (Color){20, 20, 26, 255};      /* Darth-like */
        case 1: return (Color){230, 235, 240, 255};   /* Trooper-like */
        case 2: return (Color){165, 35, 45, 255};     /* Guarda Sith */
        default: return (Color){80, 145, 95, 255};    /* Caçador */
    }
}

static void draw_vader(float cx, float cy, Color c, int vulnerable) {
    DrawCircleGradient((int)cx, (int)cy, 16, (Color){255,40,60,90}, (Color){0,0,0,0});
    DrawCircle((int)cx, (int)(cy - 2), 9, c);
    DrawRectangleRounded((Rectangle){cx - 10, cy - 1, 20, 16}, 0.35f, 8, c);
    DrawTriangle((Vector2){cx - 10, cy + 4}, (Vector2){cx - 16, cy + 15}, (Vector2){cx - 3, cy + 15}, c);
    DrawTriangle((Vector2){cx + 10, cy + 4}, (Vector2){cx + 16, cy + 15}, (Vector2){cx + 3, cy + 15}, c);
    DrawLineEx((Vector2){cx - 5, cy}, (Vector2){cx + 5, cy}, 2, vulnerable ? WHITE : C_NEON_RED);
    DrawLine((int)cx, (int)(cy+3), (int)cx, (int)(cy+11), (Color){180,185,195,255});
}

static void draw_trooper(float cx, float cy, Color c, int vulnerable) {
    DrawCircle((int)cx, (int)(cy - 1), 11, c);
    DrawRectangleRounded((Rectangle){cx - 10, cy + 3, 20, 13}, 0.28f, 8, c);
    DrawRectangle((int)(cx - 7), (int)(cy - 1), 5, 3, vulnerable ? WHITE : BLACK);
    DrawRectangle((int)(cx + 2), (int)(cy - 1), 5, 3, vulnerable ? WHITE : BLACK);
    DrawRectangle((int)(cx - 3), (int)(cy + 5), 6, 5, (Color){25,25,30,255});
}

static void draw_guard(float cx, float cy, Color c, int vulnerable) {
    DrawTriangle((Vector2){cx, cy - 14}, (Vector2){cx - 12, cy + 8}, (Vector2){cx + 12, cy + 8}, c);
    DrawRectangleRounded((Rectangle){cx - 10, cy + 5, 20, 12}, 0.35f, 8, c);
    DrawLineEx((Vector2){cx - 14, cy + 2}, (Vector2){cx + 14, cy + 2}, 3, vulnerable ? WHITE : C_NEON_RED);
}

static void draw_bounty(float cx, float cy, Color c, int vulnerable) {
    DrawRectangleRounded((Rectangle){cx - 11, cy - 13, 22, 26}, 0.28f, 8, c);
    DrawRectangle((int)(cx - 6), (int)(cy - 5), 12, 5, vulnerable ? WHITE : (Color){20,35,20,255});
    DrawRectangle((int)(cx + 7), (int)(cy - 11), 4, 8, (Color){160,190,170,255});
    DrawCircleGradient((int)cx, (int)(cy+2), 8, (Color){130,240,160,110}, (Color){0,0,0,0});
}

static void draw_villains(GameModel *m) {
    for (int i = 0; i < GHOSTS; i++) {
        Ghost *g = &m->ghosts[i];
        float cx = cell_x(g->e.x) + TILE_SIZE / 2.0f;
        float cy = cell_y(g->e.y) + TILE_SIZE / 2.0f;
        Color c = villain_color(i, g->vulnerable);
        if (g->vulnerable) DrawCircleGradient((int)cx, (int)cy, 18, (Color){70,140,255,150}, (Color){0,0,0,0});
        if (i == 0) draw_vader(cx, cy, c, g->vulnerable);
        else if (i == 1) draw_trooper(cx, cy, c, g->vulnerable);
        else if (i == 2) draw_guard(cx, cy, c, g->vulnerable);
        else draw_bounty(cx, cy, c, g->vulnerable);
    }
}

static void draw_hud(GameModel *m) {
    int x = LEFT_PAD + MAP_W * TILE_SIZE + 28;
    int y = TOP_BAR;
    DrawRectangleRounded((Rectangle){x, y, SIDE_W - 46, MAP_H * TILE_SIZE}, 0.06f, 12, C_PANEL);
    DrawRectangleLinesEx((Rectangle){x, y, SIDE_W - 46, MAP_H * TILE_SIZE}, 2, C_NEON_BLUE);

    DrawText("MISSÃO SITH", x + 22, y + 22, 27, C_GOLD);
    DrawText("Destrua os pontos de energia", x + 22, y + 58, 17, C_DIM);

    char txt[160];
    snprintf(txt, sizeof(txt), "Score: %d", m->score); DrawText(txt, x + 22, y + 104, 23, C_TEXT);
    snprintf(txt, sizeof(txt), "Vidas: %d", m->lives); DrawText(txt, x + 22, y + 135, 23, C_TEXT);
    snprintf(txt, sizeof(txt), "Cristais restantes: %d", m->pellets_left); DrawText(txt, x + 22, y + 166, 20, C_TEXT);

    DrawText("Power da Força", x + 22, y + 218, 20, C_FORCE);
    float p = (float)m->power_timer / (float)POWER_TIME;
    if (p < 0) p = 0; if (p > 1) p = 1;
    DrawRectangle(x + 22, y + 248, SIDE_W - 92, 16, (Color){32, 40, 62, 255});
    DrawRectangle(x + 22, y + 248, (int)((SIDE_W - 92) * p), 16, C_FORCE);
    DrawRectangleLines(x + 22, y + 248, SIDE_W - 92, 16, C_NEON_BLUE);

    DrawText("Vilões", x + 22, y + 306, 23, C_NEON_RED);
    DrawText("Darth Mask  - Dijkstra", x + 22, y + 342, 17, C_DIM);
    DrawText("Stormtrooper - BFS", x + 22, y + 369, 17, C_DIM);
    DrawText("Guarda Sith  - DFS", x + 22, y + 396, 17, C_DIM);
    DrawText("Caçador      - IA mista", x + 22, y + 423, 17, C_DIM);

    DrawText("Poderes extras", x + 22, y + 475, 22, C_FORCE);
    snprintf(txt, sizeof(txt), "Pulso da Forca: %s", g_force_blast_cd <= 0 ? "PRONTO" : "recarregando");
    DrawText(txt, x + 22, y + 508, 17, g_force_blast_cd <= 0 ? C_FORCE : C_DIM);
    snprintf(txt, sizeof(txt), "Campo lento: %d", g_force_slow_timer / 60);
    DrawText(txt, x + 22, y + 535, 17, C_DIM);
    snprintf(txt, sizeof(txt), "Escudo: %d  Dash: %d", g_shield_timer / 60, g_dash_timer / 60);
    DrawText(txt, x + 22, y + 562, 17, C_DIM);
    snprintf(txt, sizeof(txt), "Freeze: %d  Multiplicador: %d", g_freeze_timer / 60, g_multiplier_timer / 60);
    DrawText(txt, x + 22, y + 589, 17, C_DIM);

    if (g_ghost_release_timer > 0) {
        snprintf(txt, sizeof(txt), "Reforcos presos: %ds", g_ghost_release_timer / 60);
        DrawText(txt, x + 22, y + 616, 17, C_NEON_RED);
    } else {
        DrawText("Reforcos liberados!", x + 22, y + 616, 17, C_NEON_RED);
    }

    DrawText("Controles", x + 22, y + 632, 23, C_NEON_BLUE);
    DrawText("WASD/Setas | E poder | P pausa", x + 22, y + 667, 16, C_TEXT);
    DrawText("R reinicia | ESC menu", x + 22, y + 691, 16, C_TEXT);
}

static void draw_game(GameModel *m) {
    draw_space_background();
    DrawText("PAC-MAN GALACTIC", LEFT_PAD, 22, 36, C_GOLD);
    DrawText("Tema espacial | Darth Pac x vilões galácticos | mapa Death Star", LEFT_PAD + 390, 35, 18, C_DIM);
    draw_map(m);
    draw_bonus_item();
    draw_darth_pac(m);
    draw_power_auras(m);
    draw_villains(m);
    draw_particles();
    draw_hud(m);

    if (m->ready_timer > 0) draw_center_text("O LADO SOMBRIO DESPERTOU!", SCREEN_H / 2 - 24, 36, C_FORCE);
    if (m->state == STATE_PAUSED) draw_center_text("PAUSADO", SCREEN_H / 2 - 20, 46, C_TEXT);
    if (m->state == STATE_GAMEOVER) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){0,0,0,165});
        draw_center_text("GAME OVER", SCREEN_H/2 - 52, 58, C_NEON_RED);
        draw_center_text("R para reiniciar | ESC para menu", SCREEN_H/2 + 22, 23, C_TEXT);
    }
    if (m->state == STATE_WIN) {
        DrawRectangle(0, 0, SCREEN_W, SCREEN_H, (Color){0,0,0,150});
        draw_center_text("MISSÃO CONCLUÍDA!", SCREEN_H/2 - 52, 52, C_FORCE);
        draw_center_text("R para jogar novamente | ESC para menu", SCREEN_H/2 + 20, 23, C_TEXT);
    }
}

static void handle_game_input(GameModel *m) {
    if (IsKeyPressed(KEY_ESCAPE)) { m->state = STATE_MENU; return; }
    if (IsKeyPressed(KEY_R)) { start_game(m); return; }
    if (IsKeyPressed(KEY_P)) {
        if (m->state == STATE_PLAYING) m->state = STATE_PAUSED;
        else if (m->state == STATE_PAUSED) m->state = STATE_PLAYING;
    }
    if (m->state != STATE_PLAYING) return;
    if (IsKeyPressed(KEY_E) && g_force_blast_cd <= 0) {
        m->power_timer = POWER_TIME + 80;
        m->ghost_eat_combo = 0;
        g_force_blast_cd = 520;
        g_force_slow_timer = 300;
        for (int i = 0; i < GHOSTS; i++) {
            m->ghosts[i].vulnerable = 1;
            m->ghosts[i].in_house = 0;
            m->ghosts[i].release_timer = 0;
        }
    }
    if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))    { m->desired_dx = 0;  m->desired_dy = -1; }
    if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))  { m->desired_dx = 0;  m->desired_dy = 1;  }
    if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A))  { m->desired_dx = -1; m->desired_dy = 0;  }
    if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) { m->desired_dx = 1;  m->desired_dy = 0;  }
}

static void update_game(GameModel *m) {
    if (m->state != STATE_PLAYING) return;
    if (m->ready_timer > 0) { m->ready_timer--; return; }

    if (g_force_blast_cd > 0) g_force_blast_cd--;
    if (g_force_slow_timer > 0) g_force_slow_timer--;
    if (g_shield_timer > 0) g_shield_timer--;
    if (g_dash_timer > 0) g_dash_timer--;
    if (g_freeze_timer > 0) g_freeze_timer--;
    if (g_multiplier_timer > 0) g_multiplier_timer--;
    if (g_ghost_release_timer > 0) g_ghost_release_timer--;

    if (g_bonus.active) {
        g_bonus.timer--;
        if (g_bonus.timer <= 0) g_bonus.active = 0;
    } else {
        g_bonus_spawn_timer++;
        if (g_bonus_spawn_timer > 360) {
            g_bonus_spawn_timer = 0;
            spawn_bonus_item(m);
        }
    }

    /* A cada tempo nasce um novo cristal de poder, deixando a partida mais dinâmica */
    g_force_spawn_timer++;
    if (g_force_spawn_timer > 420) {
        g_force_spawn_timer = 0;
        for (int tries = 0; tries < 80; tries++) {
            int rx = GetRandomValue(1, MAP_W - 2);
            int ry = GetRandomValue(1, MAP_H - 2);
            if (m->grid[ry][rx] == TILE_EMPTY) {
                m->grid[ry][rx] = TILE_POWER;
                m->pellets_left++;
                m->total_pellets++;
                break;
            }
        }
    }

    int score_before = m->score;
    model_move_pacman(m);
    if (m->score > score_before) {
        burst_particles(cell_x(m->pacman.x) + TILE_SIZE/2, cell_y(m->pacman.y) + TILE_SIZE/2, C_PELLET, 6);
        if (g_multiplier_timer > 0) m->score += (m->score - score_before);
    }
    apply_bonus(m);
    repel_with_shield(m);
    int col = model_check_collisions(m);
    if (col < 0) return;
    if (g_freeze_timer <= 0 && (g_force_slow_timer <= 0 || (m->frame_count % 2 == 0))) {
        model_move_ghosts(m);

        /* Mantem dois viloes presos na base durante os 2 primeiros minutos.
           Depois do tempo acabar, eles passam a andar normalmente pelo mapa. */
        if (g_ghost_release_timer > 0) {
            m->ghosts[2].e.x = 12;
            m->ghosts[2].e.y = 15;
            m->ghosts[2].e.dx = 0;
            m->ghosts[2].e.dy = 0;

            m->ghosts[3].e.x = 15;
            m->ghosts[3].e.y = 15;
            m->ghosts[3].e.dx = 0;
            m->ghosts[3].e.dy = 0;
        }
    }
    repel_with_shield(m);
    col = model_check_collisions(m);
    if (col < 0) return;

    if (m->power_timer > 0) {
        m->power_timer--;
        if (m->power_timer == 0) {
            for (int i = 0; i < GHOSTS; i++) m->ghosts[i].vulnerable = 0;
            m->ghost_eat_combo = 0;
        }
    }
    if (m->pellets_left <= 0) {
        m->state = STATE_WIN;
        m->score_tree = bst_insert(m->score_tree, m->score, "SITH");
    }
}

int main(void) {
    srand((unsigned)time(NULL));
    GameModel model;
    model_init(&model);

    InitWindow(SCREEN_W, SCREEN_H, "Pac-Man Galactic - Raylib");
    SetTargetFPS(60);
    init_stars();

    g_splash = LoadTexture("assets/tela_inicio.png");
    g_has_splash = (g_splash.id > 0);

    double accumulator = 0.0;
    /* Movimento mais devagar, com Dash temporário como poder extra */
    const double normal_step = 0.145;
    const double dash_step = 0.085;

    while (!WindowShouldClose()) {
        if (model.state == STATE_MENU) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) start_game(&model);
            if (IsKeyPressed(KEY_ESCAPE)) break;
        } else {
            handle_game_input(&model);
            update_particles();
            accumulator += GetFrameTime();
            double step = (g_dash_timer > 0) ? dash_step : normal_step;
            while (accumulator >= step) {
                model.frame_count++;
                update_game(&model);
                if (model.state == STATE_DYING) {
                    burst_particles(cell_x(model.pacman.x) + TILE_SIZE/2, cell_y(model.pacman.y) + TILE_SIZE/2, C_NEON_RED, 55);
                    model_reset_positions(&model);
                    model.state = STATE_PLAYING;
                }
                accumulator -= step;
            }
        }
        BeginDrawing();
        if (model.state == STATE_MENU) draw_menu(&model);
        else {
            Vector2 shake = {0};
            if (g_screen_shake > 0.0f) {
                shake.x = (float)GetRandomValue(-10, 10) * g_screen_shake * 0.12f;
                shake.y = (float)GetRandomValue(-10, 10) * g_screen_shake * 0.12f;
            }
            BeginMode2D((Camera2D){ .offset = shake, .target = (Vector2){0,0}, .rotation = 0.0f, .zoom = 1.0f });
            draw_game(&model);
            EndMode2D();
        }
        EndDrawing();
    }

    if (g_has_splash) UnloadTexture(g_splash);
    model_free(&model);
    CloseWindow();
    return 0;
}
