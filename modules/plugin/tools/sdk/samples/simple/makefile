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

# This makefile contains some of our defines for the compiler:
#
# XP_UNIX       This needs to get defined for npapi.h on unix platforms.

PLUGIN_DEFINES = -DXP_UNIX -I../../include -I../include
CCC = gcc
OPTIMIZER = -g
CFLAGS = $(OPTIMIZER) $(PLUGIN_DEFINES) -I. -I/usr/include

CPPSRCS = ../common/npn_gate.cpp \
     ../common/npp_gate.cpp \
     ../common/np_entry.cpp \
     nsScriptablePeer.cpp \
     plugin.cpp

OBJS = $(strip $(CPPSRCS:.cpp=.o))

%.o: %.cpp makefile
	$(CCC) -o $@ -c $(CFLAGS) $<

SHAREDTARGET=npsimple.so

default all: $(SHAREDTARGET)

$(SHAREDTARGET): $(OBJS)
	$(CC) -shared -o $(SHAREDTARGET) $(OBJS) $(LDFLAGS)

clean:
	$(RM) $(OBJS) $(SHAREDTARGET)

