#include "../libfungw/scconfig_hooks.h"
#include "../libfungwbind/scconfig_hooks.h"

static void help1(void)
{
	argtbl_t *a;

	printf("./configure: configure fungw.\n");
	printf("\n");
	printf("Usage: ./configure [options]\n");
	printf("\n");
	printf("options are:\n");
	printf(" --prefix=path          change installation prefix from /usr to path\n");
	printf(" --libdirname=name      change the name of LIBDIR from lib to name (e.g. lib64)\n");
	printf(" --pupdirname=path      change the name of PUPDIR from lib/puplug to path\n");
	printf(" --debug                build full debug version (-g -O0, extra asserts)\n");
	for(a = main_argument_table; a->arg != NULL; a++) {
		if (a->help != NULL) {
			char tmp[64];
			sprintf(tmp, "%s=str", a->arg);
			printf(" --%-20s %s\n", tmp, a->help);
		}
	}
}

/* Runs when a custom command line argument is found
 returns true if no furhter argument processing should be done */
int hook_custom_arg(const char *key, const char *value)
{
	if (strcmp(key, "debug") == 0) {
		fungw_set_debug(strue);
		return 1;
	}
	if (strcmp(key, "prefix") == 0) {
		fungw_set_prefix(value);
		return 1;
	}
	if (strcmp(key, "libdirname") == 0) {
		fungw_set_libdirname(value);
		return 1;
	}
	if (strcmp(key, "pupdirname") == 0) {
		fungw_set_pupdirname(value);
		return 1;
	}
	else if (strcmp(key, "help") == 0) {
		help1();
		exit(0);
	}
	return 0;
}


/* Runs before anything else */
int hook_preinit()
{
	return 0;
}

/* Runs after initialization */
int hook_postinit()
{
	return fungw_hook_postinit() | fungwbind_hook_postinit();
}

/* Runs after all arguments are read and parsed */
int hook_postarg()
{
	return 0;
}

/* Runs when things should be detected for the host system */
int hook_detect_host()
{
	return fungw_hook_detect_host();
}

/* Runs when things should be detected for the target system */
int hook_detect_target()
{
	return fungw_hook_detect_target() | fungwbind_hook_detect_target();
}

/* Runs after detection hooks, should generate the output (Makefiles, etc.) */
int hook_generate()
{
	unsigned generr = 0;

	generr |= fungw_hook_generate("../doc");
	generr |= fungw_hook_generate("../libfungw");
	generr |= fungw_hook_generate("../example");
	generr |= fungwbind_hook_generate("../libfungwbind");

	fprintf(stderr, "Generating example/static_lang.c (%d)\n", generr |= tmpasm("../example", "static_lang.c.in", "static_lang.c"));

	fungw_print_configure_summary();

	return generr;
}

/* Runs before everything is uninitialized */
void hook_preuninit()
{
}

/* Runs at the very end, when everything is already uninitialized */
void hook_postuninit()
{
}

