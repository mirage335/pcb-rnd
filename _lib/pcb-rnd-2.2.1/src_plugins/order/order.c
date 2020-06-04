/*
 *                            COPYRIGHT
 *
 *  pcb-rnd, interactive printed circuit board design
 *  Copyright (C) 2019 Tibor 'Igor2' Palinkas
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

#include "config.h"

#include <stdio.h>

#include <librnd/core/actions.h>
#include "board.h"
#include "data.h"
#include <librnd/core/pcb-printf.h>
#include <librnd/core/plugins.h>
#include <librnd/core/hid_dad.h>
#include <librnd/core/hid_cfg.h>
#include "layer.h"
#include <librnd/core/event.h>
#include "order_conf.h"
#include "../src_plugins/order/conf_internal.c"

#include "order.h"

static const char *order_cookie = "order plugin";

conf_order_t conf_order;
#define ORDER_CONF_FN "order.conf"

#define ANCH "@feature_plugins"

vtp0_t pcb_order_imps;

void pcb_order_reg(const pcb_order_imp_t *imp)
{
	vtp0_append(&pcb_order_imps, (void *)imp);
}


#include "order_dlg.c"

static const char pcb_acts_OrderPCB[] = "orderPCB([gui])";
static const char pcb_acth_OrderPCB[] = "Order the board from a fab";
fgw_error_t pcb_act_OrderPCB(fgw_arg_t *res, int argc, fgw_arg_t *argv)
{
	const char *cmd = "gui";

	PCB_ACT_MAY_CONVARG(1, FGW_STR, OrderPCB, cmd = argv[1].val.str);

	if (strcmp(cmd, "gui") == 0) {
		PCB_ACT_IRES(order_dialog());
		return 0;
	}
	
	pcb_message(PCB_MSG_ERROR, "CLI version of OrderPCB() not yet implemented\n");
	PCB_ACT_IRES(-1);
	return 0;
}

static pcb_action_t order_action_list[] = {
	{"OrderPCB", pcb_act_OrderPCB, pcb_acth_OrderPCB, pcb_acts_OrderPCB}
};

static void order_install_menu(void *ctx, pcb_hid_cfg_t *cfg, lht_node_t *node, char *path)
{
	char *end = path + strlen(path);
	pcb_menu_prop_t props;
	char act[256];

	memset(&props, 0,sizeof(props));
	props.action = act;
	props.tip = "Order your PCB from a fab\nusing a dialog box";
	props.accel = "<Key>f;<Key>x;<Key>o";
	props.cookie = ANCH;

	/* prepare for appending the strings at the end of the path, "under" the anchor */
	*end = '/';
	end++;

	strcpy(end, "Order PCB"); strcpy(act, "OrderPCB(gui)");
	pcb_gui->create_menu(pcb_gui, path, &props);
}

void pcb_order_free_field_data(order_ctx_t *octx, pcb_order_field_t *f)
{
	if (f->enum_vals != NULL) {
		char **s;
		for(s = f->enum_vals; *s != NULL; s++)
			free(s);
		free(f->enum_vals);
		f->enum_vals = NULL;
	}
	if (f->val.str != NULL) {
		free((char *)f->val.str);
		f->val.str = NULL;
	}
}

static void autoload_field_crd(order_ctx_t *octx, pcb_order_field_t *f, pcb_coord_t c)
{
	switch(f->type) {
		case PCB_HATT_INTEGER: f->val.lng = c; break;
		case PCB_HATT_COORD: f->val.crd = c; break;
		case PCB_HATT_STRING:
			free((char *)f->val.str);
			f->val.str = pcb_strdup_printf("%$mm", c);
			break;
		default: break;
	}
}

static void autoload_field_lng(order_ctx_t *octx, pcb_order_field_t *f, long l, int roundup)
{
	long tmp, best, diff;
	int n, bestn;
	char **s;

	switch(f->type) {
		case PCB_HATT_INTEGER: f->val.lng = l; break;
		case PCB_HATT_COORD: f->val.crd = PCB_MM_TO_COORD(l); break;
		case PCB_HATT_STRING:
			free((char *)f->val.str);
			f->val.str = pcb_strdup_printf("%ld", l);
			break;
		case PCB_HATT_ENUM:
			bestn = -1;
			/* find the closest enum value, rounding l up or down only */
			for(n = 0, s = f->enum_vals; *s != NULL; n++,s++) {
				tmp = strtol(*s, NULL, 10);
				if (roundup) {
					if (tmp < l) continue;
				}
				else {
					if (tmp > l) continue;
				}
				diff = tmp - l;
				if (diff < 0) diff = -diff;
				if ((diff < best) || (bestn < 0)) {
					bestn = n;
					best = diff;
				}
			}
			if (bestn >= 0)
				f->val.lng = bestn;
			break;
		default: break;
	}
}

void pcb_order_autoload_field(order_ctx_t *octx, pcb_order_field_t *f)
{
	pcb_box_t bb;
	long l;
	pcb_layergrp_id_t gid;

	switch(f->autoload) {
		case PCB_OAL_none: return;
		case PCB_OAL_WIDTH:
			pcb_data_bbox(&bb, PCB->Data, 0);
			autoload_field_crd(octx, f, bb.X2 - bb.X1);
			break;
		case PCB_OAL_HEIGHT:
			pcb_data_bbox(&bb, PCB->Data, 0);
			autoload_field_crd(octx, f, bb.X2 - bb.X1);
			break;
		case PCB_OAL_LAYERS:
			for(gid = 0, l = 0; gid < PCB->LayerGroups.len; gid++)
				if (PCB->LayerGroups.grp[gid].ltype & PCB_LYT_COPPER)
					l++;
			autoload_field_lng(octx, f, l, 1);
			break;
	}
}


static void order_menu_init(pcb_hidlib_t *hidlib, void *user_data, int argc, pcb_event_arg_t argv[])
{
	pcb_hid_cfg_map_anchor_menus(ANCH, order_install_menu, NULL);
}

int pplg_check_ver_order(int ver_needed) { return 0; }

void pplg_uninit_order(void)
{
	pcb_remove_actions_by_cookie(order_cookie);
	pcb_event_unbind_allcookie(order_cookie);
	pcb_conf_unreg_file(ORDER_CONF_FN, order_conf_internal);
	pcb_conf_unreg_fields("plugins/order/");
}

int pplg_init_order(void)
{
	PCB_API_CHK_VER;

	pcb_conf_reg_file(ORDER_CONF_FN, order_conf_internal);
#define conf_reg(field,isarray,type_name,cpath,cname,desc,flags) \
	pcb_conf_reg_field(conf_order, field,isarray,type_name,cpath,cname,desc,flags);
#include "order_conf_fields.h"

	PCB_REGISTER_ACTIONS(order_action_list, order_cookie)
	pcb_event_bind(PCB_EVENT_GUI_INIT, order_menu_init, NULL, order_cookie);
	return 0;
}
