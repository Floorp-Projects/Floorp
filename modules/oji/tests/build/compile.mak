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
!if defined(MOZ_DEBUG)
WIN32DIR=WIN32_D.OBJ
!else
WIN32DIR=WIN32_O.OBJ
!endif

INCLUDES = $(INCLUDES) \
	-I. \
	-I$(TOP_DIR)/src/include \
	-I$(JAVAHOME)/include \
	-I$(JAVAHOME)/include/win32 \
	-I$(MOZILLA_HOME)/include \
	-I$(MOZILLA_HOME)/dist/include \
	-I$(MOZILLA_HOME)/dist/$(WIN32DIR)/include \
	-I$(MOZILLA_HOME)/dist/$(WIN32DIR)/include/obsolete 


!if defined(MOZ_DEBUG)
DEFINES=-DXP_WIN -DDEBUG
!else
DEFINES=-DXP_WIN
!endif

CC_OBJ=@cl $(DEFINES) /Zi /nologo $(INCLUDES) /c /Fo$(OBJ_DIR)\ /Fd$(OBJ_DIR)/default.pdb
