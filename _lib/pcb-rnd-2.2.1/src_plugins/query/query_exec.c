/*
 *                            COPYRIGHT
 *
 *  pcb-rnd, interactive printed circuit board design
 *  Copyright (C) 2016,2019 Tibor 'Igor2' Palinkas
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
 *  Contact:
 *    Project page: http://repo.hu/projects/pcb-rnd
 *    lead developer: http://repo.hu/projects/pcb-rnd/contact.html
 *    mailing list: pcb-rnd (at) list.repo.hu (send "subscribe")
 */

/* Query language - execution */

#include <stdio.h>
#include "config.h"
#include "data.h"
#include "query.h"
#include "query_exec.h"
#include "query_access.h"
#include <librnd/core/pcb-printf.h>

#define PCB dontuse

void pcb_qry_setup(pcb_qry_exec_t *ctx, pcb_board_t *pcb, pcb_qry_node_t *root)
{
	ctx->pcb = pcb;
	ctx->root = root;
	ctx->iter = NULL;
}

void pcb_qry_init(pcb_qry_exec_t *ctx, pcb_board_t *pcb, pcb_qry_node_t *root, int bufno)
{
	memset(ctx, 0, sizeof(pcb_qry_exec_t));
	ctx->all.type = PCBQ_VT_LST;
	if (bufno == -1)
		pcb_qry_list_all_pcb(&ctx->all, PCB_OBJ_ANY & (~PCB_OBJ_LAYER));
	else
		pcb_qry_list_all_data(&ctx->all, pcb_buffers[bufno].Data, PCB_OBJ_ANY & (~PCB_OBJ_LAYER));

	pcb_qry_setup(ctx, pcb, root);
}

void pcb_qry_autofree(pcb_qry_exec_t *ctx)
{
	long l;

	if (ctx->iter != NULL) {
		for(l = 0; l < ctx->iter->num_vars; l++) {
			if (ctx->iter->lst[l].data.lst.array != ctx->all.data.lst.array) /* some lists are just alias to ->all, do not free them */
				pcb_qry_list_free(&ctx->iter->lst[l]);
		}
	}

	for(l = 0; l < ctx->autofree.used; l++)
		free(ctx->autofree.array[l]);
	vtp0_uninit(&ctx->autofree);

	ctx->iter = NULL; /* no need to free the iterator: ctx->root free handled that as it was allocated in one of the nodes */
}

void pcb_qry_uninit(pcb_qry_exec_t *ctx)
{
	pcb_qry_autofree(ctx);

	pcb_qry_list_free(&ctx->all);

	if (ctx->obj2netterm_inited)
		htpp_uninit(&ctx->obj2netterm);
}

static void val_free_fields(pcb_qry_val_t *val)
{
	switch(val->type) {
		case PCBQ_VT_LST:
			pcb_qry_list_free(val);
			break;
		case PCBQ_VT_STRING:
			TODO("decide if strings are strdup'd; action return may need to, ther est not; maybe introduce DSTR for dynamic string?");
			break;
		case PCBQ_VT_VOID:
		case PCBQ_VT_OBJ:
		case PCBQ_VT_COORD:
		case PCBQ_VT_LONG:
		case PCBQ_VT_DOUBLE:
			break;
	}
}

static int pcb_qry_run_(pcb_qry_exec_t *ec, pcb_qry_node_t *prg, int it_reset, int eval_list, void (*cb)(void *user_ctx, pcb_qry_val_t *res, pcb_any_obj_t *current), void *user_ctx)
{
	pcb_qry_val_t res;
	int errs = 0;

	if (it_reset && (pcb_qry_it_reset(ec, prg) != 0))
		return -1;

	do {
		if (pcb_qry_eval(ec, prg, &res) == 0) {
			if ((eval_list) && (res.type == PCBQ_VT_LST)) {
				long n;

				/* special case: result is a list, need to return each object so 'let' gets it */
				for(n = 0; n < res.data.lst.used; n++)
					cb(user_ctx, &res, res.data.lst.array[n]);
			}
			else if (ec->iter->last_obj != NULL) {
				cb(user_ctx, &res, ec->iter->last_obj);
			}

			val_free_fields(&res);
		}
		else
			errs++;
	} while(pcb_qry_it_next(ec));
	return errs;
}


typedef struct {
	pcb_qry_exec_t *ctx;
	vtp0_t *vt;
} let_ctx_t;

static void let_cb(void *user_ctx, pcb_qry_val_t *res, pcb_any_obj_t *current)
{
	let_ctx_t *lctx = user_ctx;

	if (res->type == PCBQ_VT_OBJ) /* result overwrites last checked node */
		current = res->data.obj;

	if (pcb_qry_is_true(res))
		vtp0_append(lctx->vt, current);
}

static int pcb_qry_it_reset_(pcb_qry_exec_t *ctx);
static void pcb_qry_let(pcb_qry_exec_t *ctx, pcb_qry_node_t *node)
{
	let_ctx_t lctx;
	int vi = node->data.children->data.crd;
	pcb_qry_node_t *expr = node->data.children->next;

	lctx.ctx = ctx;
	pcb_qry_it_reset(ctx, node);

	/* set up the list */
	ctx->iter->lst[vi].type = PCBQ_VT_LST;
	lctx.vt = &ctx->iter->lst[vi].data.lst;

	/* evaluate 'let' the expression, filling up the list */
	pcb_qry_it_reset_(lctx.ctx);
	ctx->iter->it_active = node->precomp.it_active;
	pcb_qry_run_(lctx.ctx, expr, 0, 1,let_cb, &lctx);
	ctx->iter->it_active = NULL;

	/* initialize the iterator */
	ctx->iter->vects[vi] = &ctx->iter->lst[vi].data.lst;
	ctx->iter->idx[vi] = 0;
}

int pcb_qry_run(pcb_qry_exec_t *ec, pcb_board_t *pcb, pcb_qry_node_t *prg, int bufno, void (*cb)(void *user_ctx, pcb_qry_val_t *res, pcb_any_obj_t *current), void *user_ctx)
{
	int ret = 0, r, ec_uninit = 0;
	pcb_qry_exec_t ec_local;

	if (ec == NULL) {
		ec = &ec_local;
		ec_uninit = 1;
		pcb_qry_init(ec, pcb, prg, bufno);
	}
	else {
		if (ec->pcb != pcb)
			return -1;
		pcb_qry_setup(ec, pcb, prg);
	}

	if (prg->type == PCBQ_EXPR_PROG) {
		ret = pcb_qry_run_(ec, prg, 1, 0, cb, user_ctx);
	}
	else if (prg->type == PCBQ_RULE) {
		pcb_qry_node_t *n;

		/* execute 'let' statements first */
		for(n = prg->data.children->next->next; n != NULL; n = n->next) {
			if (n->type == PCBQ_LET)
				pcb_qry_let(ec, n);
		}

		for(n = prg->data.children->next->next; n != NULL; n = n->next) {
			switch(n->type) {
				case PCBQ_LET: break;
				case PCBQ_ASSERT:
					ec->root = n;
					if (ec->iter != NULL)
						ec->iter->it_active = n->precomp.it_active;
					r = pcb_qry_run_(ec, n->data.children, 1, 0, cb, user_ctx);
					if (ec->iter != NULL)
						ec->iter->it_active = NULL;
					if (r < 0)
						ret = r;
					else if (ret >= 0)
						ret += r;
					ec->root = NULL;
					break;
				default:;
			}
		}
	}
	else
		ret = -1;

	if (ec_uninit)
		pcb_qry_uninit(&ec_local);
	else
		pcb_qry_autofree(ec);
	return ret;
}

/* load unary operand to o1 */
#define UNOP() \
do { \
	if ((node->data.children == NULL) || (node->data.children->next != NULL)) \
		return -1; \
	if (pcb_qry_eval(ctx, node->data.children, &o1) < 0) \
		return -1; \
} while(0)

/* load 1st binary operand to o1 */
#define BINOPS1() \
do { \
	if ((node->data.children == NULL) || (node->data.children->next == NULL) || (node->data.children->next->next != NULL)) \
		return -1; \
	if (pcb_qry_eval(ctx, node->data.children, &o1) < 0) \
		return -1; \
} while(0)

/* load 2nd binary operand to o2 */
#define BINOPS2() \
do { \
	if (pcb_qry_eval(ctx, node->data.children->next, &o2) < 0) \
		return -1; \
} while(0)

/* load binary operands to o1 and o2 */
#define BINOPS() \
do { \
	BINOPS1(); \
	BINOPS2(); \
} while(0)

static int promote(pcb_qry_val_t *a, pcb_qry_val_t *b)
{
	if ((a->type == PCBQ_VT_VOID) || (b->type == PCBQ_VT_VOID)) {
		a->type = b->type = PCBQ_VT_VOID;
		return 0;
	}
	if (a->type == b->type)
		return 0;
	switch(a->type) {
		case PCBQ_VT_OBJ:   return -1;
		case PCBQ_VT_LST:   return -1;
		case PCBQ_VT_COORD:
			if (b->type == PCBQ_VT_DOUBLE)  PCB_QRY_RET_DBL(a, a->data.crd);
			if (b->type == PCBQ_VT_LONG)    PCB_QRY_RET_COORD(b, b->data.lng);
			return -1;
		case PCBQ_VT_LONG:
			if (b->type == PCBQ_VT_DOUBLE)  PCB_QRY_RET_DBL(a, a->data.lng);
			if (b->type == PCBQ_VT_COORD)   PCB_QRY_RET_DBL(a, a->data.lng);
			return -1;
		case PCBQ_VT_DOUBLE:
			if (b->type == PCBQ_VT_COORD)  PCB_QRY_RET_DBL(b, b->data.crd);
			if (b->type == PCBQ_VT_LONG)   PCB_QRY_RET_DBL(b, b->data.lng);
			return -1;
		case PCBQ_VT_STRING:
		case PCBQ_VT_VOID:
			return -1;
	}
	return -1;
}

static const char *op2str(char *buff, int buff_size, pcb_qry_val_t *val)
{
	switch(val->type) {
		case PCBQ_VT_COORD:
			pcb_snprintf(buff, buff_size, "%mI", val->data.crd);
			return buff;
		case PCBQ_VT_LONG:
			pcb_snprintf(buff, buff_size, "%ld", val->data.lng);
			return buff;
		case PCBQ_VT_DOUBLE:
			pcb_snprintf(buff, buff_size, "%f", val->data.crd);
			return buff;
		case PCBQ_VT_STRING:
			return val->data.str;
		case PCBQ_VT_VOID:
		case PCBQ_VT_OBJ:
		case PCBQ_VT_LST:   return NULL;
	}
	return NULL;
}

int pcb_qry_is_true(pcb_qry_val_t *val)
{
	switch(val->type) {
		case PCBQ_VT_VOID:     return 0;
		case PCBQ_VT_OBJ:      return val->data.obj->type != PCB_OBJ_VOID;
		case PCBQ_VT_LST:      return vtp0_len(&val->data.lst) > 0;
		case PCBQ_VT_COORD:    return val->data.crd;
		case PCBQ_VT_LONG:     return val->data.lng;
		case PCBQ_VT_DOUBLE:   return val->data.dbl;
		case PCBQ_VT_STRING:   return (val->data.str != NULL) && (*val->data.str != '\0');
	}
	return 0;
}

static void setup_iter_list(pcb_qry_exec_t *ctx, int var_id, pcb_qry_val_t *listval)
{
	ctx->iter->lst[var_id] = *listval;
	assert(listval->type == PCBQ_VT_LST);

	ctx->iter->vects[var_id] = &listval->data.lst;
	ctx->iter->idx[var_id] = 0;
}

static int pcb_qry_it_reset_(pcb_qry_exec_t *ctx)
{
	int n;

	pcb_qry_iter_init(ctx->iter);
	for(n = 0; n < ctx->iter->num_vars; n++) {
		if (strcmp(ctx->iter->vn[n], "@") == 0) {
			setup_iter_list(ctx, n, &ctx->all);
			ctx->iter->last_obj = NULL;
			break;
		}
	}
	return 0;
}

int pcb_qry_it_reset(pcb_qry_exec_t *ctx, pcb_qry_node_t *node)
{
	ctx->iter = pcb_qry_find_iter(node);
	if (ctx->iter == NULL)
		return -1;

	return pcb_qry_it_reset_(ctx);
}

static int pcb_qry_it_active(pcb_query_iter_t *iter, int i)
{
	if (iter->it_active == NULL) return 1; /* no activity map: all iterators are active */
	if (i >= iter->it_active->used) return 0;
	return iter->it_active->array[i];
}

int pcb_qry_it_next(pcb_qry_exec_t *ctx)
{
	int i;
	for(i = 0; i < ctx->iter->num_vars; i++) {
		if ((ctx->iter->vects[i] != NULL) && pcb_qry_it_active(ctx->iter, i)) {
			ctx->iter->idx[i]++;
			if (ctx->iter->idx[i] < vtp0_len(ctx->iter->vects[i]))
				return 1;
		}
		ctx->iter->idx[i] = 0;
	}
	return 0;
}


/* load s1 and s2 from o1 and o2, convert empty string to NULL */
#define load_strings_null() \
do { \
	s1 = o1.data.str; \
	s2 = o2.data.str; \
	if ((s1 != NULL) && (*s1 == '\0')) s1 = NULL; \
	if ((s2 != NULL) && (*s2 == '\0')) s2 = NULL; \
} while(0)

static char *pcb_qry_empty = "";

/* load s1 and s2 from o1 and o2, convert NULL to empty string */
#define load_strings_empty() \
do { \
	s1 = o1.data.str; \
	s2 = o2.data.str; \
	if (s1 == NULL) s1 = pcb_qry_empty; \
	if (s2 == NULL) s2 = pcb_qry_empty; \
} while(0)

int pcb_qry_eval(pcb_qry_exec_t *ctx, pcb_qry_node_t *node, pcb_qry_val_t *res)
{
	pcb_qry_val_t o1, o2;
	pcb_any_obj_t **tmp;
	const char *s1, *s2;

	switch(node->type) {
		case PCBQ_EXPR:
			return pcb_qry_eval(ctx, node->data.children, res);

		case PCBQ_EXPR_PROG: {
			pcb_qry_node_t *itn, *exprn;
			itn = node->data.children;
			if (itn == NULL)
				return -1;
			exprn = itn->next;
			if (exprn == NULL)
				return -1;
			if (itn->type != PCBQ_ITER_CTX)
				return -1;
			if (ctx->iter != itn->data.iter_ctx)
				return -1;
			return pcb_qry_eval(ctx, exprn, res);
		}


		case PCBQ_OP_AND: /* lazy */
			BINOPS1();
			if (!pcb_qry_is_true(&o1))
				PCB_QRY_RET_INT(res, 0);
			BINOPS2();
			if (!pcb_qry_is_true(&o2))
				PCB_QRY_RET_INT(res, 0);
			PCB_QRY_RET_INT(res, 1);

		case PCBQ_OP_THUS: /* lazy */
			BINOPS1();
			if (!pcb_qry_is_true(&o1))
				PCB_QRY_RET_INV(res);
			BINOPS2();
			*res = o2;
			return 0;

		case PCBQ_OP_OR: /* lazy */
			BINOPS1();
			if (pcb_qry_is_true(&o1))
				PCB_QRY_RET_INT(res, 1);
			BINOPS2();
			if (pcb_qry_is_true(&o2))
				PCB_QRY_RET_INT(res, 1);
			PCB_QRY_RET_INT(res, 0);

		case PCBQ_OP_EQ:
			BINOPS();
			if (promote(&o1, &o2) != 0)
				return -1;
			switch(o1.type) {
				case PCBQ_VT_VOID:     PCB_QRY_RET_INV(res);
				case PCBQ_VT_OBJ:      PCB_QRY_RET_INT(res, ((o1.data.obj) == (o2.data.obj)));
				case PCBQ_VT_LST:      PCB_QRY_RET_INT(res, pcb_qry_list_cmp(&o1, &o2));
				case PCBQ_VT_COORD:    PCB_QRY_RET_INT(res, o1.data.crd == o2.data.crd);
				case PCBQ_VT_LONG:     PCB_QRY_RET_INT(res, o1.data.lng == o2.data.lng);
				case PCBQ_VT_DOUBLE:   PCB_QRY_RET_INT(res, o1.data.dbl == o2.data.dbl);
				case PCBQ_VT_STRING:
					load_strings_null();
					if (s1 == s2)
						PCB_QRY_RET_INT(res, 1);
					if ((s1 == NULL) || (s2 == NULL))
						PCB_QRY_RET_INT(res, 0);
					PCB_QRY_RET_INT(res, strcmp(s1, s2) == 0);
			}
			return -1;

		case PCBQ_OP_NEQ:
			BINOPS();
			if (promote(&o1, &o2) != 0)
				return -1;
			switch(o1.type) {
				case PCBQ_VT_VOID:     PCB_QRY_RET_INV(res);
				case PCBQ_VT_OBJ:      PCB_QRY_RET_INT(res, ((o1.data.obj) != (o2.data.obj)));
				case PCBQ_VT_LST:      PCB_QRY_RET_INT(res, !pcb_qry_list_cmp(&o1, &o2));
				case PCBQ_VT_COORD:    PCB_QRY_RET_INT(res, o1.data.crd != o2.data.crd);
				case PCBQ_VT_LONG:     PCB_QRY_RET_INT(res, o1.data.lng != o2.data.lng);
				case PCBQ_VT_DOUBLE:   PCB_QRY_RET_INT(res, o1.data.dbl != o2.data.dbl);
				case PCBQ_VT_STRING:
					load_strings_null();
					if (s1 == s2)
						PCB_QRY_RET_INT(res, 0);
					if ((s1 == NULL) || (s2 == NULL))
						PCB_QRY_RET_INT(res, 1);
					PCB_QRY_RET_INT(res, strcmp(s1, s2) != 0);
			}
			return -1;

		case PCBQ_OP_GTEQ:
			BINOPS();
			if (promote(&o1, &o2) != 0)
				return -1;
			switch(o1.type) {
				case PCBQ_VT_VOID:     PCB_QRY_RET_INV(res);
				case PCBQ_VT_COORD:    PCB_QRY_RET_INT(res, o1.data.crd >= o2.data.crd);
				case PCBQ_VT_LONG:     PCB_QRY_RET_INT(res, o1.data.lng >= o2.data.lng);
				case PCBQ_VT_DOUBLE:   PCB_QRY_RET_INT(res, o1.data.dbl >= o2.data.dbl);
				case PCBQ_VT_STRING:
					load_strings_empty();
					PCB_QRY_RET_INT(res, strcmp(s1, s2) >= 0);
				default:               return -1;
			}
			return -1;

		case PCBQ_OP_LTEQ:
			BINOPS();
			if (promote(&o1, &o2) != 0)
				return -1;
			switch(o1.type) {
				case PCBQ_VT_VOID:     PCB_QRY_RET_INV(res);
				case PCBQ_VT_COORD:    PCB_QRY_RET_INT(res, o1.data.crd <= o2.data.crd);
				case PCBQ_VT_LONG:     PCB_QRY_RET_INT(res, o1.data.lng <= o2.data.lng);
				case PCBQ_VT_DOUBLE:   PCB_QRY_RET_INT(res, o1.data.dbl <= o2.data.dbl);
				case PCBQ_VT_STRING:
					load_strings_empty();
					PCB_QRY_RET_INT(res, strcmp(s1, s2) <= 0);
				default:               return -1;
			}
			return -1;

		case PCBQ_OP_GT:
			BINOPS();
			if (promote(&o1, &o2) != 0)
				return -1;
			switch(o1.type) {
				case PCBQ_VT_VOID:     PCB_QRY_RET_INV(res);
				case PCBQ_VT_COORD:    PCB_QRY_RET_INT(res, o1.data.crd > o2.data.crd);
				case PCBQ_VT_LONG:     PCB_QRY_RET_INT(res, o1.data.lng > o2.data.lng);
				case PCBQ_VT_DOUBLE:   PCB_QRY_RET_INT(res, o1.data.dbl > o2.data.dbl);
				case PCBQ_VT_STRING:
					load_strings_empty();
					PCB_QRY_RET_INT(res, strcmp(s1, s2) > 0);
				default:               return -1;
			}
			return -1;
		case PCBQ_OP_LT:
			BINOPS();
			if (promote(&o1, &o2) != 0)
				return -1;
			switch(o1.type) {
				case PCBQ_VT_VOID:     PCB_QRY_RET_INV(res);
				case PCBQ_VT_COORD:    PCB_QRY_RET_INT(res, o1.data.crd < o2.data.crd);
				case PCBQ_VT_LONG:     PCB_QRY_RET_INT(res, o1.data.lng < o2.data.lng);
				case PCBQ_VT_DOUBLE:   PCB_QRY_RET_INT(res, o1.data.dbl < o2.data.dbl);
				case PCBQ_VT_STRING:
					load_strings_empty();
					PCB_QRY_RET_INT(res, strcmp(s1, s2) < 0);
				default:               return -1;
			}
			return -1;

		case PCBQ_OP_ADD:
			BINOPS();
			if (promote(&o1, &o2) != 0)
				return -1;
			switch(o1.type) {
				case PCBQ_VT_VOID:     PCB_QRY_RET_INV(res);
				case PCBQ_VT_COORD:    PCB_QRY_RET_INT(res, o1.data.crd + o2.data.crd);
				case PCBQ_VT_LONG:     PCB_QRY_RET_INT(res, o1.data.lng + o2.data.lng);
				case PCBQ_VT_DOUBLE:   PCB_QRY_RET_DBL(res, o1.data.dbl + o2.data.dbl);
				default:               return -1;
			}
			return -1;

		case PCBQ_OP_SUB:
			BINOPS();
			if (promote(&o1, &o2) != 0)
				return -1;
			switch(o1.type) {
				case PCBQ_VT_VOID:     PCB_QRY_RET_INV(res);
				case PCBQ_VT_COORD:    PCB_QRY_RET_INT(res, o1.data.crd - o2.data.crd);
				case PCBQ_VT_LONG:     PCB_QRY_RET_INT(res, o1.data.lng - o2.data.lng);
				case PCBQ_VT_DOUBLE:   PCB_QRY_RET_DBL(res, o1.data.dbl - o2.data.dbl);
				default:               return -1;
			}
			return -1;

		case PCBQ_OP_MUL:
			BINOPS();
			if (promote(&o1, &o2) != 0)
				return -1;
			switch(o1.type) {
				case PCBQ_VT_VOID:     PCB_QRY_RET_INV(res);
				case PCBQ_VT_COORD:    PCB_QRY_RET_INT(res, o1.data.crd * o2.data.crd);
				case PCBQ_VT_LONG:     PCB_QRY_RET_INT(res, o1.data.lng * o2.data.lng);
				case PCBQ_VT_DOUBLE:   PCB_QRY_RET_DBL(res, o1.data.dbl * o2.data.dbl);
				default:               return -1;
			}
			return -1;

		case PCBQ_OP_DIV:
			BINOPS();
			if (promote(&o1, &o2) != 0)
				return -1;
			switch(o1.type) {
				case PCBQ_VT_VOID:     PCB_QRY_RET_INV(res);
				case PCBQ_VT_COORD:    PCB_QRY_RET_INT(res, o1.data.crd / o2.data.crd);
				case PCBQ_VT_LONG:     PCB_QRY_RET_INT(res, o1.data.lng / o2.data.lng);
				case PCBQ_VT_DOUBLE:   PCB_QRY_RET_DBL(res, o1.data.dbl / o2.data.dbl);
				default:               return -1;
			}
			return -1;

		case PCBQ_OP_MATCH:
			BINOPS1();
			{
				pcb_qry_node_t *o2n =node->data.children->next;
				char buff[128];
				const char *s;
				if (o2n->type != PCBQ_DATA_REGEX)
					return -1;
				s = op2str(buff, sizeof(buff), &o1);
				if (s != NULL)
					PCB_QRY_RET_INT(res, re_se_exec(o2n->precomp.regex, s));
			}
			PCB_QRY_RET_INV(res);

		case PCBQ_OP_NOT:
			UNOP();
			PCB_QRY_RET_INT(res, !pcb_qry_is_true(&o1));

		case PCBQ_FIELD_OF:
			if ((node->data.children == NULL) || (node->data.children->next == NULL))
				return -1;
			if (pcb_qry_eval(ctx, node->data.children, &o1) < 0)
				return -1;
			return pcb_qry_obj_field(&o1, node->data.children->next, res);

		case PCBQ_LET:
			/* no-op: present only in rules and are executed before any assert expression */
			return 0;

		case PCBQ_ASSERT:
			/* no-op: present only in rules and are executed manually */
			return 0;

		case PCBQ_VAR:
			assert((node->data.crd >= 0) && (node->data.crd < ctx->iter->num_vars));
			res->type = PCBQ_VT_VOID;
			if (ctx->iter->vects[node->data.crd] == NULL)
				return -1;
			tmp = (pcb_any_obj_t **)vtp0_get(ctx->iter->vects[node->data.crd], ctx->iter->idx[node->data.crd], 0);
			if ((tmp == NULL) || (*tmp == NULL))
				return -1;
			res->type = PCBQ_VT_OBJ;
			res->data.obj = ctx->iter->last_obj = *tmp;
			return 0;

		case PCBQ_LISTVAR: {
			int vi = pcb_qry_iter_var(ctx->iter, node->data.str, 0);
			if ((vi < 0) && (strcmp(node->data.str, "@") == 0)) {
				res->type = PCBQ_VT_LST;
				res->data.lst = ctx->all.data.lst;
				return 0;
			}
			if (vi >= 0) {
				res->type = PCBQ_VT_LST;
				res->data.lst = ctx->iter->lst[vi].data.lst;
				return 0;
			}
		}
		return -1;

		case PCBQ_FNAME:
			return -1; /* shall not eval such a node */

		case PCBQ_FCALL: {
			pcb_qry_val_t args[PCB_QRY_MAX_FUNC_ARGS];
			int n;
			pcb_qry_node_t *farg, *fname = node->data.children;
			if (fname == NULL)
				return -1;
			farg = fname->next;
			if ((fname->type !=  PCBQ_FNAME) || (fname->data.fnc == NULL))
				return -1;
			memset(args, 0, sizeof(args));
			for(n = 0; (n < PCB_QRY_MAX_FUNC_ARGS) && (farg != NULL); n++, farg = farg->next)
				if (pcb_qry_eval(ctx, farg, &args[n]) < 0)
					return -1;

			if (farg != NULL) {
				pcb_message(PCB_MSG_ERROR, "too many function arguments\n");
				return -1;
			}
			return fname->data.fnc(ctx, n, args, res);
		}

		case PCBQ_DATA_COORD:       PCB_QRY_RET_INT(res, node->data.crd);
		case PCBQ_DATA_DOUBLE:      PCB_QRY_RET_DBL(res, node->data.dbl);
		case PCBQ_DATA_STRING:      PCB_QRY_RET_STR(res, node->data.str);
		case PCBQ_DATA_CONST:       PCB_QRY_RET_INT(res, node->precomp.cnst);
		case PCBQ_DATA_OBJ:         PCB_QRY_RET_OBJ(res, node->precomp.obj);
		case PCBQ_DATA_INVALID:     PCB_QRY_RET_INV(res);

		/* not yet implemented: */
		case PCBQ_RULE:

		/* must not meet these while executing a node */
		case PCBQ_FLAG:
		case PCBQ_DATA_REGEX:
		case PCBQ_nodetype_max:
		case PCBQ_FIELD:
		case PCBQ_RNAME:
		case PCBQ_DATA_LYTC:
		case PCBQ_ITER_CTX:
			return -1;
	}
	return -1;
}
