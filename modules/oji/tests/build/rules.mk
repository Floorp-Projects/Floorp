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
LIBS = -lnspr4 -lxpcom -loji -lplc4
LD_PATH = -L$(MOZILLA_HOME)/dist/lib  -L$(MOZILLA_HOME)/dist/bin/components -L$(OBJ_DIR)
LD_FLAGS_GCC=-shared
LD_FLAGS_WS=-G

ADD_CPP=@echo

MAKE_MAK=perl $(TOP_DIR)/build/make_mak.pl

MAKE_DEPENDS=perl $(TOP_DIR)/build/make_depends.pl

include depend.mak

make_depends: Makefile
	@if test ! -s depend.mak; then \
		 $(MAKE_DEPENDS) $(OBJ_DIR) ADD_CPP $(CPP); \
	fi

objdir:
	@if test 'x$(OBJ_DIR)' != 'x'; then \
		if test ! -d $(OBJ_DIR); then \
	 	mkdir $(OBJ_DIR); \
		fi; \
	fi

del_cmd:
	@if test -f cmd.mak; then \
		rm cmd.mak; \
	fi

compile: objdir del_cmd $(OBJS)
	@-mv *.o $(OBJ_DIR) 2> /dev/null
	@$(MAKE_MAK) cmd.mak CC_OBJ $(TOP_DIR) $(OBJ_DIR) $(INCLUDES)
	@make -f cmd.mak all
	@-mv *.o $(OBJ_DIR) 2> /dev/null


link: 
	@if test 'x$(USE_SUN_WS)' = 'x1'; then \
		$(CC) $(LD_FLAGS_WS) $(LIBS) $(OBJS) $(LD_PATH) -o $(OBJ_DIR)/lib$(DLL).so; \
	else \
		$(CC) $(LD_FLAGS_GCC) $(LIBS) $(OBJS) $(LD_PATH) -o $(OBJ_DIR)/lib$(DLL).so; \
	fi;
	@cp obj/lib$(DLL).so $(TOP_DIR)/build/bin/.
	@cp obj/lib$(DLL).so $(TOP_DIR)/../../../dist/bin/components/.





