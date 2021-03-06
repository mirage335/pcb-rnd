#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <puplug/config.h>
#include <puplug/os_dep.h>
#include <puplug/os_dep_fs.h>

static const char *generated = "This file is generated by the puplug utility. Please DO NOT edit this file.";

static void var_set(char const **vars, int idx, const char *key, const char *val)
{
	idx *= 2;
	vars[idx+0] = key;
	vars[idx+1] = val;
}

static void var_printf(const char *fmt, const char **vars)
{
	const char **v;
	while(*fmt != '\0') {
		switch(*fmt) {
			case '\\':
				fmt++;
				switch(*fmt) {
					case 'n': putchar('\n'); break;
					case 'r': putchar('\r'); break;
					case 't': putchar('\t'); break;
					case '\\': putchar('\\'); break;
				}
				break;
			case '%':
				fmt++;
				if (*fmt == '%') {
					putchar('%');
				}
				else if (*fmt == '$') {
					int wlen;
					char key[64];
					const char *end;

					end = strchr(fmt+1, '$');
					wlen = end - (fmt+1);
					if (wlen >= sizeof(key)) {
						fprintf(stderr, "Error: %%$key$ too long: %s\n", fmt);
						exit(1);
					}
					memcpy(key, fmt+1, wlen);
					key[wlen] = '\0';
					for(v = vars; *v != NULL; v += 2) {
						if (strcmp(v[0], key) == 0) {
							if (v[1] != NULL)
								printf("%s", v[1]);
							break;
						}
					}
					fmt = end;
				}
				else {
					for(v = vars; *v != NULL; v += 2) {
						if ((v[0][0] == '%') && (v[0][1] == *fmt)) {
							if (v[1] != NULL)
								printf("%s", v[1]);
							break;
						}
					}
				}
				break;
			default:
				putchar(*fmt);
		}
		fmt++;
	}
}

int var_seek_end(char const **vars)
{
	int n;
	for(n = 0; *vars != NULL; vars+=2, n++) ;
	return n;
}
static char *PUP_strdup(const char *s)
{
	char *res;
	size_t len = strlen(s);
	res = malloc(len+1);
	if (res == NULL)
		return NULL;
	memcpy(res, s, len+1);
	return res;
}


typedef enum {
	FLG_AUTOLOAD = 1,
	FLG_DEF_BUILDIN = 2,
	FLG_DEF_PLUGIN = 4,
	FLG_DEF_DISABLE = 8,
	FLG_DEF_DISABLE_ALL = 16
} flag_t;

typedef struct {
	char *s, buff[8192];
	int deps_alloced;
	int lineno;
	
	flag_t *flags;
	const char **vars;
	const char *dep_fmt;
	const char *conflict_fmt;
} parse_dep_t;

#include <puplug/libs_dep_parse.h>

static int pup_parse_dep(parse_dep_t *st, char *first, char *args)
{
	if (st->dep_fmt != NULL) {
		int end = var_seek_end(st->vars);
		var_set(st->vars, end+0, "%m", first);
		var_set(st->vars, end+1, NULL, NULL);
		var_printf(st->dep_fmt, st->vars);
		var_set(st->vars, end+0, NULL, NULL);
	}
	return 0;
}

static int pup_parse_conflict(parse_dep_t *st, char *first, char *args)
{
	if (st->conflict_fmt != NULL) {
		int end = var_seek_end(st->vars);
		var_set(st->vars, end+0, "%m", first);
		var_set(st->vars, end+1, NULL, NULL);
		var_printf(st->conflict_fmt, st->vars);
		var_set(st->vars, end+0, NULL, NULL);
	}
	return 0;
}

static int pup_parse_tag(parse_dep_t *st, char *tag, char *args)
{
	if (st->vars != NULL) {
		int end = var_seek_end(st->vars);
		var_set(st->vars, end+0, PUP_strdup(tag), PUP_strdup(args));
		var_set(st->vars, end+1, NULL, NULL);
	}
	return 0;
}


static int pup_parse_autoload(parse_dep_t *st, char *first, char *args)
{
	(*st->flags) |= FLG_AUTOLOAD;
	return 0;
}

static int pup_parse_default(parse_dep_t *st, char *first, char *args)
{
	if (strcmp(first, "buildin") == 0)
		(*st->flags) |= FLG_DEF_BUILDIN;
	else if (strcmp(first, "plugin") == 0)
		(*st->flags) |= FLG_DEF_PLUGIN;
	else if (strcmp(first, "disable") == 0)
		(*st->flags) |= FLG_DEF_DISABLE;
	else if (strcmp(first, "disable-all") == 0)
		(*st->flags) |= FLG_DEF_DISABLE_ALL;
	return 0;
}

static int pup_parse_syntax_error(parse_dep_t *st, const char *msg)
{
	fprintf(stderr, "Syntax error: %s\n", msg);
	return -1;
}

static char *load_pup(const char *pn, int strfilter, flag_t *flags, const char **vars, const char *dep_fmt, const char *conflict_fmt)
{
	char fn[8192];
	FILE *f;
	int c, res = 0;
	parse_dep_t st;
	char *buff = NULL;
	int used = 0, alloced = 0, ignore = 0, first = 1, sepd = 1;

	sprintf(fn, "%s.pup", pn);

	/* try to open the dep file of the library */
	f = fopen(fn, "r");
	if (f == NULL) {
		fprintf(stderr, "Error: can't read '%s' for plugin %s\n", fn, pn);
		exit(1);
	}

	pup_parse_pup_init(&st);
	st.flags = flags;
	st.vars = vars;
	st.dep_fmt = dep_fmt;
	st.conflict_fmt = conflict_fmt;

	*flags = 0;

	/* Read the file char by char seeking words */
	do {
		c = fgetc(f);
		res = pup_parse_pup(&st, c);

		if (used+2 >= alloced) {
			alloced += 1024;
			buff = realloc(buff, alloced+1);
		}
		if (strfilter && first) {
			switch(c) {
				case '#':
				case '$':
					ignore = 1;
					break;
			}
		}
		switch(c) {
			case EOF: break;
			case '\n':
			case '\r':
			case ';':
				if (!sepd) {
					buff[used++] = ';';
					sepd = 1;
				}
				ignore = 0;
				first = 1;
				break;
			case '\"':
			case '\\':
				if (strfilter && !ignore)
					buff[used++] = '\\';
				/* fall thru */
			default:
				if (!ignore) {
					buff[used++] = c;
					if (!isspace(c))
						sepd = 0;
				}
				if (!isspace(c))
					first = 0;
		}
	} while((res == 0) && (c != EOF));

	fclose(f);
	if (res != 0) {
		free(buff);
		return NULL;
	}
	buff[used] = '\0';
	return buff;
}

static int buildin_c(char *list)
{
	char *next, *curr;
	int n, len = 0;

	while((*list == '\r') || (*list == '\n') || (*list == ';')) list++;

	printf("/* %s */\n\n", generated);
	printf("#include <stdlib.h>\n");
	printf("#include <puplug/libs.h>\n\n");
	for(curr = list; curr != NULL; curr = next) {
		char *pn, *fn;
		next = strpbrk(curr, "\r\n;");
		if (next != NULL) {
			*next = '\0';
			next++;
			while((*next == '\r') || (*next == '\n') || (*next == ';')) next++;
		}
		if ((*curr == '#') || (*curr == '\0')) {
			len++;
			continue;
		}
		pn = curr;
		fn = strchr(pn, '=');
		if (fn == NULL) {
			fprintf(stderr, "Syntax error in package list near %s\n", pn);
			return 1;
		}
		*fn = '\0';
		fn++;
		len++;
		printf("extern int pplg_init_%s(void);\n", pn);
		printf("extern void pplg_uninit_%s(void);\n", pn);
		printf("extern int pplg_check_ver_%s(int);\n", pn);
	}

	printf("\nconst pup_buildin_t pup_buildins[] = {\n");
	for(n = 0, curr = list; n < len; n++, curr = next) {
		char *pn, *fn, *deps, *end;
		char path[PUP_PATH_MAX];
		flag_t flags;
		pn = curr;
		fn = pn + strlen(pn) + 1;
		if (*pn != '#')
			next = fn + strlen(fn) + 1;
		else
			next = fn;
		while((*next == '\r') || (*next == '\n') || (*next == ';')) next++;
		if ((*pn == '\0') || (*pn == '#'))
			continue;

		strcpy(path, fn);
		end = strrchr(path, '.');
		if ((end != NULL) && ((strcmp(end, ".c") == 0) || (strcmp(end, ".pup") == 0) || (strcmp(end, ".PUP") == 0)))
			*end = '\0';
		deps = load_pup(path, 1, &flags, NULL, NULL, NULL);
		printf("\t{\"%s\", pplg_init_%s, pplg_uninit_%s, pplg_check_ver_%s, %d, \"%s\"},\n", pn, pn, pn, pn, !!(flags & FLG_AUTOLOAD) , (deps == NULL ? "" : deps));
		free(deps);
	}

	printf("\t{NULL, NULL, NULL, NULL, 0, NULL}\n");
	printf("};\n");

	return 0;
}

int main_buildin_c(int argc, char *argv[])
{
	int alloced = 0, used = 0, len, res;
	char *list = NULL;
	char buff[1024];

	if (argv[1] == NULL) {
		fprintf(stderr, "Error: need a list of plugins or '-' to read stdin\n");
		exit(1);
	}
	if (strcmp(argv[1], "-") != 0)
		return buildin_c(argv[1]);


	while((len = fread(buff, 1, sizeof(buff), stdin)) > 0) {
		if (used + len + 1 >= alloced) {
			alloced = used + len + 4096;
			list = realloc(list, alloced);
		}
		memcpy(list + used, buff, len);
		used += len;
	}
	list[used] = '\0';
	res = buildin_c(list);
	free(list);
	return res;
}

int main_buildin_h(int argc, char *argv[])
{
	printf("/* %s */\n\n", generated);
	printf("#include <puplug/libs.h>\n");
	printf("extern const pup_buildin_t pup_buildins[];\n");
	return 0;
}

int main_objlist(int argc, char *argv[])
{
	char *next, *curr;

	for(curr = argv[1]; curr != NULL; curr = next) {
		char *pn, *fn;
		char *end;
		next = strchr(curr, ';');
		if (next != NULL) {
			*next = '\0';
			next++;
		}
		pn = curr;
		fn = strchr(pn, '=');
		if (fn == NULL) {
			fprintf(stderr, "Syntax error in package list near %s\n", pn);
			return 1;
		}
		*fn = '\0';
		fn++;
		end = strrchr(fn, '.');
		if ((end == NULL) || (strcmp(end, ".c") != 0))
			continue;
		end[1] = 'o';
		printf("%s\n", fn);
	}
	return 0;
}


static void findpups(const char *dirname, int rootlen, int noisy, const char *found_fmt, const char *dep_fmt, const char *conflict_fmt)
{
	const char *name;
	char sub[PUP_PATH_MAX], *subend;
	void *dir;


	dir = pup_open_dir(dirname);
	if (dir == NULL) {
		if (noisy)
			fprintf(stderr, "Directory %s not found.\n", dirname);
		return;
	}

	strcpy(sub, dirname);
	subend = sub + strlen(sub);
	*subend = '/';
	subend++;
	while((name = pup_read_dir(dir)) != NULL) {
		char *end;
		if (name[0] == '.')
			continue;
		end = strrchr(name, '.');
		strcpy(subend, name);
		if ((end != NULL) && ((strcmp(end, ".pup") == 0) || (strcmp(end, ".PUP") == 0))) {
			char plugin_name[PUP_PATH_MAX], *tmp;
			const char *vars[2*32];
			flag_t flags;
			int n, vstart, vend;
			const char *s3, *sall;

			/* render the pure name (basename, no extension) */
			strcpy(plugin_name, name);
			plugin_name[end - name] = '\0';

			var_set(vars, 0, "%n", name);               /* basename */
			var_set(vars, 1, "%D", dirname+rootlen+1);  /* dirname/relative */
			var_set(vars, 2, "%d", dirname);            /* dirname/absolute*/
			var_set(vars, 3, "%p", sub);                /* path/full */
			var_set(vars, 3, "%N", plugin_name);        /* basename without extension */
			var_set(vars, 4, NULL, NULL);

			vstart = 4;
			end = strrchr(sub, '.');
			*end = '\0';
			tmp = load_pup(sub, 0, &flags, vars, dep_fmt, conflict_fmt);
			*end = '.';
			vend = var_seek_end(vars);

			s3 = "";
			sall = "1";
			if (flags & FLG_DEF_BUILDIN)          { s3 = "sbuildin"; sall = "1"; }
			else if (flags & FLG_DEF_PLUGIN)      { s3 = "splugin"; sall = "1";  }
			else if (flags & FLG_DEF_DISABLE)     { s3 = "sdisable"; sall = "1"; }
			else if (flags & FLG_DEF_DISABLE_ALL) { s3 = "sdisable"; sall = "0"; }


			var_set(vars, vend+0, "%a", (flags & FLG_AUTOLOAD ? "1" : "0"));
			var_set(vars, vend+1, "%3", s3);
			var_set(vars, vend+2, "%A", sall);
			var_set(vars, vend+3, NULL, NULL);

			var_printf(found_fmt, vars);
			for(n = vstart*2; n < vend*2; n++)
				free((char *)vars[n]);
			free(tmp);
		}
		else
			findpups(sub, rootlen, 0, found_fmt, dep_fmt, conflict_fmt);
	}
	pup_close_dir(dir);
}

int main_findpups(int argc, char *argv[])
{
	char *found_fmt, *dep_fmt = NULL, *conflict_fmt = NULL, *root = argv[1];

	if (root == NULL) {
		fprintf(stderr, "No directory.\n");
		return -1;
	}

	found_fmt = argv[2];
	if (found_fmt == NULL)
		found_fmt = "%p\n";
	else
		dep_fmt = argv[3];
	if (dep_fmt != NULL)
		conflict_fmt = argv[4];

	findpups(root, strlen(root), 1, found_fmt, dep_fmt, conflict_fmt);

	return 0;
}

int main(int argc, char *argv[])
{
	const char *cmd = argv[1];
	argc--;
	argv++;

	if (cmd == NULL) {
		fprintf(stderr, "Error: need a command name; try %s help\n", argv[0]);
		return 1;
	}

	if (strcmp(cmd, "buildin.c") == 0) return main_buildin_c(argc, argv);
	if (strcmp(cmd, "buildin.h") == 0) return main_buildin_h(argc, argv);
	if (strcmp(cmd, "objlist") == 0) return main_objlist(argc, argv);
	if (strcmp(cmd, "findpups") == 0) return main_findpups(argc, argv);


	fprintf(stderr, "Error: unknown command %s; try %s help\n", cmd, argv[0]);
	return 1;
}

