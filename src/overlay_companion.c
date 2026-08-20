/*
 * ZMK Overlay Companion
 *
 * Generic BLE telemetry service for ZMK keyboard overlay applications.
 *
 * Service UUID:
 *   7D8C5F20-7C8A-4F45-9C84-2F6E8A7B2000
 *
 * Characteristics:
 *
 * F21 Active layer
 *   READ + NOTIFY
 *   payload: uint16 little-endian layer ID
 *
 * F22 Physical key event
 *   NOTIFY
 *   payload:
 *     bytes 0-1 = uint16 little-endian keymap position
 *     byte 2    = state (1 = pressed, 0 = released)
 *
 * F23 Protocol information
 *   READ
 *   payload:
 *     byte 0    = protocol major
 *     byte 1    = protocol minor
 *     bytes 2-3 = uint16 little-endian capability flags
 *
 * F24 Explicit modifier state/event
 *   READ + NOTIFY
 *   payload:
 *     byte 0 = active modifier mask
 *     byte 1 = modifier involved in this event (one-bit mask, 0 for READ)
 *     byte 2 = event state (1 = pressed, 0 = released; 0 for READ)
 *
 * Modifier mask:
 *   bit 0 = Left Ctrl
 *   bit 1 = Left Shift
 *   bit 2 = Left Alt
 *   bit 3 = Left GUI
 *   bit 4 = Right Ctrl
 *   bit 5 = Right Shift
 *   bit 6 = Right Alt
 *   bit 7 = Right GUI
 *
 * Capability flags:
 *   bit 0 = active-layer reporting
 *   bit 1 = physical key-event reporting
 *   bit 2 = explicit modifier-state reporting
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/sys/util.h>

#include <zmk/event_manager.h>
#include <zmk/events/keycode_state_changed.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/hid.h>
#include <zmk/keymap.h>
#include <zmk/keys.h>

#define ZMK_OVERLAY_PROTOCOL_MAJOR 1U
#define ZMK_OVERLAY_PROTOCOL_MINOR 1U

#define ZMK_OVERLAY_CAP_ACTIVE_LAYER   BIT(0)
#define ZMK_OVERLAY_CAP_KEY_EVENTS     BIT(1)
#define ZMK_OVERLAY_CAP_MODIFIER_STATE BIT(2)

#define ZMK_OVERLAY_CAPABILITIES                                              \
    (ZMK_OVERLAY_CAP_ACTIVE_LAYER | ZMK_OVERLAY_CAP_KEY_EVENTS |             \
     ZMK_OVERLAY_CAP_MODIFIER_STATE)

#define BT_UUID_ZMK_OVERLAY_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x7d8c5f20, 0x7c8a, 0x4f45, 0x9c84, 0x2f6e8a7b2000)

#define BT_UUID_ZMK_OVERLAY_LAYER_CHAR_VAL \
    BT_UUID_128_ENCODE(0x7d8c5f21, 0x7c8a, 0x4f45, 0x9c84, 0x2f6e8a7b2000)

#define BT_UUID_ZMK_OVERLAY_KEY_EVENT_CHAR_VAL \
    BT_UUID_128_ENCODE(0x7d8c5f22, 0x7c8a, 0x4f45, 0x9c84, 0x2f6e8a7b2000)

#define BT_UUID_ZMK_OVERLAY_PROTOCOL_INFO_CHAR_VAL \
    BT_UUID_128_ENCODE(0x7d8c5f23, 0x7c8a, 0x4f45, 0x9c84, 0x2f6e8a7b2000)

#define BT_UUID_ZMK_OVERLAY_MODIFIER_STATE_CHAR_VAL \
    BT_UUID_128_ENCODE(0x7d8c5f24, 0x7c8a, 0x4f45, 0x9c84, 0x2f6e8a7b2000)

static struct bt_uuid_128 overlay_service_uuid =
    BT_UUID_INIT_128(BT_UUID_ZMK_OVERLAY_SERVICE_VAL);

static struct bt_uuid_128 layer_char_uuid =
    BT_UUID_INIT_128(BT_UUID_ZMK_OVERLAY_LAYER_CHAR_VAL);

static struct bt_uuid_128 key_event_char_uuid =
    BT_UUID_INIT_128(BT_UUID_ZMK_OVERLAY_KEY_EVENT_CHAR_VAL);

static struct bt_uuid_128 protocol_info_char_uuid =
    BT_UUID_INIT_128(BT_UUID_ZMK_OVERLAY_PROTOCOL_INFO_CHAR_VAL);

static struct bt_uuid_128 modifier_state_char_uuid =
    BT_UUID_INIT_128(BT_UUID_ZMK_OVERLAY_MODIFIER_STATE_CHAR_VAL);

/* uint16 little-endian active layer. */
static uint8_t active_layer_payload[2];

/* uint16 little-endian position + uint8 state. */
static uint8_t key_event_payload[3];

/*
 * Explicit modifier state as an HID modifier bitmask.
 *
 * We keep local press counts just like ZMK HID does. This means two
 * independent bindings holding the same modifier do not cause the overlay
 * modifier state to clear when only one of them is released.
 */
static uint8_t modifier_state_payload[3];
static uint8_t modifier_press_counts[8];

/* major, minor, uint16 little-endian capability mask. */
static const uint8_t protocol_info_payload[4] = {
    ZMK_OVERLAY_PROTOCOL_MAJOR,
    ZMK_OVERLAY_PROTOCOL_MINOR,
    (uint8_t)(ZMK_OVERLAY_CAPABILITIES & 0xFFU),
    (uint8_t)((ZMK_OVERLAY_CAPABILITIES >> 8) & 0xFFU),
};

static void refresh_active_layer(void) {
    uint16_t layer =
        (uint16_t)zmk_keymap_highest_layer_active();

    sys_put_le16(
        layer,
        active_layer_payload
    );
}

static void refresh_modifier_state(void) {
    /*
     * Used by READ so a newly connected overlay gets ZMK's current state
     * even if the modifier was already active before the BLE subscription.
     */
    modifier_state_payload[0] =
        (uint8_t)zmk_hid_get_explicit_mods();

    modifier_state_payload[1] = 0U;
    modifier_state_payload[2] = 0U;
}

static ssize_t read_active_layer(
    struct bt_conn *conn,
    const struct bt_gatt_attr *attr,
    void *buf,
    uint16_t len,
    uint16_t offset
) {
    refresh_active_layer();

    return bt_gatt_attr_read(
        conn,
        attr,
        buf,
        len,
        offset,
        active_layer_payload,
        sizeof(active_layer_payload)
    );
}

static ssize_t read_protocol_info(
    struct bt_conn *conn,
    const struct bt_gatt_attr *attr,
    void *buf,
    uint16_t len,
    uint16_t offset
) {
    return bt_gatt_attr_read(
        conn,
        attr,
        buf,
        len,
        offset,
        protocol_info_payload,
        sizeof(protocol_info_payload)
    );
}

static ssize_t read_modifier_state(
    struct bt_conn *conn,
    const struct bt_gatt_attr *attr,
    void *buf,
    uint16_t len,
    uint16_t offset
) {
    refresh_modifier_state();

    return bt_gatt_attr_read(
        conn,
        attr,
        buf,
        len,
        offset,
        modifier_state_payload,
        sizeof(modifier_state_payload)
    );
}

static void layer_ccc_changed(
    const struct bt_gatt_attr *attr,
    uint16_t value
) {
    ARG_UNUSED(attr);
    ARG_UNUSED(value);
}

static void key_event_ccc_changed(
    const struct bt_gatt_attr *attr,
    uint16_t value
) {
    ARG_UNUSED(attr);
    ARG_UNUSED(value);
}

static void modifier_state_ccc_changed(
    const struct bt_gatt_attr *attr,
    uint16_t value
) {
    ARG_UNUSED(attr);
    ARG_UNUSED(value);
}

BT_GATT_SERVICE_DEFINE(
    zmk_overlay_companion_service,

    BT_GATT_PRIMARY_SERVICE(
        &overlay_service_uuid.uuid
    ),

    /* attrs[1] declaration, attrs[2] value */
    BT_GATT_CHARACTERISTIC(
        &layer_char_uuid.uuid,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ,
        read_active_layer,
        NULL,
        active_layer_payload
    ),

    /* attrs[3] */
    BT_GATT_CCC(
        layer_ccc_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE
    ),

    /* attrs[4] declaration, attrs[5] value */
    BT_GATT_CHARACTERISTIC(
        &key_event_char_uuid.uuid,
        BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_NONE,
        NULL,
        NULL,
        NULL
    ),

    /* attrs[6] */
    BT_GATT_CCC(
        key_event_ccc_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE
    ),

    /* attrs[7] declaration, attrs[8] value */
    BT_GATT_CHARACTERISTIC(
        &protocol_info_char_uuid.uuid,
        BT_GATT_CHRC_READ,
        BT_GATT_PERM_READ,
        read_protocol_info,
        NULL,
        (void *)protocol_info_payload
    ),

    /* attrs[9] declaration, attrs[10] value */
    BT_GATT_CHARACTERISTIC(
        &modifier_state_char_uuid.uuid,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ,
        read_modifier_state,
        NULL,
        modifier_state_payload
    ),

    /* attrs[11] */
    BT_GATT_CCC(
        modifier_state_ccc_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE
    )
);

static int overlay_layer_state_listener(
    const zmk_event_t *eh
) {
    const struct zmk_layer_state_changed *event =
        as_zmk_layer_state_changed(eh);

    if (event == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    refresh_active_layer();

    (void)bt_gatt_notify(
        NULL,
        &zmk_overlay_companion_service.attrs[2],
        active_layer_payload,
        sizeof(active_layer_payload)
    );

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(
    zmk_overlay_layer_state,
    overlay_layer_state_listener
);

ZMK_SUBSCRIPTION(
    zmk_overlay_layer_state,
    zmk_layer_state_changed
);

static int overlay_key_position_listener(
    const zmk_event_t *eh
) {
    const struct zmk_position_state_changed *event =
        as_zmk_position_state_changed(eh);

    if (event == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    if (event->position > UINT16_MAX) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    sys_put_le16(
        (uint16_t)event->position,
        key_event_payload
    );

    key_event_payload[2] =
        event->state ? 1U : 0U;

    (void)bt_gatt_notify(
        NULL,
        &zmk_overlay_companion_service.attrs[5],
        key_event_payload,
        sizeof(key_event_payload)
    );

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(
    zmk_overlay_key_position,
    overlay_key_position_listener
);

ZMK_SUBSCRIPTION(
    zmk_overlay_key_position,
    zmk_position_state_changed
);

static int overlay_modifier_state_listener(
    const zmk_event_t *eh
) {
    const struct zmk_keycode_state_changed *event =
        as_zmk_keycode_state_changed(eh);

    if (event == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    /*
     * Hold-tap does not emit a modifier keycode until the hold side has
     * actually resolved. That makes this event a useful ground truth for
     * distinguishing "physically down but undecided" from "modifier hold".
     */
    if (!is_mod(event->usage_page, event->keycode)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    uint8_t modifier_index =
        (uint8_t)(
            event->keycode -
            HID_USAGE_KEY_KEYBOARD_LEFTCONTROL
        );

    if (modifier_index >= ARRAY_SIZE(modifier_press_counts)) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    uint8_t modifier_bit =
        (uint8_t)BIT(modifier_index);

    if (event->state) {
        if (modifier_press_counts[modifier_index] < UINT8_MAX) {
            modifier_press_counts[modifier_index]++;
        }

        modifier_state_payload[0] |= modifier_bit;
    } else {
        if (modifier_press_counts[modifier_index] > 0U) {
            modifier_press_counts[modifier_index]--;
        }

        if (modifier_press_counts[modifier_index] == 0U) {
            modifier_state_payload[0] &=
                (uint8_t)~modifier_bit;
        }
    }

    /*
     * Include the modifier event itself as well as the resulting mask.
     * The desktop app uses this to associate a real modifier activation
     * with the physically held hold-tap position.
     */
    modifier_state_payload[1] = modifier_bit;
    modifier_state_payload[2] =
        event->state ? 1U : 0U;

    (void)bt_gatt_notify(
        NULL,
        &zmk_overlay_companion_service.attrs[10],
        modifier_state_payload,
        sizeof(modifier_state_payload)
    );

    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(
    zmk_overlay_modifier_state,
    overlay_modifier_state_listener
);

ZMK_SUBSCRIPTION(
    zmk_overlay_modifier_state,
    zmk_keycode_state_changed
);
