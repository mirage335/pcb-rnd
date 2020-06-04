/*
 *                            COPYRIGHT
 *
 *  sphash -- simple, (almost) perfect hash, generated compile time
 *  Copyright (C) 2008, 2011, 2013, 2014, 2016 Tibor 'Igor2' Palinkas
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 *  THE ABOVE LICENSE DOES NOT AFFECT THE OUTPUT GENERATED RUNNING THE
 *  PROGRAM: the output is the property of the person who ran the program.
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <stdarg.h>

char *aux_fields = NULL;
char **data = NULL;
char **aux_data = NULL;
int *values  = NULL;
int *got  = NULL;

int data_num = 0;
int hash_size;

int overrun = 1000;
int debug_level = 0;

int SPHASH_SEED0 = 1;
int SPHASH_SEED1 = 0;
int SPHASH_SEED2 = 0;

int SPHASH_SEED0_best = -1; /* to */
int SPHASH_SEED1_best = -1; /* from */
int SPHASH_SEED2_best = -1; /* seed */

int shortest = 1000;

int last;

int multi = 0;

#define sphash_function(acc, c, n)		acc += (acc << SPHASH_SEED2) + (c)
char *sphash_function_str = "#define sphash_function(acc, c, n)		acc += (acc << SPHASH_SEED2) + (c)";

int lprintf(int level, const char *fmt, ...)
{
	int ret;
	va_list ap;

	if (level > debug_level)
		return 0;

	va_start(ap, fmt);
	ret = vprintf(fmt, ap);
	va_end(ap);
	return ret;
}

#define banner "/* Generated by sphash - do not modify */\n"

/* C89-portable strdup */
static char *sphash_strdup(const char *s)
{
	char *r;
	size_t len;

	if (s == NULL)
		return NULL;

	len = strlen(s)+1;
	r = malloc(len);
	memcpy(r, s, len);
	return r;
}

typedef struct multi_s multi_t;
struct multi_s {
	char *s;
	char *s_id;
	int value;
	multi_t *next;
	multi_t *children;
};

multi_t *multi_enums = NULL;

multi_t *multi_alloc(const char *s, int value, multi_t **linkin)
{
	multi_t *m;
	char *i;

	m = malloc(sizeof(multi_t));
	m->s = sphash_strdup(s);
	m->s_id = sphash_strdup(s);
	for(i = m->s_id; *i != '\0'; i++) {
		if (!isalnum(*i)) {
			*i = '_';
		}
	}
	m->value = value;
	m->children = NULL;
	if (linkin != NULL) {
		m->next = *linkin;
		*linkin = m;
	}
	else
		m->next = NULL;
	return m;
}

int load_strings(FILE *f)
{
	int allocated;
	char line[1024], *s;
	int n;
	multi_t *parent;
	int enum_num, keys;

	allocated = 0;
	enum_num = keys = 0;

	while(!(feof(f))) {
		next:;
		*line = '\0';
		fgets(line, sizeof(line), f);
		s = line;
		if (*s != '\0') {
			s[strlen(s)-1] = '\0';
			/* "parse" multi-file */
			if ((multi) && (*s != ' ') && (*s != '\t')) {
				parent = multi_alloc(s, -2, &multi_enums);
				enum_num++;
				continue;
			}
			if (multi) {
				while((*s == ' ') || (*s == '\t'))
					s++;
			}
			
			keys++;
			/* don't add something twice */
			for(n = 0; n < data_num; n++) {
				if (strcmp(data[n], s) == 0) {
					if (multi) /* for multi-enums we want duplicate items to show up in per-enum lists */
						multi_alloc(s, n, &(parent->children));
					goto next;
				}
			}
	
			/* grow and insert */
			if (data_num >= allocated) {
				allocated += 32;
				data = realloc(data, sizeof(char *) * allocated);
				if (aux_fields != NULL)
					aux_data = realloc(aux_data, sizeof(char *) * allocated);
			}

			/* add in central array for hasing and in multi-enum */
			data[data_num] = sphash_strdup(s);
			if (aux_fields != NULL) {
				char *end = strchr(data[data_num], '\t');
				if (end != NULL) {
					*end = '\0';
					end++;
					while(*end == '\t') end++;
					aux_data[data_num] = end;
				}
				else
					aux_data[data_num] = NULL;
			}
			if (multi)
					multi_alloc(s, data_num, &(parent->children));
			data_num++;
		}
	}
	if (multi)
		lprintf(1, "[load_strings] Read %d unique (%d total) keys for %d enums.\n", data_num, keys, enum_num);
	else
		lprintf(1, "[load_strings] Read %d unique (%d total) keys.\n", data_num, keys);
	fclose(f);
	return 0;
}

unsigned int sphash(char *str)
{
	unsigned int n, a;
	char *s;

	a = 0;
	last = 0;
	
	for(n = 0, s = str; (*s != '\0') && (n<SPHASH_SEED1) ; n++,s++);
	
	for(; (*s != '\0') && (n<SPHASH_SEED0) ; n++,s++) {
		sphash_function(a, *s, n);
	}
	return a;
}

/* Returns the number of matching strings */
int hash_degree()
{
	unsigned int n, val, wrong;

	wrong = 0;

/*	for(n=0; n<hash_size; n++) {
		values[n] = -1;
		got[n] = 0;
	}*/
	memset(got, 0, sizeof(int) * hash_size);

	for(n=0; n<data_num; n++) {
		val = sphash(data[n]);
		lprintf(4, " '%s' -> %d %d\n", data[n], val, val % hash_size);
		val = val % hash_size;
		values[n] = val;
		if (got[val] != 0)
			wrong++;
		else
			got[val]++;
		if (wrong > 4)
			break;
	}

	return wrong;
}

int optimize_hash_function()
{
	int degree, best, len, v;

	/* Some sort of GP should be here instead of this (and more SEEDS): */
best = 10000;
for(SPHASH_SEED0 = 4; SPHASH_SEED0 < 64; SPHASH_SEED0+=2) {
	v = SPHASH_SEED0 - shortest - 1;
	if (v < 0)
		v = 0;
	for(SPHASH_SEED1 = SPHASH_SEED0 - 1; SPHASH_SEED1 >= v; SPHASH_SEED1--) {
		for(SPHASH_SEED2 = 0; SPHASH_SEED2 < 8; SPHASH_SEED2++) {
			degree = hash_degree();
			lprintf(3, "%d %d %d hash_size=%d => %d\n", SPHASH_SEED0, SPHASH_SEED1, SPHASH_SEED2, hash_size, degree);
			if (degree < best)
				best = degree;
			if (degree == 0) {
				len = SPHASH_SEED0 - SPHASH_SEED1;
				if (len < shortest) {
					shortest = len;
					SPHASH_SEED0_best = SPHASH_SEED0;
					SPHASH_SEED1_best = SPHASH_SEED1;
					SPHASH_SEED2_best = SPHASH_SEED2;
				}
			}
		}
	}
}

/*	if (best == 5)
		best=12345;*/

	return best;
}

#define minmax(v, min, max) \
	do { \
		if (v < min) \
			min = v; \
		if (v > max) \
			max = v; \
	} while(0)

void dump_file(char *file_name_, char *prefix)
{
	FILE *f;
	int n,m;
	int l = strlen(file_name_);
	multi_t *en, *item;
	char *s, *file_name = malloc(l + 4), *file_name__;
	memcpy(file_name, file_name_, l);
	file_name[l+0] = '.';
	file_name[l+1] = 'c';
	file_name[l+2] = '\0';

	f = fopen(file_name, "w");
	assert(f != NULL);
	fprintf(f, banner);
	fprintf(f, "#include <stdlib.h>\n");
	fprintf(f, "#include <string.h>\n");
	if (aux_fields != NULL) {
		file_name[l+1] = 'h';
		fprintf(f, "#include \"%s\"\n", file_name);
		file_name[l+1] = 'c';
	}
	fprintf(f, "\n");

	fprintf(f, "#define SPHASH_SEED0 %d\n", SPHASH_SEED0);
	fprintf(f, "#define SPHASH_SEED1 %d\n", SPHASH_SEED1);
	fprintf(f, "#define SPHASH_SEED2 %d\n", SPHASH_SEED2);
	fprintf(f, "%s\n\n", sphash_function_str);

	fprintf(f, "char *sphash_%s_strings[] = {\n", prefix);
	for(n=0; n<hash_size; n++) {
		for(m=0; m<hash_size; m++) {
			if ((m < data_num) && (values[m] == n)) {
				fprintf(f, "	\"%s\", /* %d */\n", data[m], m);
				goto ok;
			}
		}
		fprintf(f, "	\"\",\n");
		ok:;
	}
	fprintf(f, "	NULL\n};\n\n");

	fprintf(f, "int sphash_%s_nums[] = {\n", prefix);
	for(n=0; n<hash_size; n++) {
		for(m=0; m<hash_size; m++) {
			if ((m < data_num) &&(values[m] == n)) {
				fprintf(f, "	%d,\n", m);
				goto ok2;
			}
		}
		fprintf(f, "	-1, \n");
		ok2:;
	}
	fprintf(f, "	-1\n};\n\n");

	fprintf(f, "int %s_sphash(const char *str)\n", prefix);
	fprintf(f, "{\n");
	fprintf(f, "	unsigned int n, a;\n");
	fprintf(f, "	const char *s;\n");
	fprintf(f, "\n");
	fprintf(f, "	for(n = 0, s = str; (*s != '\\0') && (n<SPHASH_SEED1) ; n++,s++);\n");
	fprintf(f, "	a = 0;\n");
	fprintf(f, "	for(; (*s != '\\0') && (n<SPHASH_SEED0) ; n++,s++) {\n");
	fprintf(f, "		sphash_function(a, *s, n);\n");
	fprintf(f, "	}\n");
	fprintf(f, "	a = a %% %d;\n", hash_size);
	fprintf(f, "	if (strcmp(sphash_%s_strings[a], str) == 0)\n", prefix);
	fprintf(f, "		return sphash_%s_nums[a];\n", prefix);
	fprintf(f, "	return -1;\n");
	fprintf(f, "}\n\n");


	fprintf(f, "static int keynames[] = {\n");
	for(m=0; m<data_num-1; m++) {
		fprintf(f, "	% -5d, /* %s */\n", values[m], data[m]);
	}
	fprintf(f, "	% -5d  /* %s */\n};\n", values[m], data[m]);

	if (aux_fields != NULL) {
		fprintf(f, "\n%s_aux_t %s_aux[] = {\n", prefix, prefix);
		for(m=0; m<data_num-1; m++)
			fprintf(f, "	{%s},\n", aux_data[m]);
		fprintf(f, "	{%s}\n", aux_data[m]);
		fprintf(f, "};\n\n");
	}

	if (!multi) {
		if (aux_fields == NULL)
			fprintf(f, "typedef int %s_keys_t;\n", prefix);
		fprintf(f, "const char *%s_keyname(%s_keys_t keyid)\n", prefix, prefix);
	}
	else
		fprintf(f, "const char *%s_keyname(int keyid)\n", prefix);
	fprintf(f, "{\n");
	fprintf(f, "	if ((keyid < 0) || (keyid > %d))\n		return NULL;\n", data_num);
	fprintf(f, "	return sphash_%s_strings[keynames[keyid]];\n", prefix);
	fprintf(f, "}\n\n");

	if (multi) {
		for(en = multi_enums; en != NULL; en = en->next) {
			fprintf(f, "int %s_%s_isvalid(int keyid)\n", prefix, en->s);
			fprintf(f, "{\n");
			item = en->children;
			fprintf(f, "	return");
			/* We can't expect linear range because of all the reuses - let the compiler optimize */
			fprintf(f, " (keyid == %d)", item->value);
			for(item = item->next; item != NULL; item = item->next)
				fprintf(f, " || (keyid == %d)", item->value);
			fprintf(f, ";\n");
			fprintf(f, "}\n\n");
		}
	}

	fprintf(f, "#undef SPHASH_SEED0\n");
	fprintf(f, "#undef SPHASH_SEED1\n");
	fprintf(f, "#undef SPHASH_SEED2\n");
	fprintf(f, "#undef sphash_function\n");
	fclose(f);

	file_name[l+1] = 'h';
	f = fopen(file_name, "w");
	fprintf(f, banner);
	file_name__ = sphash_strdup(file_name_);
	for(s = file_name__; *s != '\0'; s++) {
		if (((*s >= 'a') && (*s <= 'z')) || ((*s >= 'A') && (*s <= 'Z')) || ((*s >= '0') && (*s <= '9')))
			continue;
		*s = '_';
	}
	fprintf(f, "#ifndef SPHASH_%s_H\n", file_name__);
	fprintf(f, "#define SPHASH_%s_H\n", file_name__);
	free(file_name__);
	fprintf(f, "int %s_sphash(const char *str);\n\n", prefix);
	fprintf(f, "/* total number of keys: */\n");
	fprintf(f, "#define %s_SPHASH_SIZE %d\n\n", prefix, data_num);

	if (multi) {
		fprintf(f, "/* universal invalid key: */\n");
		fprintf(f, "#define %s_SPHASH_INVALID (-1)\n\n", prefix);
		for(en = multi_enums; en != NULL; en = en->next) {
			unsigned int min = -1, max = 0, numitems = 0;
			fprintf(f, "typedef enum %s_%s_keys_e {\n", prefix, en->s);
			for(item = en->children; item != NULL; item = item->next) {
				fprintf(f, "	%s_%s_%s = %d,\n", prefix, en->s, item->s_id, item->value);
				minmax(item->value, min, max);
				numitems++;
			}
			fprintf(f, "	%s_%s_SPHASH_INVALID = -1\n", prefix, en->s);
			fprintf(f, "} %s_%s_keys_t;\n\n", prefix, en->s);
			fprintf(f, "typedef enum %s_%s_props_e {\n", prefix, en->s);
			fprintf(f, "	%s_%s_SPHASH_MINVAL = %d,\n", prefix, en->s, min);
			fprintf(f, "	%s_%s_SPHASH_MAXVAL = %d,\n", prefix, en->s, max);
			fprintf(f, "	%s_%s_SPHASH_RANGE = %d,\n", prefix, en->s, max-min+1);
			fprintf(f, "	%s_%s_SPHASH_NUMITEMS = %d\n", prefix, en->s, numitems);
			fprintf(f, "} %s_%s_props_t;\n", prefix, en->s);
			fprintf(f, "int %s_%s_isvalid(int keyid); /* returns 1 if keyid is in the above enum or 0 if not */\n", prefix, en->s);
			fprintf(f, "#define %s_%s_keyid2idx(keyid)  ((keyid)-%d)\n", prefix, en->s, min);
			fprintf(f, "#define %s_%s_idx2keyid(idx)    ((idx)+%d)\n\n\n", prefix, en->s, min);
		}
		fprintf(f, "\nconst char *%s_keyname(int keyid);\n", prefix);
	}
	else {
		unsigned int min = -1, max = 0, numitems = 0;
		fprintf(f, "/* integer values of string keys: */\n");
		fprintf(f, "typedef enum %s_keys_e {\n", prefix);
		for(n=0; n<hash_size; n++) {
			for(m=0; m<hash_size; m++) {
				if ((m < data_num) &&(values[m] == n)) {
					char *s;
					for(s = data[m]; *s != '\0'; s++)
						if (!isalnum(*s))
							*s = '_';

					fprintf(f, "	%s_%s = %d,\n", prefix, data[m], m);
					minmax(m, min, max);
					numitems++;
					break;
				}
			}
		}
		fprintf(f, "	%s_SPHASH_INVALID = -1\n", prefix);
		fprintf(f, "} %s_keys_t;\n\n", prefix);
		fprintf(f, "typedef enum %s_props_e {\n", prefix);
		fprintf(f, "	%s_SPHASH_MINVAL = %d,\n", prefix, min);
		fprintf(f, "	%s_SPHASH_MAXVAL = %d,\n", prefix, max);
		fprintf(f, "	%s_SPHASH_RANGE = %d,\n", prefix, max-min+1);
		fprintf(f, "	%s_SPHASH_NUMITEMS = %d\n", prefix, numitems);
		fprintf(f, "} %s_props_t;\n", prefix);
		fprintf(f, "\nconst char *%s_keyname(%s_keys_t keyid);\n", prefix, prefix);
		fprintf(f, "#define %s_keyid2idx(keyid)  ((keyid)-%d)\n", prefix, min);
		fprintf(f, "#define %s_idx2keyid(idx)    ((idx)+%d)\n", prefix, min);

	}

	if (aux_fields != NULL) {
		fprintf(f, "\ntypedef struct %s_aux_e {\n", prefix);
		fprintf(f, "%s\n", aux_fields);
		fprintf(f, "} %s_aux_t;\n", prefix);
		fprintf(f, "extern %s_aux_t %s_aux[];\n\n", prefix, prefix);
	}
	fprintf(f, "#endif\n");
	fclose(f);
}

int main(int argc, char *argv[])
{
	int best, n;
	FILE *f;
	char *fn = NULL, *cmd;
	char *prefix = "test";
	char *outfn  = "test_hash";
	char *end;

	n = 1;
	while(argv[n] != NULL) {
		cmd = argv[n];
		if (*cmd == '-') {
			while(*cmd == '-') cmd++;
			switch(*cmd) {
				case 'p': /* --prefix */
					n++;
					prefix = argv[n];
					break;
				case 'o': /* --out */
					n++;
					outfn = argv[n];
					break;
				case 'm': /* --multi */
					multi=1;
					break;
				case 'a': /* --aux */
					n++;
					aux_fields = argv[n];
					break;
				case 'v': /* --verbose */
					debug_level++;
					break;
				case 'O': /* --overrun */
					n++;
					overrun = strtol(argv[n], &end, 10);
					if (*end != '\0') {
						fprintf(stderr, "Expected integer for -O\n");
						return 1;
					}
			}
		}
		else {
			fn = argv[n];
		}
		n++;
	}

	if (fn != NULL) {
		f = fopen(fn, "r");
		if (f == NULL) {
			printf("Can't open file '%s'\n", fn);
			return -1;
		}
	}
	else
		f = stdin;

	if (load_strings(f) < 0)
		return 1;

	values = calloc(sizeof(int), data_num*overrun);
	got = calloc(sizeof(int), data_num*overrun);

	hash_size = 1;

	for(hash_size = data_num; hash_size < data_num * overrun; hash_size *= 1.25) {
		best = optimize_hash_function();
		lprintf(2, "Hash_size: %d best=%d\n", hash_size, best);
		if (best <= 4) {
			break;
		}
	}


	for(; hash_size < data_num * overrun; hash_size += 8) {
		best = optimize_hash_function();
		lprintf(2, "Hash_size: %d best=%d (shortest=%d)\n", hash_size, best, shortest);
		if (best == 0) {
			if (shortest < 40) {
				SPHASH_SEED0 = SPHASH_SEED0_best;
				SPHASH_SEED1 = SPHASH_SEED1_best;
				SPHASH_SEED2 = SPHASH_SEED2_best;
				hash_degree();
				lprintf(3, "Seeds: %d %d %d, size=%d/%d\n", SPHASH_SEED0, SPHASH_SEED1, SPHASH_SEED2, hash_size, data_num);
				dump_file(outfn, prefix);
				return 0;
			}
		}
	}

	return 1;
}
