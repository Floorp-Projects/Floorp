# decodes resource files from AppleSingle to Resource Manager format.

ASDECODE = ../../../../../dist/bin/asdecode
RESOURCE_FILES = _Dialogs.rsrc _Strings.rsrc

all:  $(RESOURCE_FILES)

_Dialogs.rsrc:  Dialogs.rsrc
	$(ASDECODE) Dialogs.rsrc _Dialogs.rsrc

_Strings.rsrc:  Strings.rsrc
	$(ASDECODE) Strings.rsrc _Strings.rsrc

clean:
	rm -f $(RESOURCE_FILES)
