# ATTENTION: Overload with 'ops.sh' or similar. Cause to not to call "_wantGetDep" and immediately fail (return 1) in all cases if system instaltion in undesired.
# Unusual. Attempt to install distribution 'pcb-rnd' .
# Unusual. Possibly no need to continue checking build dependencies if system installation exists.
# No need to rebuild if a valid system installation exists.
# WARNING: Distribution version might not supply essential features.
_accept_system-pcb-rnd() {
	_wantGetDep pcb-rnd && return 0
	return 1
}
# _accept_system-pcb-rnd() {
# 	#_wantGetDep pcb-rnd && return 0
# 	return 1
# }

_test_prog() {
	true
	
	_accept_system-pcb-rnd
}


_test_build_prog() {
	_accept_system-pcb-rnd && return 0
	
	_test_build-app_pcb-rnd
}

_testBuilt_prog() {
	_accept_system-pcb-rnd && return 0
	
	# Limited test of self-built PCB binary.
	# CAUTION: Copying to an alternate distro or otherwise possibly binary incompatible system may allow possibility of untested failures!
	if [[ -e "$scriptLib"/pcb-rnd-2.2.1/src/pcb-rnd ]] && "$scriptLib"/pcb-rnd-2.2.1/src/pcb-rnd --help > /dev/null 2>&1
	then
		return 0
	fi
	
	_stop 1
}

_build_prog() {
	_test_build-app_pcb-rnd
	
	"$scriptAbsoluteLocation" _build-app_pcb-rnd
}

_setup_prog() {
	true
}
