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
TOP_DIR = ../..
OBJ_DIR = obj

CPP =   get_lcm.cpp \
	InitLiveConnectClasses.cpp \
	IsLiveConnectEnabled.cpp \
	ShutdownLiveConnect.cpp \
	StartupLiveConnect.cpp \
	WrapJavaObject.cpp 

OBJS =  $(OBJ_DIR)/get_lcm.o \
	$(OBJ_DIR)/InitLiveConnectClasses.o \
	$(OBJ_DIR)/IsLiveConnectEnabled.o \
	$(OBJ_DIR)/ShutdownLiveConnect.o \
	$(OBJ_DIR)/StartupLiveConnect.o \
	$(OBJ_DIR)/WrapJavaObject.o 


DLL = ojiapilcmtests

include ../../build/rules.mk

all: compile link


