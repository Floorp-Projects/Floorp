#!nmake
#
# The contents of this file are subject to the Netscape Public License
# Version 1.0 (the "NPL"); you may not use this file except in
# compliance with the NPL.  You may obtain a copy of the NPL at
# http://www.mozilla.org/NPL/
#
# Software distributed under the NPL is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
# for the specific language governing rights and limitations under the
# NPL.
#
# The Initial Developer of this code under the NPL is Netscape
# Communications Corporation.  Portions created by Netscape are
# Copyright (C) 1998 Netscape Communications Corporation.  All Rights
# Reserved.

DEPTH=../../..

include $(DEPTH)/config/config.mk

MODULE=raptor 

LIBRARY_NAME=raptorplugin

EXPORTS =	\
    nsPluginsCID.h  \
	nsIPluginHost.h \
	nsIPluginInstanceOwner.h

CPPSRCS =                              \
    nsPluginHostImpl.cpp  \
    nsPluginFactory.cpp  \
    ns4xPlugin.cpp  \
    ns4xPluginInstance.cpp  \
    ns4xPluginStream.cpp  \
    nsPluginInstancePeer.cpp  \
    nsPluginStreamPeer.cpp  \
    nsMalloc.cpp  \
    $(NULL)

REQUIRES = raptor plugin java xpcom netlib libplc21 oji

LCFLAGS =-D_IMPL_NS_PLUGIN

TARGETS = $(LIBRARY)

INCLUDES += -I.

include $(DEPTH)/config/rules.mk
