/* Auto-generated from AprilRobotics/apriltag tag36h11 (IDs 0-3).
 * 8x8 cell grids: 1 = white, 0 = black. Outer ring is the black
 * border (width_at_border=8, 1-cell border, reversed_border=false).
 * Payload bits placed per tag36h11.c bit_x/bit_y, MSB-first rcode
 * matching the detector's quick_decode order. */
#ifndef APRILTAGS_H
#define APRILTAGS_H
#include <stdint.h>

#define APRILTAG_GRID 8       /* cells per side (border + 6x6 payload) */
#define APRILTAG_COUNT 4      /* IDs 0..3, one per screen corner */

/* apriltag_cell[id][y][x]; 1=white, 0=black */
static const uint8_t apriltag_cell[APRILTAG_COUNT][APRILTAG_GRID][APRILTAG_GRID] = {
    {
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1, 1, 0, 1, 0, 1, 0 },
        { 0, 0, 1, 1, 1, 0, 1, 0 },
        { 0, 0, 1, 1, 0, 0, 0, 0 },
        { 0, 1, 0, 1, 0, 0, 0, 0 },
        { 0, 0, 1, 0, 1, 1, 0, 0 },
        { 0, 0, 0, 0, 1, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
    },
    {
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1, 1, 0, 1, 1, 0, 0 },
        { 0, 0, 1, 0, 1, 1, 1, 0 },
        { 0, 1, 1, 1, 1, 0, 0, 0 },
        { 0, 0, 1, 1, 0, 0, 0, 0 },
        { 0, 1, 0, 1, 1, 0, 1, 0 },
        { 0, 0, 0, 1, 0, 0, 1, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
    },
    {
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1, 1, 0, 1, 1, 1, 0 },
        { 0, 0, 1, 0, 0, 1, 0, 0 },
        { 0, 1, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 1, 0, 0, 1, 0 },
        { 0, 0, 0, 0, 1, 0, 0, 0 },
        { 0, 0, 0, 1, 1, 1, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
    },
    {
        { 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 1, 1, 1, 0, 0, 1, 0 },
        { 0, 0, 0, 0, 1, 1, 1, 0 },
        { 0, 1, 0, 0, 1, 1, 1, 0 },
        { 0, 1, 0, 1, 0, 0, 1, 0 },
        { 0, 1, 1, 0, 0, 1, 0, 0 },
        { 0, 0, 1, 1, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0 },
    },
};

#endif /* APRILTAGS_H */
