# ZMK Overlay Companion
#
# Enable only on the keyboard side that processes the keymap and connects
# to the host (the central half or a dedicated dongle).
if(CONFIG_ZMK_OVERLAY_COMPANION)
    target_sources(app PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/src/overlay_companion.c
    )
endif()
