/*
    fungw - language-agnostic function gateway
    Copyright (C) 2018, 2019  Tibor 'Igor2' Palinkas

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

    Project page: http://repo.hu/projects/fungw
    Version control: svn://repo.hu/fungw/trunk
*/

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <libfungw/fungw.h>
#include <libfungw/fungw_conv.h>

#include "os_dep.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/signal.h>
#include <fcntl.h>
#include <errno.h>

typedef struct {
	const char *name;
	const char *shebang, *execflag;
	const char *head, *foot;
} fgws_cli_lang_t;

#include "sh_head.h"
static const char sh_foot[] = "\nfgw_main_loop\n";

static const fgws_cli_lang_t langs[] = {
	{"shell", "/bin/sh",          "-c",    sh_head_arr,    sh_foot},
	{"bash",  "/bin/bash",        "-c",    sh_head_arr,    sh_foot},
	{"dash",  "/bin/dash",        "-c",    sh_head_arr,    sh_foot},
	{"shell", "/usr/bin/sh",      "-c",    sh_head_arr,    sh_foot},
	{"bash",  "/usr/bin/bash",    "-c",    sh_head_arr,    sh_foot},
	{"dash",  "/usr/bin/dash",    "-c",    sh_head_arr,    sh_foot},
	{NULL,    NULL,               NULL,    NULL,           NULL}
};

typedef struct {
	fgw_obj_t *obj;
	int fd_c2s[2]; /* c->script pipe; c writes [1], script reads [0] */
	int fd_s2c[2]; /* c->script pipe; c reads [0], script writes [1] */
	pid_t pid;
	int running; /* whether the background process is running */

	/* read buffer */
	char buf[256];
	int buf_fill, bufp;

	/* name of the script being executed */
	char scr_name[L_tmpnam];
} cli_ctx_t;

static fgw_error_t fgws_cli_call_script(fgw_arg_t *res, int argc, fgw_arg_t *argv);

/* buffered read on the control pipe */
static int cli_getc(cli_ctx_t *ctx)
{
	if (ctx->bufp >= ctx->buf_fill) {
		ctx->buf_fill = read(ctx->fd_s2c[0], ctx->buf, sizeof(ctx->buf));
		if (ctx->buf_fill <= 0)
			return ctx->buf_fill;
		ctx->bufp = 0;
	}
	return ctx->buf[ctx->bufp++];
}

/* blocking write all buf */
static ssize_t cli_write_(int fd, const char *buf, ssize_t len)
{
	ssize_t orig = len;
	while(len > 0) {
		ssize_t l;
		l = write(fd, buf, len);
		if (l <= 0)
			return -1;
		buf += l;
		len -= l;
	}
	return orig;
}

static ssize_t cli_write(cli_ctx_t *ctx, const char *buf, ssize_t len)
{
	return cli_write_(ctx->fd_c2s[1], buf, len);
}


/* print small (<1k) formatted strings */
static void cli_printf(cli_ctx_t *ctx, const char *fmt, ...)
{
	char tmp[1024];
	size_t len;
	va_list ap;
	va_start(ap, fmt);
	len = vsprintf(tmp, fmt, ap);
	va_end(ap);
	cli_write(ctx, tmp, len);
}

static int cli_got_eof(cli_ctx_t *ctx, ssize_t len, int err_no)
{
/* later, for nonblocking:
	if ((len == 0) && (err_no == EAGAIN)) {
		usleep?
		return 0;
	}
*/

	if (len <= 0) {
		ctx->running = 0;
		return 1;
	}
	return 0;
}

static int cli_read_fgw_func_reg(cli_ctx_t *ctx, char *buf, int maxlen)
{
	int bl = 0;
	fgw_func_t *func;

	for(;;) {
		int c;

		/* read the function name */
		c = cli_getc(ctx);
		if (cli_got_eof(ctx, c, errno))
			return -1;
		if ((c == '\n') || (c == '\r'))
			break;
		buf[bl] = c;
		bl++;
		if (bl >= maxlen)
			return -1;
	}
	buf[bl] = '\0';

	func = fgw_func_reg(ctx->obj, buf, fgws_cli_call_script);
	if (func == NULL) {
		fgw_async_error(ctx->obj, "fgw_func_reg: failed to register function\n");
		fgw_async_error(ctx->obj, buf);
		fgw_async_error(ctx->obj, "\n");
		cli_write(ctx, "fr_err\n", 7);
	}
	cli_write(ctx, "fr_ok\n", 6);
	return 0;
}

static int cli_read_fgw_arg(cli_ctx_t *ctx, fgw_arg_t *arg)
{
	int used = 0, alloced = 256, grow = 2048, limit=1024*1024;

	arg->type = FGW_STR | FGW_DYN;
	arg->val.str = malloc(alloced);

	for(;;) {
		int c;

		/* read the function name */
		c = cli_getc(ctx);
		if (cli_got_eof(ctx, c, errno)) {
			free(arg->val.str);
			arg->type = 0;
			return -1;
		}
		if ((c == '\n') || (c == '\r'))
			break;
		if (used >= alloced) {
			alloced += grow;
			if (alloced > limit) {
				free(arg->val.str);
				arg->type = 0;
				return -1;
			}
			arg->val.str = realloc(arg->val.str, alloced+1);
		}
		arg->val.str[used] = c;
		used++;
	}
	arg->val.str[used] = '\0';
	return 0;
}

static int cli_read_fgw_int(cli_ctx_t *ctx, int *dst)
{
	long tmp = 0;
	int valid = 0;

	for(;;) {
		int c;

		/* read the function name */
		c = cli_getc(ctx);
		if (cli_got_eof(ctx, c, errno))
			return -1;
		if ((c == '\n') || (c == '\r'))
			break;
		if (isdigit(c)) {
			tmp *= 10;
			tmp += c - '0';
			valid = 1;
		}
		else {
			valid = 0;
			break;
		}
	}
	*dst = tmp;
	return !valid;
}

#define cli_wait_ok_free_arg() \
do { \
	int n; \
	if (argv != NULL) { \
		for (n = 1; n < argc; n++) {\
			if (argv[n].type == (FGW_STR | FGW_DYN)) \
				free(argv[n].val.str); \
			argv[n].type = 0; \
		} \
		free(argv); \
		argv = NULL; \
	} \
	argp = argc = -1; \
	argv_alloced = 0; \
} while(0)

static int cli_wait_ok(cli_ctx_t *ctx, fgw_arg_t *ret)
{
	char buf[1024];
	int bl = 0;
	int argp = -1, argc = -1;
	int argv_alloced = 0;
	fgw_arg_t *argv = NULL;

	for(;;) {
		int c;

		/* read the command */
		c = cli_getc(ctx);
		if (cli_got_eof(ctx, c, errno))
			return -1;
		if (isspace(c)) {
			buf[bl] = '\0';
			if (buf[0] == '\0')
				continue;
			if ((buf[0] == 'o') && (buf[1] == 'k')) {
				if (ret != NULL) {
					fprintf(stderr, "cli_wait_ok(): got 'ok' while waiting for a function return\n");
					goto err;
				}
				goto done;
			}
			else if (strcmp(buf, "call_begin") == 0) {
				if (cli_read_fgw_int(ctx, &argc) != 0) {
					fprintf(stderr, "cli_wait_ok(): invalid call_beign argc: %s\n", buf+11);
					goto err;
				}
				if (argc > 1024) {
					fprintf(stderr, "cli_wait_ok(): argc too high: %s\n", buf+11);
					goto err;
				}
				if (argc > argv_alloced) {
					argv_alloced = argc;
					argv = realloc(argv, argv_alloced * sizeof(fgw_arg_t));
				}
				argp = 1;
				bl = 0;
			}
			else if (strcmp(buf, "call_arg") == 0) {
				if (argc < 0) {
					fprintf(stderr, "cli_wait_ok(): invalid call_arg without call_begin\n");
					goto err;
				}
				if (argp >= argc) {
					fprintf(stderr, "cli_wait_ok(): invalid call_arg: too many arguments\n");
					goto err;
				}
				cli_read_fgw_arg(ctx, &argv[argp]);
				argp++;
				bl = 0;
			}
			else if (strcmp(buf, "call_end") == 0) {
				fgw_arg_t fn, res;
				fgw_func_t *func;
				fgw_error_t ferr;

				if (argp != argc) {
					fprintf(stderr, "cli_wait_ok(): invalid call_end: number of args mismatch: %d != %d\n", argp, argc);
					goto err;
				}
				cli_read_fgw_arg(ctx, &fn);
				func = fgw_func_lookup(ctx->obj->parent, fn.val.str);
				if (func == NULL) {
					fprintf(stderr, "cli_wait_ok(): invalid call_end: unknown function '%s'\n", fn.val.str);
					free(fn.val.str);
					goto err;
				}
				free(fn.val.str);
				argv[0].type = FGW_FUNC;
				argv[0].val.argv0.func = func;
				argv[0].val.argv0.user_call_ctx = ctx->obj->script_user_call_ctx;

				res.type = FGW_PTR;
				res.val.ptr_void = NULL;
				ferr = func->func(&res, argc, argv);
				fgw_argv_free(ctx->obj->parent, argc, argv);
				cli_wait_ok_free_arg();
				if ((ferr == 0) && (fgw_arg_conv(ctx->obj->parent, &res, FGW_STR | FGW_DYN) == 0)) {
					cli_write(ctx, "retok ", 6);
					cli_write(ctx, res.val.str, strlen(res.val.str));
					cli_write(ctx, "\n", 1);
				}
				else
					cli_write(ctx, "retfail\n", 8);
				if (res.type == (FGW_STR | FGW_DYN))
					free(res.val.str);
				bl = 0;
			}
			else if (strcmp(buf, "retok") == 0) {
				if (ret == NULL) {
					fprintf(stderr, "cli_wait_ok(): got 'retok' while NOT waiting for a function return\n");
					goto err;
				}
				if (cli_read_fgw_arg(ctx, ret) < 0)
					goto err;
				goto done;
			}
			else if (strcmp(buf, "retfail") == 0) {
				if (ret == NULL) {
					fprintf(stderr, "cli_wait_ok(): got 'retfail' while NOT waiting for a function return\n");
					goto err;
				}
				goto ret1;
			}
			else if (strcmp(buf, "fgw_func_reg") == 0) {
				if (cli_read_fgw_func_reg(ctx, buf, sizeof(buf)) < 0)
					goto err;
				bl = 0;
			}
			else {
				fprintf(stderr, "cli_wait_ok(): Invalid request from the script: '%s'\n", buf);
				goto err;
			}
		}
		else {
			buf[bl] = c;
			bl++;
			if (bl >= sizeof(buf))
				goto err;
		}
	}

	done:;
	cli_wait_ok_free_arg();
	return 0;

	ret1:;
	cli_wait_ok_free_arg();
	return 1;

	err:;
	cli_wait_ok_free_arg();
	return -1;
}

/* API: register an fgw function in the script, make the function visible/callable */
static void fgws_cli_reg_func(fgw_obj_t *obj, const char *name, fgw_func_t *f)
{
	/* Do not register functions in the script, in script->c direction fgw calls
	   are wrapped due to syntax limitations. */
}

/* API: fgw calls a cli function */
static fgw_error_t fgws_cli_call_script(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	fgw_obj_t *obj = argv[0].val.func->obj;
	cli_ctx_t *ctx = obj->script_data;
	int n, rv;

	res->type = FGW_PTR;
	res->val.ptr_void = NULL;

	fgws_ucc_save(obj);
	cli_printf(ctx, "call_begin %d\n", argc-1);
	for(n = 1; n < argc; n++) {
		cli_write(ctx, "call_arg ", 9);
		fgw_arg_conv(obj->parent, &argv[n], FGW_STR | FGW_DYN);
#warning TODO: protect against \n
		cli_write(ctx, argv[n].val.str, strlen(argv[n].val.str));
		cli_write(ctx, "\n", 1);
	}
	cli_printf(ctx, "call_end %s\n", argv[0].val.func->name);
	rv = cli_wait_ok(ctx, res);
	fgws_ucc_restore(obj);
	for (n = 1; n < argc; n++) {
		if (argv[n].type == (FGW_STR | FGW_DYN)) {
			argv[n].type = 0;
			free(argv[n].val.str);
		}
	}
	if (rv == 0)
		return FGW_SUCCESS;
	if (res->type == (FGW_STR | FGW_DYN)) {
		res->type = 0;
		free(res->val.str);
	}
	return FGW_ERR_UNKNOWN;
}

/* API: unload the script */
static int fgws_cli_unload(fgw_obj_t *obj)
{
	cli_ctx_t *ctx = obj->script_data;
	if ((ctx->running) && (ctx->pid > 1))
		kill(ctx->pid, SIGTERM);

	if (*ctx->scr_name != '\0')
		remove(ctx->scr_name);

	close(ctx->fd_c2s[0]);
	close(ctx->fd_c2s[1]);
	close(ctx->fd_s2c[0]);
	close(ctx->fd_s2c[1]);

	free(ctx);

	return 0;
}

/* API: init the interpreter so that functions can be registered */
static int fgws_cli_init(fgw_obj_t *obj, const char *filename, const char *opts)
{
	cli_ctx_t *ctx = malloc(sizeof(cli_ctx_t));

	obj->script_data = ctx;
	ctx->obj = obj;

	ctx->pid = -1;
	ctx->buf_fill = ctx->bufp = 0;
	*ctx->scr_name = '\0';

	if (pipe(ctx->fd_c2s) != 0) {
		free(ctx);
		return -1;
	}
	if (pipe(ctx->fd_s2c) != 0) {
		close(ctx->fd_c2s[0]);
		close(ctx->fd_c2s[1]);
		free(ctx);
		return -1;
	}
	return 0;
}

/* API: load a script into an object  */
static int fgws_cli_load(fgw_obj_t *obj, const char *filename, const char *opts)
{
	cli_ctx_t *ctx = obj->script_data;
	char shebang[2048], *shb, buf[1024];
	FILE *f;
	const fgws_cli_lang_t *l;
	int fd;

	/* figure language from shebang */
	f = fopen(filename, "r");
	if (f == NULL) {
		fprintf(stderr, "fgws_cli_load: can't open '%s' for read\n", filename);
		fclose(f);
		return -1;
	}
	shb = fgets(shebang, sizeof(shebang)-1, f);
	if (shb == NULL) {
		fprintf(stderr, "fgws_cli_load: unable to load shebang from %s\n", filename);
		fclose(f);
		return -1;
	}
	if ((shb[0] != '#') && (shb[0] != '!')) {
		fprintf(stderr, "fgws_cli_load: invalid shebang prefix in %s\n", filename);
		fclose(f);
		return -1;
	}
	shb+=2;
	while(isspace(*shb)) shb++;

	for(l = langs; l->name != NULL; l++)
		if (strncmp(l->shebang, shb, strlen(l->shebang)) == 0)
			break;

	if (l->name == NULL) {
		fprintf(stderr, "fgws_cli_load: unrecognized shebang in %s: '%s'\n", filename, shb);
		fclose(f);
		return -1;
	}

	/* write the temp file with header, script and footer */
	/* NOTE: tmpnam is the only portable way to do this; O_EXCL below should
	   guarantee it's safe. There's a worse, unavoidable race when we later
	   exec the interpreter on the file, by name. */
	if (tmpnam(ctx->scr_name) == NULL) {
		fprintf(stderr, "fgws_cli_load: failed to create temp file\n");
		fclose(f);
		return -1;
	}

	fd = open(ctx->scr_name, O_WRONLY | O_EXCL | O_CREAT, 0600);
	if (fd < 0) {
		fprintf(stderr, "fgws_cli_load: failed to create temp file '%s'\n", ctx->scr_name);
		fclose(f);
		return -1;
	}
	cli_write_(fd, l->head, strlen(l->head));
	while(!feof(f)) {
		int len = fread(buf, 1, sizeof(buf), f);
		if (len > 0)
			cli_write_(fd, buf, len);
	}
	cli_write_(fd, l->foot, strlen(l->foot));
	close(fd);
	fclose(f);

	/* fork and execute the interpreter */
	ctx->pid = fork();
	if (ctx->pid == 0) {
		int fd;
		/* child */
		close(ctx->fd_c2s[1]);
		close(ctx->fd_s2c[0]);

		if (ctx->fd_c2s[0] != 3) {
			close(3);
			fd = dup2(ctx->fd_c2s[0], 3);
			if (fd != 3) {
				fprintf(stderr, "Can't dup2 script input on fd 3: %d %s\n", fd, strerror(errno));
				exit(1);
			}
		}
		if (ctx->fd_s2c[1] != 4) {
			close(4);
			fd = dup2(ctx->fd_s2c[1], 4);
			if (fd != 4) {
				fprintf(stderr, "Can't dup2 script input on fd 4: %d\n", fd);
				exit(1);
			}
		}

		for(fd = 5; fd < 1024; fd++)
			close(fd);

		execl(l->shebang, l->execflag, ctx->scr_name, NULL);
		exit(1);
	}
	ctx->running = 1;
	close(ctx->fd_c2s[0]);
	close(ctx->fd_s2c[1]);

	cli_wait_ok(ctx, NULL);

	return 0;
}

static int fgws_cli_test_parse(const char *filename, FILE *f)
{
	const char *exts[] = {".sh", ".bash", NULL };
	const char *shebang[] = {"sh", "bash", "dash", NULL };

	if (f != NULL) {
		char line[128], *s;
		const char **sh;

		s = fgets(line, sizeof(line), f);
		if ((s != NULL) && (s[0] == '#') && (s[1] == '!')) {
			s += 2;
			while(isspace(*s)) s++;
			if (strncmp(s, "/usr", 4) == 0) s += 4;
			if (strncmp(s, "/opt", 4) == 0) s += 4;
			if (strncmp(s, "/local", 6) == 0) s += 6;
			if (strncmp(s, "/bin", 4) == 0) s += 4;
			for(sh = shebang; *sh != NULL; sh++)
				if (strncmp(*sh, s, strlen(*sh)) == 0)
					return 1;
		}
	}
	return fgw_test_parse_fn(filename, exts);
}

/* API: engine registration */
static const fgw_eng_t fgw_cli_eng = {
	"cli",
	fgws_cli_call_script,
	fgws_cli_init,
	fgws_cli_load,
	fgws_cli_unload,
	fgws_cli_reg_func,
	NULL,
	fgws_cli_test_parse
};

int pplg_check_ver_fungw_cli(int version_we_need)
{
	return 0;
}

int pplg_init_fungw_cli(void)
{
	(void)sh_head; /* suppress warning on unised var */
	fgw_eng_reg(&fgw_cli_eng);
	return 0;
}

void pplg_uninit_fungw_cli(void)
{
	fgw_eng_unreg(fgw_cli_eng.name);
}
