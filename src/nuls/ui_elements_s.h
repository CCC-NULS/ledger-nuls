#include "os_io_seproxyhal.h"

extern const ux_menu_entry_t menu_main[4];
extern const ux_menu_entry_t menu_about[3];

unsigned char nuls_hex_amount_to_displayable(unsigned char *amount, char *dest);
void ui_idle();
extern ux_state_t ux;
