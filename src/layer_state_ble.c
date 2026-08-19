/*
 * TOTEM active-layer BLE reporter
 *
 * Exposes the dongle's current highest active ZMK layer as one byte over BLE.
 *
 * Service UUID:
 *   7D8C5F20-7C8A-4F45-9C84-2F6E8A7B2000
 *
 * Active-layer characteristic UUID:
 *   7D8C5F21-7C8A-4F45-9C84-2F6E8A7B2000
 *   READ + NOTIFY
 *
 * Key-event characteristic UUID:
 *   7D8C5F22-7C8A-4F45-9C84-2F6E8A7B2000
 *   NOTIFY
 *
 * Key-event payload:
 *   [position, state]
 *   state: 1 = pressed, 0 = released
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdint.h>

#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/sys/util.h>

#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>


#define BT_UUID_TOTEM_LAYER_SERVICE_VAL \
    BT_UUID_128_ENCODE(0x7d8c5f20, 0x7c8a, 0x4f45, 0x9c84, 0x2f6e8a7b2000)

#define BT_UUID_TOTEM_LAYER_CHAR_VAL \
    BT_UUID_128_ENCODE(0x7d8c5f21, 0x7c8a, 0x4f45, 0x9c84, 0x2f6e8a7b2000)

#define BT_UUID_TOTEM_KEY_EVENT_CHAR_VAL \
    BT_UUID_128_ENCODE(0x7d8c5f22, 0x7c8a, 0x4f45, 0x9c84, 0x2f6e8a7b2000)


static struct bt_uuid_128 layer_service_uuid =
    BT_UUID_INIT_128(BT_UUID_TOTEM_LAYER_SERVICE_VAL);

static struct bt_uuid_128 layer_char_uuid =
    BT_UUID_INIT_128(BT_UUID_TOTEM_LAYER_CHAR_VAL);

static struct bt_uuid_128 key_event_char_uuid =
    BT_UUID_INIT_128(BT_UUID_TOTEM_KEY_EVENT_CHAR_VAL);

static uint8_t active_layer;

/*
 * Key event payload:
 *   byte 0 = physical key position (0-255)
 *   byte 1 = state (1 = pressed, 0 = released)
 *
 * TOTEM has 38 positions, so one byte is sufficient for position.
 */
static uint8_t key_event_payload[2];


static void refresh_active_layer(void) {
    active_layer = (uint8_t)zmk_keymap_highest_layer_active();
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
        &active_layer,
        sizeof(active_layer)
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


BT_GATT_SERVICE_DEFINE(
    totem_layer_service,

    BT_GATT_PRIMARY_SERVICE(
        &layer_service_uuid.uuid
    ),

    BT_GATT_CHARACTERISTIC(
        &layer_char_uuid.uuid,
        BT_GATT_CHRC_READ | BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_READ,
        read_active_layer,
        NULL,
        &active_layer
    ),

    BT_GATT_CCC(
        layer_ccc_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE
    ),

    BT_GATT_CHARACTERISTIC(
        &key_event_char_uuid.uuid,
        BT_GATT_CHRC_NOTIFY,
        BT_GATT_PERM_NONE,
        NULL,
        NULL,
        NULL
    ),

    BT_GATT_CCC(
        key_event_ccc_changed,
        BT_GATT_PERM_READ | BT_GATT_PERM_WRITE
    )
);


static int layer_state_listener(const zmk_event_t *eh) {
    const struct zmk_layer_state_changed *event =
        as_zmk_layer_state_changed(eh);

    if (event == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    refresh_active_layer();

    /*
     * Attribute layout for this service:
     *   0 = Primary Service
     *   1 = Characteristic Declaration
     *   2 = Characteristic Value
     *   3 = CCC Descriptor
     *
     * Notifications must target the characteristic VALUE attribute.
     */
    (void)bt_gatt_notify(
        NULL,
        &totem_layer_service.attrs[2],
        &active_layer,
        sizeof(active_layer)
    );

    return ZMK_EV_EVENT_BUBBLE;
}


ZMK_LISTENER(
    totem_layer_state,
    layer_state_listener
);

ZMK_SUBSCRIPTION(
    totem_layer_state,
    zmk_layer_state_changed
);


static int key_position_listener(const zmk_event_t *eh) {
    const struct zmk_position_state_changed *event =
        as_zmk_position_state_changed(eh);

    if (event == NULL) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    /*
     * The ZMK event position is the keymap position. TOTEM currently has
     * only 38 positions, so the compact two-byte payload is sufficient.
     */
    if (event->position > UINT8_MAX) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    key_event_payload[0] = (uint8_t)event->position;
    key_event_payload[1] = event->state ? 1U : 0U;

    /*
     * Attribute layout:
     *   0 = Primary Service
     *   1 = Layer Characteristic Declaration
     *   2 = Layer Characteristic Value
     *   3 = Layer CCC
     *   4 = Key Event Characteristic Declaration
     *   5 = Key Event Characteristic Value
     *   6 = Key Event CCC
     */
    (void)bt_gatt_notify(
        NULL,
        &totem_layer_service.attrs[5],
        key_event_payload,
        sizeof(key_event_payload)
    );

    return ZMK_EV_EVENT_BUBBLE;
}


ZMK_LISTENER(
    totem_key_position,
    key_position_listener
);

ZMK_SUBSCRIPTION(
    totem_key_position,
    zmk_position_state_changed
);
