#pragma once

// Linux types special characters via Ctrl+Shift+U hex input;
// macOS and Windows are handled in keymap.c without host software.
#define UNICODE_SELECTED_MODES UNICODE_MODE_LINUX

// MAC_BASE, MAC_FN, WIN_BASE, WIN_FN, UMLAUT
#define DYNAMIC_KEYMAP_LAYER_COUNT 5

// Restart the keyboard when USB re-enumerates (e.g. dual boot reboot), so the
// OS detection starts fresh instead of mixing data from the previous OS
#define OS_DETECTION_KEYBOARD_RESET

// Lighting defaults, restored with Fn + top right key: dark until typing,
// red to purple ripples (values read from the keyboard via VIA)
#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_SOLID_REACTIVE_MULTINEXUS
#define RGB_MATRIX_DEFAULT_HUE 0
#define RGB_MATRIX_DEFAULT_SAT 255
#define RGB_MATRIX_DEFAULT_VAL RGB_MATRIX_MAXIMUM_BRIGHTNESS
#define RGB_MATRIX_DEFAULT_SPD 127
