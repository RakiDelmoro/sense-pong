#ifndef PONG_H
#define PONG_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Show the Pong welcome screen.
 *
 * This is the entry point called once at boot. It builds the welcome UI
 * (title + START button). Pressing START builds the game screen and starts
 * the ball. During a game a PAUSE button opens a menu with RESUME and
 * MAIN MENU options. The game runs forever until paused / quit.
 */
void pong_start(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* PONG_H */
