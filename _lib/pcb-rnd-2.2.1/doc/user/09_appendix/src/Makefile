# NOTE: aagraph is at svn://repo.hu/projects/aagraph/trunk

PCBRND = ../../../../src/pcb-rnd
CLEANFILES=../action_details.html ../action_reference.html ../formats.html ../bridges.svg ../dialogs.html

all: $(CLEANFILES)

../action_details.html: ./action_compiler.sh ../action_src/*.html
	./action_compiler.sh ../action_src/*.html > ../action_details.html

../action_reference.html : ./dump_actions_to_html.sh $(PCBRND) ../action_details.html
	./dump_actions_to_html.sh > ../action_reference.html

../bridges.svg: bridges.txt
	aagraph -Tsvg bridges.txt > ../bridges.svg

../formats.html: gen_formats.sh ../../../../src_plugins/io_*/*.pup ../../../../src_plugins/import_*/*.pup ../../../../src_plugins/export_*/*.pup
	./gen_formats.sh > ../formats.html

../dialogs.html: $(PCBRND) ./gen_dialogs.sh ./dialog_extra.awk
	./gen_dialogs.sh > ../dialogs.html

clean:  
	rm $(CLEANFILES)
