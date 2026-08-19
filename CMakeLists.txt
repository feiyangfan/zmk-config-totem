# Compile the active-layer BLE reporter only into the TOTEM dongle build.
if(CONFIG_SHIELD_TOTEM_DONGLE)
    target_sources(app PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/layer_state_ble.c
    )
endif()
