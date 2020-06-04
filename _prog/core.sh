##### Core

# ATTENTION: Overload with 'ops.sh' or similar.
_pcb-rnd() {
	if type pcb-rnd > /dev/null 2>&1
	then
		pcb-rnd "$@"
		return
	fi
	
	"$scriptLib"/pcb-rnd-2.2.1/src/pcb-rnd "$@"
	return
}
# _pcb-rnd() {
# 	"$scriptLib"/pcb-rnd-2.2.1/src/pcb-rnd "$@"
# 	return
# }


_test_build-app_pcb-rnd() {
	_getDep /usr/include/glib-2.0/glib.h
	_getDep glib-2.0/glib.h
	
	_getDep /usr/include/gtk-2.0/gdk/gdk.h
	_getDep gtk-2.0/gdk/gdk.h
	
	_getDep /usr/include/gtkglext-1.0/gdk/gdkgl.h
	_getDep gtkglext-1.0/gdk/gdkgl.h
	
	_getDep /usr/include/Mrm/MrmPublic.h
	_getDep Mrm/MrmPublic.h
	_getDep /usr/include/Xm/Xm.h
	_getDep Xm/Xm.h
	_getDep /usr/include/uil/Uil.h
	_getDep uil/Uil.h
	
	_getDep /usr/include/gd.h
	_getDep gd.h
	
	
	_getDep flex
	_getDep flex++
	
	_getDep bison
	
	_getDep /usr/lib/x86_64-linux-gnu/liby.a
	_getDep liby.a
	
	
	# Needed by fungw .
	
	# CAUTION: Version 1.0.1 is delcared 'too old' by 'pcb-rnd' project.
	#_getDep /usr/include/genht/ht.h
	#_getDep genht/ht.h
	#_wantDep /usr/include/genht/ht.h && _messagePlain_request 'request: ensure libgenht is newer than 1.0.1'
	#_wantDep genht/ht.h && _messagePlain_request 'request: ensure libgenht is newer than 1.0.1'
	
	# CAUTION: "At the moment libmawk must be installed as root into /usr"... (pcb-rnd project)
	_getDep /usr/include/libmawk.h
	#_getDep libmawk.h
}

_build-app_pcb-rnd_genht() {
	_wantGetDep /usr/lib/libgenht.so.1.1.1 && return 0
	
	cd "$scriptLib"/genht
	
	make -j$(nprocs)
	sudo -n make install
	
	cd "$scriptLib"
	return 0
}


# WARNING: Not verified if fungw will obtain crucial scripting languages (e.g. python, lua, perl, tcl, mawk) when compiled on a typical system.
# http://repo.hu/projects/pcb-rnd/user/06_feature/scripting/install.txt
# svn co svn://repo.hu/genht/trunk genht
_build-app_pcb-rnd_fungw() {
	_wantGetDep /usr/local/lib/libfungw.so && return 0
	_wantGetDep fungw && return 0
	
	_test_build-app_pcb-rnd
	
	_mustGetSudo
	
	_build-app_pcb-rnd_genht
	
	
	
	cd "$scriptLib"/fungw
	
	./configure
	make -j 4
	sudo -n make install
	
	cd "$scriptLib"
	return 0
}

_build-app_pcb-rnd() {
	_test_build-app_pcb-rnd
	
	_mustGetSudo
	
	_build-app_pcb-rnd_fungw
	
	
	
	cd "$scriptLib"/pcb-rnd-2.2.1
	
	./configure --all=buildin --buildin-lib_compat_help --buildin-lib_gensexpr --buildin-lib_gtk_common --buildin-lib_hid_common --buildin-lib_hid_gl --buildin-lib_hid_pcbui --buildin-lib_netmap --buildin-lib_polyhelp --buildin-lib_vfs --buildin-lib_wget --buildin-acompnet --buildin-act_draw --buildin-act_read --buildin-ar_cpcb --buildin-asm --buildin-autocrop --buildin-autoplace --buildin-autoroute --buildin-dialogs --buildin-distalign --buildin-distaligntext --buildin-djopt --buildin-draw_csect --buildin-draw_fab --buildin-draw_fontsel --buildin-drc_orig --buildin-drc_query --buildin-exto_std --buildin-fontmode --buildin-jostle --buildin-millpath --buildin-mincut --buildin-oldactions --buildin-order_pcbway --buildin-order --buildin-polycombine --buildin-polystitch --buildin-propedit --buildin-puller --buildin-query --buildin-renumber --buildin-report --buildin-rubberband_orig --buildin-script --buildin-serpentine --buildin-shand_cmd --buildin-shape --buildin-sketch_route --buildin-smartdisperse --buildin-stroke --buildin-teardrops --buildin-tool_std --buildin-vendordrill --buildin-fp_board --buildin-fp_fs --buildin-fp_wget --buildin-import_dsn --buildin-import_gnetlist --buildin-import_hpgl --buildin-import_net_cmd --buildin-import_net_action --buildin-import_netlist --buildin-import_pxm_gd --buildin-import_pxm_pnm --buildin-import_sch2 --buildin-import_sch --buildin-import_ttf --buildin-cam --buildin-export_bom --buildin-export_dsn --buildin-export_dxf --buildin-export_excellon --buildin-export_fidocadj --buildin-export_gcode --buildin-export_gerber --buildin-export_lpr --buildin-export_oldconn --buildin-export_openems --buildin-export_openscad --buildin-export_png --buildin-export_ps --buildin-export_stat --buildin-export_stl --buildin-export_svg --buildin-export_test --buildin-export_xy --buildin-io_dsn --buildin-io_kicad_legacy --buildin-io_kicad --buildin-io_pcb
	
	make -j$(nproc)
	
	# WARNING: May be unnecessary, and may cause conflicts with distro package.
	# WARNING: Not adequately tested - "make uninstall" .
	#sudo -n make install
	
	cd "$scriptLib"
	return 0
}


