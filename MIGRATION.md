# TOTEM display-dongle migration

## Build outputs

`build.yaml` produces five artifacts:

- `totem_dongle.uf2`: nice!nano v2 central, SH1106 display, and ZMK Studio over USB.
- `totem_left.uf2`: XIAO BLE left peripheral.
- `totem_right.uf2`: XIAO BLE right peripheral.
- `settings_reset_nice_nano.uf2`: settings reset for the nice!nano dongle only.
- `settings_reset_xiao.uf2`: settings reset for either XIAO BLE half only.

Never interchange the nice!nano and XIAO reset images.

## Pre-flash gate

Do not flash any experimental image until all of the following are true:

1. All five build-matrix entries have completed successfully from the same commit.
2. The original `CURRENT.UF2`, `INFO_UF2.TXT`, and `INDEX.HTM` are stored outside this repository and have recorded SHA-256 hashes.
3. Known-good original TOTEM left and right UF2 files are stored outside this repository and have recorded SHA-256 hashes.
4. The controller identity for every file is written down: nice!nano v2 for the dongle and XIAO BLE for both halves.

Regular ZMK firmware does not erase persistent settings. The reset images are therefore required when changing the split topology.

## First migration

Keep devices that are not mentioned in the current step powered off. Boot each settings-reset image once before replacing it with the production image; a reset image is not normal keyboard firmware.

1. In macOS Bluetooth settings, forget the old TOTEM entry and any old dongle keyboard entry.
2. Power off both TOTEM halves.
3. On the dongle, flash `settings_reset_nice_nano.uf2`, allow it to boot, then flash `totem_dongle.uf2`.
4. Keep the right half off. On the left half, flash `settings_reset_xiao.uf2`, allow it to boot, then flash `totem_left.uf2`. Power on the dongle and left half and wait for them to connect.
5. On the right half, flash `settings_reset_xiao.uf2`, allow it to boot, then flash `totem_right.uf2`. Power it on and wait for it to connect.
6. Pair the dongle's TOTEM Bluetooth device with macOS if BLE host output is wanted. USB HID works through the dongle without host BLE pairing.
7. Power-cycle the dongle and each half independently and confirm reconnection.

Pairing the left half before the right half makes the display's peripheral battery order deterministic: left first, right second.

## Functional test order

Test with USB host output first, before adding BLE host pairing or judging the display UI.

1. Confirm every left and right key position against the physical layout.
2. Confirm all six layers, mod-taps, layer-taps, both combos, the `gif` macro, Bluetooth controls, and reset/bootloader bindings.
3. Confirm the dongle reconnects after a power cycle and after each half is power-cycled independently.
4. Confirm the OLED shows output state, active layer, modifiers, animation, and two peripheral battery values as those values become available.
5. Confirm BLE host output, then switch between USB and BLE using the existing keymap behavior.
6. Connect ZMK Studio to the dongle over USB and confirm that the 38-key TOTEM layout is rendered in the same order as the physical keyboard.

## ZMK Studio persistence

Studio locking is disabled in this first migration build, matching the seller repository, so an `&studio_unlock` binding is not required and no existing key was displaced.

Studio saves runtime key assignments in the dongle's settings partition. After Studio has saved a change, later edits to `config/totem.keymap` will not become active merely by reflashing firmware. Use **Restore Stock Settings** in ZMK Studio to return to the source-defined keymap. Flashing `settings_reset_nice_nano.uf2` also clears Studio data, but it additionally clears split and host bonds and therefore requires the full pairing procedure again.

Macros, combos, custom behaviors, and other advanced definitions remain source-controlled. Studio can assign behavior instances already present in the firmware but cannot define every advanced construct.

## Rollback

Do not begin rollback unless the original three dongle backup files and both known-good TOTEM UF2 files have been verified.

1. Forget the experimental TOTEM/dongle entry in macOS Bluetooth settings.
2. Power off all three devices.
3. Flash `settings_reset_xiao.uf2` to the left XIAO, boot it once, then flash the original known-good TOTEM left-central UF2.
4. Flash `settings_reset_xiao.uf2` to the right XIAO, boot it once, then flash the original known-good TOTEM right-peripheral UF2.
5. Power on left and right and allow their original split relationship to pair again. Pair the restored left central with macOS.
6. Flash `settings_reset_nice_nano.uf2` to the dongle, boot it once, then flash the untouched original `CURRENT.UF2`. Recreate any original Corne split/host bonds if that dongle is returned to its former keyboard.

The reset step is intentional: production UF2 flashing alone does not clear BLE bonds, split pairing data, host profiles, output selection, or Studio runtime settings.
