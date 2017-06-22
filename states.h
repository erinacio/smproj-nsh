#ifndef STATES_H
#define STATES_H

#include <stdbool.h>

extern bool state_debug;

typedef enum state_debug_level_e {
    DEBUG_IL_ONLY,  // Only generates IL, won't feed them to the VM
    DEBUG_NO_EXEC,  // Feeds ILs to the VM, but won't actually execute them
    DEBUG_NORMAL,   // Runs as usual
} state_debug_level_t;

extern state_debug_level_t state_debug_level;

#endif // STATES_H
