/* Copyright 2024 ~ 2025 @ Keychron (https://www.keychron.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H
#include "keychron_common.h"
#ifdef LK_WIRELESS_ENABLE
#    include "transport.h"
#endif

enum layers {
    MAC_BASE,
    MAC_FN,
    WIN_BASE,
    WIN_FN,
    UMLAUT,
};

#define FN_MAC MO(MAC_FN)
#define FN_WIN MO(WIN_FN)

enum custom_keycodes {
    UC_ADOT = SAFE_RANGE,
    UC_ODOT,
    UC_UDOT,
    UC_ESZETT,
    UC_EURO,
    UC_DEGREE,
    UC_SUP1,
    UC_SUP2,
    UC_SUP3,
    UC_SUP4,
    UC_SUP5,
    UC_SUP6,
    UC_SUP7,
    UC_SUP8,
    UC_SUP9,
    UC_SUP0,
    HOST_AUTO,
    HOST_LNX,
    HOST_MAC,
    HOST_WIN,
    LIGHT_DEFAULT
};

// Host OS used to decide how special characters are typed.
// HOST_MODE_AUTO uses OS detection, which only works over USB.
enum host_mode {
    HOST_MODE_AUTO,
    HOST_MODE_LINUX,
    HOST_MODE_MACOS,
    HOST_MODE_WINDOWS
};

// One host mode per connection, stored in the 32 bit user EEPROM config.
enum host_connection {
    CONN_USB,
    CONN_BLUETOOTH,
    CONN_2G4,
    CONN_COUNT
};

static const uint8_t default_host_modes[CONN_COUNT] = {
    [CONN_USB]       = HOST_MODE_AUTO,
    [CONN_BLUETOOTH] = HOST_MODE_MACOS,
    [CONN_2G4]       = HOST_MODE_LINUX,
};

typedef union {
    uint32_t raw;
    uint8_t  mode[CONN_COUNT];
} host_config_t;

static host_config_t host_config;

// How to produce one character on each OS (host layout: US / ABC).
// Linux:   Ctrl+Shift+U hex input (IBus / fcitx5)
// macOS:   optional dead key followed by a key, built into the US layout
// Windows: Alt + numpad decimal code (cp1252), built into Windows
// A value of 0 / KC_NO means the character is not available on that OS.
typedef struct {
    const char *unicode;
    const char *unicode_shifted;
    uint16_t    mac_dead;
    uint16_t    mac;
    uint16_t    mac_shifted;
    uint8_t     win;
    uint8_t     win_shifted;
} special_char_t;

static const special_char_t special_chars[] = {
    [UC_ADOT   - UC_ADOT] = {"ä", "Ä", A(KC_U), KC_A,        S(KC_A),     228, 196},
    [UC_ODOT   - UC_ADOT] = {"ö", "Ö", A(KC_U), KC_O,        S(KC_O),     246, 214},
    [UC_UDOT   - UC_ADOT] = {"ü", "Ü", A(KC_U), KC_U,        S(KC_U),     252, 220},
    [UC_ESZETT - UC_ADOT] = {"ß", "ẞ", KC_NO,   A(KC_S),     KC_NO,       223, 0},
    [UC_EURO   - UC_ADOT] = {"€", "€", KC_NO,   S(A(KC_2)),  S(A(KC_2)),  128, 128},
    [UC_DEGREE - UC_ADOT] = {"°", "℃", KC_NO,   S(A(KC_8)),  KC_NO,       176, 0},
    [UC_SUP1   - UC_ADOT] = {"¹", "¹", KC_NO,   KC_NO,       KC_NO,       185, 185},
    [UC_SUP2   - UC_ADOT] = {"²", "²", KC_NO,   KC_NO,       KC_NO,       178, 178},
    [UC_SUP3   - UC_ADOT] = {"³", "³", KC_NO,   KC_NO,       KC_NO,       179, 179},
    [UC_SUP4   - UC_ADOT] = {"⁴", "⁴", KC_NO,   KC_NO,       KC_NO,       0,   0},
    [UC_SUP5   - UC_ADOT] = {"⁵", "⁵", KC_NO,   KC_NO,       KC_NO,       0,   0},
    [UC_SUP6   - UC_ADOT] = {"⁶", "⁶", KC_NO,   KC_NO,       KC_NO,       0,   0},
    [UC_SUP7   - UC_ADOT] = {"⁷", "⁷", KC_NO,   KC_NO,       KC_NO,       0,   0},
    [UC_SUP8   - UC_ADOT] = {"⁸", "⁸", KC_NO,   KC_NO,       KC_NO,       0,   0},
    [UC_SUP9   - UC_ADOT] = {"⁹", "⁹", KC_NO,   KC_NO,       KC_NO,       0,   0},
    [UC_SUP0   - UC_ADOT] = {"⁰", "⁰", KC_NO,   KC_NO,       KC_NO,       0,   0},
};

void eeconfig_init_user(void) {
    host_config.raw = 0;
    memcpy(host_config.mode, default_host_modes, sizeof(default_host_modes));
    eeconfig_update_user(host_config.raw);
}

void keyboard_post_init_user(void) {
    host_config.raw = eeconfig_read_user();
    for (uint8_t i = 0; i < CONN_COUNT; i++) {
        if (host_config.mode[i] > HOST_MODE_WINDOWS) {
            host_config.mode[i] = default_host_modes[i];
        }
    }
}

static uint8_t current_connection(void) {
#ifdef LK_WIRELESS_ENABLE
    switch (get_transport()) {
        case TRANSPORT_BLUETOOTH:
            return CONN_BLUETOOTH;
        case TRANSPORT_P2P4:
            return CONN_2G4;
        default:
            break;
    }
#endif
    return CONN_USB;
}

static uint8_t current_host(void) {
    uint8_t mode = host_config.mode[current_connection()];
    if (mode != HOST_MODE_AUTO) {
        return mode;
    }
    switch (detected_host_os()) {
        case OS_MACOS:
        case OS_IOS:
            return HOST_MODE_MACOS;
        case OS_WINDOWS:
            return HOST_MODE_WINDOWS;
        default:
            return HOST_MODE_LINUX;
    }
}

// Start OS detection from scratch when the host resets an already configured
// keyboard (dual boot reboot, UEFI handing over to the OS). Otherwise the USB
// setup data of the previous OS is mixed into the guess. Done without
// OS_DETECTION_KEYBOARD_RESET, whose keyboard restart stalled enumeration.
void notify_usb_device_state_change_user(struct usb_device_state usb_device_state) {
    static usb_configure_state_t last_state = USB_DEVICE_STATE_NO_INIT;

    if (usb_device_state.configure_state == USB_DEVICE_STATE_INIT && last_state >= USB_DEVICE_STATE_CONFIGURED) {
        erase_wlength_data();
    }
    last_state = usb_device_state.configure_state;
}

static uint8_t numpad_digit(uint8_t digit) {
    return digit == 0 ? KC_KP_0 : KC_KP_1 + digit - 1;
}

// Alt + numpad code, e.g. Alt+0228 for ä. Needs Num Lock on.
static void send_alt_code(uint8_t code) {
    bool num_lock = host_keyboard_led_state().num_lock;
    if (!num_lock) {
        tap_code(KC_NUM);
    }
    register_code(KC_LALT);
    tap_code(KC_KP_0);
    tap_code(numpad_digit(code / 100));
    tap_code(numpad_digit(code / 10 % 10));
    tap_code(numpad_digit(code % 10));
    unregister_code(KC_LALT);
    if (!num_lock) {
        tap_code(KC_NUM);
    }
}

static void send_special_char(const special_char_t *c, bool shifted) {
    switch (current_host()) {
        case HOST_MODE_MACOS: {
            uint16_t key = shifted ? c->mac_shifted : c->mac;
            if (key != KC_NO) {
                if (c->mac_dead != KC_NO) {
                    tap_code16(c->mac_dead);
                }
                tap_code16(key);
            }
            break;
        }
        case HOST_MODE_WINDOWS: {
            uint8_t code = shifted ? c->win_shifted : c->win;
            if (code) {
                send_alt_code(code);
            }
            break;
        }
        default:
            send_unicode_string(shifted ? c->unicode_shifted : c->unicode);
            break;
    }
}

// Sets the host mode for the currently active connection
static void set_host_mode(uint8_t mode) {
    host_config.mode[current_connection()] = mode;
    eeconfig_update_user(host_config.raw);
}

// Handle custom keycodes
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) {
        return true;  // Only process on key press
    }

    switch (keycode) {
        case UC_ADOT ... UC_SUP0: {
            uint8_t mods      = get_mods();
            uint8_t weak_mods = get_weak_mods();
            bool    shifted   = (mods & MOD_MASK_SHIFT) || host_keyboard_led_state().caps_lock;

            clear_mods();
            clear_weak_mods();
            send_keyboard_report();
            send_special_char(&special_chars[keycode - UC_ADOT], shifted);
            set_mods(mods);
            set_weak_mods(weak_mods);
            send_keyboard_report();
            return false;
        }

        case HOST_AUTO:
            set_host_mode(HOST_MODE_AUTO);
            return false;
        case HOST_LNX:
            set_host_mode(HOST_MODE_LINUX);
            return false;
        case HOST_MAC:
            set_host_mode(HOST_MODE_MACOS);
            return false;
        case HOST_WIN:
            set_host_mode(HOST_MODE_WINDOWS);
            return false;

        case LIGHT_DEFAULT:
            // Back to the lighting defaults from config.h (saved to EEPROM)
            rgb_matrix_enable();
            rgb_matrix_mode(RGB_MATRIX_DEFAULT_MODE);
            rgb_matrix_sethsv(RGB_MATRIX_DEFAULT_HUE, RGB_MATRIX_DEFAULT_SAT, RGB_MATRIX_DEFAULT_VAL);
            rgb_matrix_set_speed(RGB_MATRIX_DEFAULT_SPD);
            return false;
    }
    return true;
}

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [MAC_BASE] = LAYOUT_ansi_87(
        KC_ESC,   KC_BRID,  KC_BRIU,  KC_MCTRL, KC_LNPAD, UG_VALD,  UG_VALU,  KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,  KC_VOLD,  KC_VOLU,  KC_MUTE,   KC_SNAP,  KC_SIRI,  UG_TOGG,
        KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_MINS,  KC_EQL,   KC_BSPC,   KC_INS,   KC_HOME,  KC_PGUP,
        KC_TAB,   KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,     KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     KC_LBRC,  KC_RBRC,  KC_BSLS,   KC_DEL,   KC_END,   KC_PGDN,
        KC_CAPS,  KC_A,     KC_S,     KC_D,     KC_F,     KC_G,     KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  KC_QUOT,            KC_ENT,
        KC_LSFT,            KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,            KC_RSFT,             KC_UP,
        KC_LCTL,  KC_LOPTN, KC_LCMMD,                               KC_SPC,                                 KC_RCMMD, MO(UMLAUT),FN_MAC,   KC_RCTL,   KC_LEFT,  KC_DOWN,  KC_RGHT),

    [MAC_FN] = LAYOUT_ansi_87(
        _______,  KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,   KC_F12,   UG_TOGG,   _______,  _______,  LIGHT_DEFAULT,
        _______,  BT_HST1,  BT_HST2,  BT_HST3,  P2P4G,    _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,   _______,  _______,  _______,
        UG_TOGG,  UG_NEXT,  UG_VALU,  UG_HUEU,  UG_SATU,  UG_SPDU,  _______,  _______,  HOST_WIN, HOST_AUTO,_______,  _______,  _______,  _______,   _______,  _______,  _______,
        _______,  UG_PREV,  UG_VALD,  UG_HUED,  UG_SATD,  UG_SPDD,  _______,  _______,  _______,  HOST_LNX,  _______,  _______,            _______,
        _______,            _______,  _______,  _______,  _______,  BAT_LVL,  _______,  HOST_MAC,  _______,  _______,  _______,            _______,             _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,   _______,  _______,  _______),

    [WIN_BASE] = LAYOUT_ansi_87(
        KC_ESC,   KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,   KC_F12,   KC_MUTE,   KC_PSCR,  KC_CTANA, UG_TOGG,
        KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_MINS,  KC_EQL,   KC_BSPC,   KC_INS,   KC_HOME,  KC_PGUP,
        KC_TAB,   KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,     KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     KC_LBRC,  KC_RBRC,  KC_BSLS,   KC_DEL,   KC_END,   KC_PGDN,
        KC_CAPS,  KC_A,     KC_S,     KC_D,     KC_F,     KC_G,     KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  KC_QUOT,            KC_ENT,
        KC_LSFT,            KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,            KC_RSFT,             KC_UP,
        KC_LCTL,  KC_LWIN,  KC_LALT,                                KC_SPC,                                 KC_RALT,  MO(UMLAUT),FN_WIN,   KC_RCTL,   KC_LEFT,  KC_DOWN,  KC_RGHT),

    [WIN_FN] = LAYOUT_ansi_87(
        _______,  KC_BRID,  KC_BRIU,  KC_TASK,  KC_FILE,  UG_VALD,  UG_VALU,  KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,  KC_VOLD,  KC_VOLU,  UG_TOGG,   _______,  _______,  LIGHT_DEFAULT,
        _______,  BT_HST1,  BT_HST2,  BT_HST3,  P2P4G,    _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,   _______,  _______,  _______,
        UG_TOGG,  UG_NEXT,  UG_VALU,  UG_HUEU,  UG_SATU,  UG_SPDU,  _______,  _______,  HOST_WIN, HOST_AUTO,_______,  _______,  _______,  _______,   _______,  _______,  _______,
        _______,  UG_PREV,  UG_VALD,  UG_HUED,  UG_SATD,  UG_SPDD,  _______,  _______,  _______,  HOST_LNX,  _______,  _______,            _______,
        _______,            _______,  _______,  _______,  _______,  BAT_LVL,  _______,  HOST_MAC,  _______,  _______,  _______,            _______,             _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,   _______,  _______,  _______),

    [UMLAUT] = LAYOUT_ansi_87(
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,   _______,  _______,  _______,
        UC_DEGREE,UC_SUP1,  UC_SUP2,  UC_SUP3,  UC_SUP4,  UC_SUP5,  UC_SUP6,  UC_SUP7,  UC_SUP8,  UC_SUP9,  UC_SUP0,  _______,  _______,  _______,   _______,  _______,  _______,
        _______,  _______,  _______,  UC_EURO,  _______,  _______,  _______,  UC_UDOT,  _______,  UC_ODOT,  _______,  _______,  _______,  _______,   _______,  _______,  _______,
        _______,  UC_ADOT,  UC_ESZETT,_______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,             _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,   _______,  _______,  _______),
};

// clang-format on
#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
    [MAC_BASE] = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [MAC_FN]   = {ENCODER_CCW_CW(UG_VALD, UG_VALU)},
    [WIN_BASE] = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [WIN_FN]   = {ENCODER_CCW_CW(UG_VALD, UG_VALU)},
    [UMLAUT]   = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
};
#endif // ENCODER_MAP_ENABLE
