test:
	cd RTT && $(MAKE) all && $(MAKE) test
	cd pcbflags && $(MAKE) all && $(MAKE) test
	cd conf && $(MAKE) all && $(MAKE) test
	cd strflags && $(MAKE) all && $(MAKE) test
	cd pcb-printf && $(MAKE) all && $(MAKE) test
	cd uniq_name && $(MAKE) all && $(MAKE) test
	cd propedit && $(MAKE) all && $(MAKE) test
	cd remote && $(MAKE) all && $(MAKE) test
	cd drc_pstk_hbrk && $(MAKE) all && $(MAKE) test
# This uses make instead of $(MAKE) on purpose as a "portbale" -j1 to work aroudn GNU make
	cd librnd && make all && make test
#	cd gsch2pcb-rnd && $(MAKE) all && $(MAKE) test
	cd drc_query && $(MAKE) test
	cd query && $(MAKE) test
	@echo " "
	@echo "+-------------------------------------------------+"
	@echo "+  All tests passed, pcb-rnd is safe to install.  +"
	@echo "+-------------------------------------------------+"

clean:
	cd RTT && $(MAKE) clean
	cd pcbflags && $(MAKE) clean
	cd conf && $(MAKE) clean
	cd strflags && $(MAKE) clean
	cd pcb-printf && $(MAKE) clean
	cd uniq_name && $(MAKE) clean
	cd propedit && $(MAKE) clean
	cd remote && $(MAKE) clean
	cd drc_pstk_hbrk && $(MAKE) clean
	cd librnd && $(MAKE) clean
#	cd gsch2pcb-rnd && $(MAKE) clean
	cd drc_query && $(MAKE) clean
	cd query && $(MAKE) clean

