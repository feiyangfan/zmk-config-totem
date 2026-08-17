# Architecture and compatibility report

## Inspected source snapshots

- User TOTEM repository: `feiyangfan/zmk-config-totem` at `83251955e2e2777bf82266922f688afddf1d7870` (2023-05-16).
- Seller Corne dongle repository: `tokyo2006/zmk-corne-dongle` at `925382e03385e671b7afaeabfd3e06e401470d08` (2025-10-16).
- Tested public TOTEM dongle reference: `eigatech/zmk-config`, branch `totem-dongle`, at `a847541997b4c215c128cc42398e2eedcf6046c9`.
- Tested public TOTEM Prospector/Studio reference: `eigatech/zmk-config`, branch `totem-prospector`, at `43cfdaaa2c2611b0609a8831825adfd4ca5d5467`.
- Current ZMK snapshot used by this migration: `6e2ef41e022d555b10f116e395832913f71717b3`.
- Current `zmk-dongle-display` snapshot: `2bb333f87136d33e94a49d86236ed9ec254a8060`.

The original `CURRENT.UF2`, `INFO_UF2.TXT`, and `INDEX.HTM` were named in the handoff but were not included in the supplied attachment directory. Their contents and hashes remain unverified.

## Existing seller Corne dongle architecture

The seller build matrix has one keyless nice!nano v2 central and two nice!nano v2 peripherals:

- Central: `eyeslash_corne_central_dongle dongle_display`, with `studio-rpc-usb-uart`, Studio enabled, and Studio locking disabled.
- Left and right: `eyeslash_corne_peripheral_left/right nice_view_custom`.
- Reset: nice!nano `settings_reset`.

The central uses a zero-key mock kscan, sets `CONFIG_ZMK_SPLIT_ROLE_CENTRAL=y`, accepts exactly two split peripherals, and raises the Bluetooth connection/paired limits to six. Its overlay defines a 129x64 SH1106 at I2C address `0x3c`, using SDA `P1.06` and SCL `P0.10`. These pin mappings came directly from the working seller source and were not inferred.

The Corne shield itself is a 48-position logical layout using a 5x14 transform. Its half overlays include Corne matrix pins, encoder, RGB underglow, PWM backlight, external-power control, and nice!view SPI wiring. Those are not TOTEM hardware.

The active OLED UI comes from `zmk-dongle-display`. It implements output/profile state, peripheral battery values, modifiers, HID indicators, Bongo Cat, active layer, and optional WPM. In the seller configuration, WPM and dongle-battery display are supported by the module but are not enabled. Peripheral battery fetching is enabled. The SH1106 hardware definition is in the seller's central overlay, not in the display module.

`hammerbeam-slideshow` is used only by the seller's Corne peripheral nice!view builds. `prospector-zmk-module` is declared in the seller manifest but its custom central implementation is conditional on the `prospector_adapter` shield, which the seller build matrix does not select. Neither module is needed for the TOTEM halves or the SH1106 dongle build.

## Existing TOTEM architecture

The original matrix builds `totem_left` and `totem_right` for `seeeduino_xiao_ble`. `Kconfig.defconfig` makes only the left shield central. Both halves share a 4x10 logical matrix and the same 38-position transform; the right overlay adds a five-column offset. The row and column GPIO definitions are the verified working XIAO mappings.

The effective user keymap is `config/totem.keymap`, not the fallback keymap inside the shield folder. It has six layers, two combos, the `gif` macro, mod-taps, layer-taps, Bluetooth/output controls, and reset/bootloader bindings. The migration copy is byte-for-byte identical.

The original shield chooses `zmk,matrix_transform` directly and has no Studio physical-layout node. On current ZMK, the XIAO board itself provides the voltage-divider battery device used by each peripheral.

## ZMK version compatibility

Both source repositories specify `revision: main`, and both workflows call the ZMK reusable workflow at `@main`. They therefore do not identify an exact historical ZMK version for their known-good UF2 files. Building either repository today resolves to the same current ZMK line, so there is no separate version upgrade to perform before merging.

Current ZMK uses Zephyr 4.1 board identifiers: `xiao_ble//zmk` replaces `seeeduino_xiao_ble`, and `nice_nano//zmk` selects nice!nano v2 by default. This migration pins both ZMK and the display module to exact commits so later upstream changes cannot silently alter a build.

The obsolete peripheral assignment of `CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_PROXY` was not copied. In the pinned ZMK source that option is central-only and depends on battery fetching. The OLED consumes the central's peripheral battery events, enabled by `CONFIG_ZMK_SPLIT_BLE_CENTRAL_BATTERY_LEVEL_FETCHING=y`.

## File classification

### Reused unchanged

- `config/totem.keymap` and `config/totem.conf` from the user's TOTEM repository.
- All verified TOTEM matrix GPIO values and the existing 38-position matrix transform.
- The seller's SH1106 dimensions, address, offsets, flags, precharge value, and I2C pin selections.
- The external `zmk-dongle-display` implementation.
- Studio's USB-UART snippet and seller setting that disables Studio locking.

### Reused with TOTEM parameters

- Seller keyless-central pattern: renamed to `totem_dongle` and pointed at the TOTEM transform/layout.
- Seller split count and Bluetooth capacity.
- Seller peripheral battery fetching.
- Seller build matrix, with current board identifiers and XIAO reset output added.

### Corne-specific and replaced

- Every `eyeslash_corne*` shield file and both Corne keymaps.
- The 48-position Corne transform and physical geometry.
- Corne row/column pins, encoder, RGB, PWM backlight, external-power, and nice!view SPI configuration.
- Corne JSON and keymap-drawer outputs.
- `nice_view_custom` peripheral display builds and their Hammerbeam dependency.

### Potentially incompatible or unverified

- The exact ZMK revision used to create the user's currently flashed TOTEM UF2 files.
- The exact source revision used to create `CURRENT.UF2`.
- Runtime SH1106 behavior on this individual dongle until hardware testing.
- BLE bond migration and slot order until the documented reset/pair sequence is performed.
- Runtime Studio behavior until the compiled dongle is connected to Studio.

## Physical layout source and validation

The physical layout is reused from the published `eigatech/zmk-config` TOTEM dongle/Prospector branches. Its source `info.json` describes the physical TOTEM rotations and coordinates. It contains exactly 38 keys, and its transform is identical to the user's existing 38-entry transform. No logical key ordering changed.

## Final repository structure

```text
build.yaml
ARCHITECTURE.md
MIGRATION.md
config/
  west.yml
  totem.conf
  totem.keymap
  boards/shields/totem/
    Kconfig.defconfig
    Kconfig.shield
    totem.dtsi
    totem.zmk.yml
    totem_left.conf
    totem_left.overlay
    totem_right.conf
    totem_right.overlay
    totem_dongle.conf
    totem_dongle.overlay
```

The Git history is based on the user's TOTEM repository so the keymap remains the direct source of truth. The difficult dongle architecture—the keyless central, exact display overlay, display module, battery fetching, Studio transport, and connection sizing—is imported from the seller repository. This produces a smaller and more auditable change than replacing the TOTEM repository with the Corne tree.

## Build matrix

| Artifact | Board | Shield(s) | Role |
| --- | --- | --- | --- |
| `totem_dongle.uf2` | `nice_nano//zmk` | `totem_dongle dongle_display` | central, OLED, USB/BLE HID, Studio |
| `totem_left.uf2` | `xiao_ble//zmk` | `totem_left` | peripheral |
| `totem_right.uf2` | `xiao_ble//zmk` | `totem_right` | peripheral |
| `settings_reset_nice_nano.uf2` | `nice_nano//zmk` | `settings_reset` | dongle reset |
| `settings_reset_xiao.uf2` | `xiao_ble//zmk` | `settings_reset` | half reset |

## Implementation and validation sequence

1. Pin current ZMK and `zmk-dongle-display` revisions.
2. Add the keyless `totem_dongle` central without changing the keymap.
3. Move central role selection from the left shield to the dongle shield.
4. Add the tested 38-key physical layout while preserving the transform.
5. Add current board identifiers, both reset targets, and deterministic artifact names.
6. Build all five targets from one commit.
7. Do not flash until backup files are present, hashed, and the pre-flash gate passes.
8. Reset and pair dongle, left, then right; validate input over USB before display and host BLE testing.
9. Validate Studio last, after the core split and OLED are stable.
