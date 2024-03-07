/**
 * @file screen_menu_tune.cpp
 */

#include "screen_menu_tune.hpp"
#include "marlin_client.hpp"
#include "marlin_server.hpp"
#include "utility_extensions.hpp"
#include <option/has_mmu2.h>

ScreenMenuTune::ScreenMenuTune()
    : ScreenMenuTune__(_(label)) {
    ScreenMenuTune__::ClrMenuTimeoutClose();

#if HAS_MMU2()
    // Do not allow disabling filament sensor
    if (config_store().mmu2_enabled.get()) {
        Item<MI_FILAMENT_SENSOR>().hide();
    }
#endif
}

void ScreenMenuTune::windowEvent(EventLock /*has private ctor*/, window_t *sender, GUI_event_t event, void *param) {
    switch (event) {
    case GUI_event_t::LOOP: {
        const auto current_command = marlin_client::get_command();
        Item<MI_M600>().set_is_enabled( //
            marlin_server::all_axes_homed()
            && marlin_server::all_axes_known()
            && (current_command != marlin_server::Cmd::G28)
            && (current_command != marlin_server::Cmd::G29)
            && (current_command != marlin_server::Cmd::M109)
            && (current_command != marlin_server::Cmd::M190) //
        );

#if ENABLED(CANCEL_OBJECTS)
        // Enable cancel object menu
        if (marlin_vars()->cancel_object_count > 0) {
            Item<MI_CO_CANCEL_OBJECT>().Enable();
        } else {
            Item<MI_CO_CANCEL_OBJECT>().Disable();
        }
#endif /* ENABLED(CANCEL_OBJECTS) */
        break;
    }

    default:
        break;
    }
    SuperWindowEvent(sender, event, param);
}
