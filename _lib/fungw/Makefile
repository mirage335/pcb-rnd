all:
	cd libfungw && make all
	cd libfungwbind && make all
	cd example && make all
	cd doc && make all

clean:
	cd example && make clean
	cd libfungw && make clean
	cd libfungwbind && make clean
	cd doc && make clean
	-cd src_3rd/genht && make clean

distclean:
	cd example && make distclean
	cd libfungw && make distclean
	cd libfungwbind && make distclean
	cd doc && make distclean
	cd scconfig && make distclean
	-cd src_3rd/genht && make clean

install:
	cd libfungw && make install
	cd libfungwbind && make install
	cd doc && make install


linstall:
	cd libfungw && make linstall
	cd libfungwbind && make linstall
	cd doc && make linstall

uninstall:
	cd libfungw && make uninstall
	cd libfungwbind && make uninstall
	cd doc && make uninstall
