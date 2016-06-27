#pragma once

#include "toast/library.hpp"

typedef struct {
    char name[32];
    bool is_simulation;
    bool is_wpilib;
} ProviderInfo;

CAPI ProviderInfo *provider_info();

// Called before Toast Loading
CAPI void provider_preinit();

// Called after Toast loading
CAPI void provider_init();

CAPI void thp_state_set_callback(void (*callback_periodic)(bool,bool,bool,bool), void (*callback_transition)(bool,bool,bool,bool));
CAPI void thp_state_call_periodic(bool disabled, bool auton, bool teleop, bool test);
CAPI void thp_state_call_init(bool disabled, bool auton, bool teleop, bool test);