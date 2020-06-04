#include <libfungw/fungw.h>

/* Common: register the engine so new objects can be created */
void string_init(void);


/* Uncommon: for testing direct calls */
fgw_error_t fgwc_malloc(fgw_arg_t *res, int argc, fgw_arg_t *argv);
fgw_error_t fgwc_free(fgw_arg_t *res, int argc, fgw_arg_t *argv);
fgw_error_t fgwc_atoi(fgw_arg_t *res, int argc, fgw_arg_t *argv);



