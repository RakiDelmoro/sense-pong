/**
 * sense-pong - Pong UI for the SenseCAP Indicator (ESP32-S3, 480x480 round LCD)
 *
 * Flow:
 *   boot -> welcome screen (title + START)
 *        -> START pressed -> game screen (stationary paddles + auto ball)
 *                        -> PAUSE button -> pause overlay (RESUME / MAIN MENU)
 *                          RESUME   -> back to game
 *                          MAIN MENU-> back to welcome
 *
 * The game runs forever. Paddles are stationary (no player control yet).
 */

#include "pong.h"
#include <stdlib.h>
#include "joystick.h"
#include "rp2040_boot.h"

#define DISP_W 480
#define DISP_H 480

/* Playfield geometry */
#define WALL_TOP        0
#define WALL_BOTTOM     (DISP_H - 1)
#define PADDLE_W        12
#define PADDLE_H        80     /* ~1/6 of 480px height = balanced Pong paddle */
#define PADDLE_MARGIN   24
#define BALL_SIZE       12

/* Left paddle: stationary, vertically centered */
#define LP_X            PADDLE_MARGIN
#define LP_Y            ((DISP_H - PADDLE_H) / 2)

/* Right paddle: stationary, vertically centered */
#define RP_X            (DISP_W - PADDLE_MARGIN - PADDLE_W)
#define RP_Y            ((DISP_H - PADDLE_H) / 2)

#define NET_X           ((DISP_W - 4) / 2)   /* center dashed line */
#define NET_SEG_H       16
#define NET_GAP         12

#define BALL_SPEED      4                     /* pixels per tick */
#define CPU_SPEED         3                     /* right paddle chase speed (fallback) */
#define TICK_PERIOD_MS  16                    /* ~60 fps — calmer gameplay pace */

/* Right paddle speed when driven by the joystick (pixels per tick at full stick). */
#define JOYSTICK_SPEED   6

/* Paddle can move between these Y bounds (keeps full paddle on-screen) */
#define PADDLE_Y_MIN    0
#define PADDLE_Y_MAX    (DISP_H - PADDLE_H)

/* Fonts */
#define FONT_TITLE      (&lv_font_montserrat_48)
#define FONT_SCORE      (&lv_font_montserrat_28)
#define FONT_MENU       (&lv_font_montserrat_36)
#define FONT_SUB        (&lv_font_montserrat_16)

typedef enum {
    STATE_WELCOME,
    STATE_PLAYING,
    STATE_PAUSED,
} pong_phase_t;

typedef struct {
    /* screens / overlays */
    lv_obj_t *welcome_screen;
    lv_obj_t *game_screen;
    lv_obj_t *pause_overlay;

    /* game objects (children of game_screen) */
    lv_obj_t *left_paddle;
    lv_obj_t *right_paddle;
    lv_obj_t *ball;
    lv_obj_t *score_label;

    /* ball motion */
    int16_t ball_x;
    int16_t ball_y;
    int16_t ball_vx;
    int16_t ball_vy;

    uint8_t score_left;
    uint8_t score_right;

    int16_t right_paddle_y;        /* CPU-controlled right paddle top Y */
    int16_t left_paddle_y;         /* player-controlled left paddle top Y */

    pong_phase_t state;
    lv_timer_t *tick_timer;
} pong_state_t;

static pong_state_t g;

/* ---- forward decls ---- */
static void build_welcome(void);
static void build_game(void);
static void show_pause_overlay(void);
static void hide_pause_overlay(void);
static void goto_welcome(void);
static void goto_game(void);

/* ---- small UI helpers ---- */

static lv_obj_t *make_rect(lv_obj_t *parent, lv_coord_t w, lv_coord_t h,
                           lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_size(r, w, h);
    lv_obj_set_pos(r, x, y);
    lv_obj_set_style_bg_color(r, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(r, LV_OPA_COVER, 0);
    lv_obj_clear_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    return r;
}

static lv_obj_t *make_button(lv_obj_t *parent, lv_coord_t w, lv_coord_t h,
                             lv_coord_t x, lv_coord_t y,
                             const char *text, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_style_radius(btn, 8, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, lv_color_black(), 0);
    lv_obj_center(lbl);

    if (cb) lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    return btn;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, lv_color_t color)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_color(lbl, color, 0);
    if (font) lv_obj_set_style_text_font(lbl, font, 0);
    return lbl;
}

static void build_center_net(lv_obj_t *parent)
{
    lv_coord_t y = 0;
    while (y < DISP_H) {
        make_rect(parent, 4, NET_SEG_H, NET_X, y);
        y += NET_SEG_H + NET_GAP;
    }
}

static void black_screen(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_style_border_width(scr, 0, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

/* ---- ball / score ---- */

static void reset_ball(int8_t direction)
{
    g.ball_x = (DISP_W - BALL_SIZE) / 2;
    g.ball_y = (DISP_H - BALL_SIZE) / 2;

    int8_t dy = (rand() % 5) - 2;            /* -2..2 */
    if (dy == 0) dy = 1;
    g.ball_vx = (direction >= 0 ? 1 : -1) * BALL_SPEED;
    g.ball_vy = dy;
}

static void update_score_label(void)
{
    lv_label_set_text_fmt(g.score_label, "%d   %d", g.score_left, g.score_right);
}

/* ---- game tick ---- */

static void pong_tick(lv_timer_t *timer)
{
    (void)timer;
    if (g.state != STATE_PLAYING) return;

    g.ball_x += g.ball_vx;
    g.ball_y += g.ball_vy;

    /* Left paddle: velocity control with deadzone + low-pass smoothing.
     * Stick deflection sets paddle SPEED, not position, so when you let go
     * (stick near center) the paddle stays put instead of snapping back to
     * the stick's resting value. The deadzone kills analog jitter and the
     * smoothing filters out the per-sample noise that made it jump. */
    if (joystick_is_connected()) {
        float jy = joystick_get_y();              /* -1.0 .. +1.0 */

        /* Low-pass filter: blend new sample with previous to smooth noise. */
        static float jy_filt = 0.0f;
        jy_filt = jy_filt + 0.5f * (jy - jy_filt);

        /* Deadzone: ignore tiny center values so the paddle holds still. */
        const float dz = 0.12f;
        float vel = 0.0f;
        if (jy_filt >  dz) vel = (jy_filt - dz) / (1.0f - dz);
        else if (jy_filt < -dz) vel = (jy_filt + dz) / (1.0f - dz);

        /* vel: -1 (up) .. +1 (down). Apply as per-tick speed. */
        g.left_paddle_y += (int16_t)(vel * JOYSTICK_SPEED);
        if (g.left_paddle_y < PADDLE_Y_MIN) g.left_paddle_y = PADDLE_Y_MIN;
        if (g.left_paddle_y > PADDLE_Y_MAX) g.left_paddle_y = PADDLE_Y_MAX;
    }
    lv_obj_set_pos(g.left_paddle, LP_X, g.left_paddle_y);

    /* Right paddle: CPU-chase the ball's vertical center */
    {
        int16_t paddle_center = g.right_paddle_y + PADDLE_H / 2;
        int16_t ball_center   = g.ball_y + BALL_SIZE / 2;
        if (ball_center < paddle_center - 1) {
            g.right_paddle_y -= CPU_SPEED;
        } else if (ball_center > paddle_center + 1) {
            g.right_paddle_y += CPU_SPEED;
        }
    }
    if (g.right_paddle_y < PADDLE_Y_MIN) g.right_paddle_y = PADDLE_Y_MIN;
    if (g.right_paddle_y > PADDLE_Y_MAX) g.right_paddle_y = PADDLE_Y_MAX;
    lv_obj_set_pos(g.right_paddle, RP_X, g.right_paddle_y);

    /* Top / bottom walls */
    if (g.ball_y <= WALL_TOP) {
        g.ball_y = WALL_TOP;
        g.ball_vy = -g.ball_vy;
    } else if (g.ball_y + BALL_SIZE >= WALL_BOTTOM) {
        g.ball_y = WALL_BOTTOM - BALL_SIZE;
        g.ball_vy = -g.ball_vy;
    }

    /* Left paddle (player): bounce if ball overlaps paddle */
    if (g.ball_vx < 0 &&
        g.ball_x <= LP_X + PADDLE_W &&
        g.ball_x + BALL_SIZE >= LP_X &&
        g.ball_y + BALL_SIZE >= g.left_paddle_y &&
        g.ball_y <= g.left_paddle_y + PADDLE_H) {
        g.ball_x = LP_X + PADDLE_W;
        g.ball_vx = -g.ball_vx;
    }

    /* Right paddle (CPU): bounce if ball overlaps paddle */
    if (g.ball_vx > 0 &&
        g.ball_x + BALL_SIZE >= RP_X &&
        g.ball_x <= RP_X + PADDLE_W &&
        g.ball_y + BALL_SIZE >= g.right_paddle_y &&
        g.ball_y <= g.right_paddle_y + PADDLE_H) {
        g.ball_x = RP_X - BALL_SIZE;
        g.ball_vx = -g.ball_vx;
    }

    /* Scoring: ball escaped a side */
    if (g.ball_x + BALL_SIZE < 0) {
        g.score_right++;
        update_score_label();
        reset_ball(+1);
    } else if (g.ball_x > DISP_W) {
        g.score_left++;
        update_score_label();
        reset_ball(-1);
    }

    lv_obj_set_pos(g.ball, g.ball_x, g.ball_y);
}

/* ---- button callbacks ---- */

static void start_btn_cb(lv_event_t *e)
{
    (void)e;
    goto_game();
}

static void flash_rp2040_btn_cb(lv_event_t *e)
{
    (void)e;
    /* Force the RP2040 into its UF2 bootloader. The ESP32 keeps running.
     * A 'RPI-RP2' drive should appear on the host PC shortly. */
    rp2040_force_bootloader();
}

static void pause_btn_cb(lv_event_t *e)
{
    (void)e;
    if (g.state != STATE_PLAYING) return;
    g.state = STATE_PAUSED;
    lv_timer_pause(g.tick_timer);
    show_pause_overlay();
}

static void resume_btn_cb(lv_event_t *e)
{
    (void)e;
    if (g.state != STATE_PAUSED) return;
    hide_pause_overlay();
    g.state = STATE_PLAYING;
    lv_timer_resume(g.tick_timer);
}

static void mainmenu_btn_cb(lv_event_t *e)
{
    (void)e;
    goto_welcome();
}

/* ---- screen builders ---- */

static void build_welcome(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    black_screen(scr);

    /* Title */
    lv_obj_t *title = make_label(scr, "PONG", FONT_TITLE, lv_color_white());
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 120);

    /* Subtitle */
    lv_obj_t *sub = make_label(scr, "sense-pong", FONT_SUB,
                               lv_color_make(170, 170, 170));
    lv_obj_align(sub, LV_ALIGN_TOP_MID, 0, 188);

    /* START button */
    make_button(scr, 180, 64, (DISP_W - 180) / 2, 270, "START", start_btn_cb);

    g.welcome_screen = scr;
    lv_scr_load(scr);
    g.state = STATE_WELCOME;
}

static void build_game(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    black_screen(scr);

    /* Center dashed net */
    build_center_net(scr);

    /* Left paddle (player via joystick), starts centered */
    g.left_paddle_y = LP_Y;
    g.left_paddle  = make_rect(scr, PADDLE_W, PADDLE_H, LP_X, LP_Y);
    /* Right paddle (CPU), starts centered */
    g.right_paddle_y = RP_Y;
    g.right_paddle = make_rect(scr, PADDLE_W, PADDLE_H, RP_X, RP_Y);

    /* PAUSE button (top center, clear of the round-screen clipping) */
    make_button(scr, 100, 32, (DISP_W - 100) / 2, 8, "PAUSE", pause_btn_cb);

    /* Score */
    g.score_label = make_label(scr, "0   0", FONT_SCORE, lv_color_white());
    update_score_label();
    lv_obj_align(g.score_label, LV_ALIGN_TOP_MID, 0, 48);

    /* Ball */
    g.ball = make_rect(scr, BALL_SIZE, BALL_SIZE, 0, 0);

    g.score_left = 0;
    g.score_right = 0;
    reset_ball((rand() % 2) ? +1 : -1);
    lv_obj_set_pos(g.ball, g.ball_x, g.ball_y);

    g.game_screen = scr;
    g.pause_overlay = NULL;
    lv_scr_load(scr);
}

static void show_pause_overlay(void)
{
    if (g.pause_overlay) return;

    lv_obj_t *ov = lv_obj_create(g.game_screen);
    lv_obj_remove_style_all(ov);
    lv_obj_set_size(ov, DISP_W, DISP_H);
    lv_obj_set_pos(ov, 0, 0);
    lv_obj_set_style_bg_color(ov, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(ov, LV_OPA_80, 0);
    lv_obj_clear_flag(ov, LV_OBJ_FLAG_SCROLLABLE);
    /* bring to top */
    lv_obj_move_foreground(ov);

    lv_obj_t *paused_lbl = make_label(ov, "PAUSED", FONT_MENU, lv_color_white());
    lv_obj_align(paused_lbl, LV_ALIGN_TOP_MID, 0, 150);

    make_button(ov, 170, 56, (DISP_W - 170) / 2, 220, "RESUME", resume_btn_cb);
    make_button(ov, 170, 56, (DISP_W - 170) / 2, 292, "MAIN MENU", mainmenu_btn_cb);

    g.pause_overlay = ov;
}

static void hide_pause_overlay(void)
{
    if (!g.pause_overlay) return;
    lv_obj_del(g.pause_overlay);
    g.pause_overlay = NULL;
}

/* ---- state transitions ---- */

static void goto_game(void)
{
    build_game();
    g.state = STATE_PLAYING;
    if (!g.tick_timer) {
        g.tick_timer = lv_timer_create(pong_tick, TICK_PERIOD_MS, NULL);
    } else {
        lv_timer_reset(g.tick_timer);
        lv_timer_resume(g.tick_timer);
    }
}

static void goto_welcome(void)
{
    /* stop the ball */
    if (g.tick_timer) {
        lv_timer_pause(g.tick_timer);
    }
    g.state = STATE_WELCOME;

    if (g.game_screen) {
        lv_obj_del(g.game_screen);
        g.game_screen = NULL;
        g.pause_overlay = NULL;
        g.ball = NULL;
        g.left_paddle = g.right_paddle = NULL;
        g.score_label = NULL;
    }

    /* (re)build welcome if it was deleted, else just show it */
    if (!g.welcome_screen) {
        build_welcome();
    } else {
        lv_scr_load(g.welcome_screen);
    }
}

/* ---- public API ---- */

void pong_start(void)
{
    g.tick_timer = NULL;
    g.game_screen = NULL;
    g.pause_overlay = NULL;
    g.welcome_screen = NULL;
    build_welcome();
}
