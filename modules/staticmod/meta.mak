#!nmake
#
# The contents of this file are subject to the Netscape Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation. All
# Rights Reserved.
#
# Contributor(s): 
#

DEPTH=..\..

include <$(DEPTH)/config/config.mak>

MAKE_OBJ_TYPE   = DLL
DLLNAME         = $(META_MODULE).dll
DLL             = .\$(OBJDIR)\$(DLLNAME)

LINK_COMP_NAMES = $(DIST)\$(META_MODULE)-link-comp-names
LINK_COMPS      = $(DIST)\$(META_MODULE)-link-comps
LINK_LIBS       = $(DIST)\$(META_MODULE)-link-libs
SEDCMDS         = nsMetaModule_$(META_MODULE).cpp.sed

EXTRA_LIBS_LIST_FILE = $(OBJDIR)\$(META_MODULE)-libs.txt

GARBAGE         = $(GARBAGE) $(SEDCMDS) $(LIBFILE) nsMetaModule_$(META_MODULE).cpp

CPP_OBJS        = .\$(OBJDIR)\nsMetaModule_$(META_MODULE).obj

# XXX Lame! This is currently the superset of all static libraries not
# explicitly made part of the META_MODULE.
LLIBS           = $(DIST)\lib\gkgfx.lib         \
                  $(DIST)\lib\rdfutil_s.lib     \
                  $(DIST)\lib\js3250.lib        \
                  $(DIST)\lib\xpcom.lib         \
                  $(LIBNSPR)

!ifdef MOZ_GECKO_DLL
LLIBS           = $(LLIBS)                      \
                  $(DIST)\lib\png.lib           \
                  $(DIST)\lib\mng.lib           \
                  $(DIST)\lib\util.lib          \
                  $(DIST)\lib\expat.lib         \
                  $(DIST)\lib\nsldap32v40.lib   

WIN_LIBS        = comctl32.lib  \
                  comdlg32.lib  \
                  uuid.lib      \
                  ole32.lib     \
                  shell32.lib   \
                  oleaut32.lib  \
                  version.lib
!endif

include <$(DEPTH)/config/rules.mak>

#
# Create the sed commands that are used translate nsMetaModule_(foo).cpp.in
# into nsMetaModule_(foo).cpp, using the component names,
#
$(SEDCMDS): $(LINK_COMP_NAMES)
        echo +++make: Creating $@
        rm -f $@
        echo s/%COMPONENT_NS_GET_MODULE%/\>> $@
        sed -e "s/\(.*\)/REGISTER_MODULE_USING(\1_NSGetModule);\\\/" $(LINK_COMP_NAMES) >> $@
        echo />> $@
        echo s/%COMPONENT_LIST%/\>> $@
        sed -e "s/\(.*\)/{ \1_NSGM_comps, \1_NSGM_comp_count },\\\/" $(LINK_COMP_NAMES) >> $@
        echo />> $@
        echo s/%DECLARE_COMPONENT_LIST%/\>> $@
        sed -e "s/\(.*\)/extern \"C\" nsresult \1_NSGetModule(nsIComponentManager*, nsIFile*, nsIModule**); extern nsModuleComponentInfo* \1_NSGM_comps; extern PRUint32 \1_NSGM_comp_count;\\\/" $(LINK_COMP_NAMES) >> $@
        echo />> $@

#
# Create nsMetaModule_(foo).cpp from nsMetaModule_(foo).cpp.in
#
nsMetaModule_$(META_MODULE).cpp: nsMetaModule_$(META_MODULE).cpp.in $(SEDCMDS)
        echo +++make: Creating $@
        rm -f $@
        sed -f $(SEDCMDS) nsMetaModule_$(META_MODULE).cpp.in > $@

#
# If no link components file has been created, make an empty one now.
#
$(LINK_COMPS):
        echo +++ make: Creating empty link components file: $@
        touch $@

#
# If no link libs file has been created, make an empty one now.
#
$(LINK_LIBS):
        echo +++ make: Creating empty link libraries file: $@
        touch $@

#
# Create a list of libraries that we'll need to link against from the
# component list and the ``export library'' list
#
$(EXTRA_LIBS_LIST_FILE): $(LINK_COMPS) $(LINK_LIBS)
        echo +++ make: Creating list of link libraries: $@
        rm -f $@
        sed -e "s/\(.*\)/$(DIST:\=\\\)\\\lib\\\\\1.lib/" $(LINK_COMPS)  > $@
        sed -e "s/\(.*\)/$(DIST:\=\\\)\\\lib\\\\\1.lib/" $(LINK_LIBS)  >> $@


# XXX this is a hack. The ``gecko'' meta-module consists
# of all the static components linked into a DLL instead
# of an executable. To make this work, we'll copy the
# statically linked libs, components, and component names
# to the right file. This relies on the fact that the
# modules/staticmod directory gets built after all the other
# directories in the tree are processed.
!if defined(MOZ_GECKO_DLL) && "$(META_MODULE)" == "gecko"
export::
        copy $(FINAL_LINK_LIBS) $(DIST)\$(META_MODULE)-link-libs
        copy $(FINAL_LINK_COMPS) $(DIST)\$(META_MODULE)-link-comps
        copy $(FINAL_LINK_COMP_NAMES) $(DIST)\$(META_MODULE)-link-comp-names
!endif

install:: $(DLL)
        $(MAKE_INSTALL) $(DLL) $(DIST)/bin/components

clobber::
        rm -f $(DIST)/bin/components/$(DLLNAME)

