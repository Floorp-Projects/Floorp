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
TOP_DIR= ../..

OBJ_DIR= obj

CPP=get_jm.cpp \
	CreateProxyJNI.cpp \
	GetProxyJNI.cpp \
	IsJavaEnabled.cpp \
	ShowJavaConsole.cpp

OBJS=  $(OBJ_DIR)/get_jm.o \
	$(OBJ_DIR)/CreateProxyJNI.o \
	$(OBJ_DIR)/GetProxyJNI.o   \
	$(OBJ_DIR)/IsJavaEnabled.o   \
	$(OBJ_DIR)/ShowJavaConsole.o

DLL= ojiapijmtests

include ../../build/rules.mk

all: compile link
