#include <stdio.h>
#include <locale.h>
#include <librnd/core/misc_util.h>
#include <librnd/core/pcb-printf.h>
#include <librnd/core/pcb_bool.h>

int main(int argc, char *argv[])
{
	pcb_printf_slot[0] = "%{ ()}mq";
	setlocale(LC_ALL, "C");
	pcb_fprintf(stdout, argv[1], argv[2]);
	printf("\n");
	return 0;
}
