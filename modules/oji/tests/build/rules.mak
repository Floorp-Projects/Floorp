# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is Sun Microsystems,
# Inc. Portions created by Sun are
# Copyright (C) 1999 Sun Microsystems, Inc. All
# Rights Reserved.
#
# Contributor(s): 

LIBS = nspr4.lib xpcom.lib oji.lib plc4.lib

!if defined(MOZ_DEBUG)
LD_FLAGS = /LIBPATH:$(MOZILLA_HOME)/dist/WIN32_D.OBJ/lib \
	/LIBPATH:$(OBJ_DIR)
!else
LD_FLAGS = /LIBPATH:$(MOZILLA_HOME)/dist/WIN32_O.OBJ/lib \
	/LIBPATH:$(OBJ_DIR)
!endif

ADD_CPP=@echo

MAKE_MAK=perl $(TOP_DIR)/build/make_mak.pl

!if exist(depend.mak)
include depend.mak
!endif

objdir:
!if defined(OBJ_DIR)
!if !exist($(OBJ_DIR))
 	@mkdir $(OBJ_DIR)
!endif
!endif

del_cmd:
!if exist(cmd.mak)
	@del cmd.mak
!endif

compile: objdir del_cmd $(OBJS) 
	@$(MAKE_MAK) cmd.mak CC_OBJ $(TOP_DIR) $(OBJ_DIR) $(INCLUDES)
	@nmake -nologo -f cmd.mak all

link:
	@link /NOLOGO /DEBUG $(LD_FLAGS) $(LIBS) $(OBJS) /DLL /OUT:$(OBJ_DIR)\$(DLL).dll
	@cp obj/$(DLL).dll $(TOP_DIR)/build/bin
!if defined(MOZ_DEBUG)
	@cp obj/$(DLL).dll $(TOP_DIR)/../../../dist/win32_d.obj/bin/components
!else
	@cp obj/$(DLL).dll $(TOP_DIR)/../../../dist/win32_o.obj/bin/components
!endif

make_depends: Makefile.win
!if !exist(depend.mak)
#it's convenient to add one more parameter - COMMON_DEPENDS 
#which contain all common includes for particular interface tests		
	@perl $(TOP_DIR)/build/make_depends.pl $(OBJ_DIR) ADD_CPP $(CPP)
!endif

clean:
	@-del /f/s/q $(OBJ_DIR)\* cmd.mak depend.mak
