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
#

CFLAGS         +=-D_IMPL_NS_XPFC

LD_LIBS += \
	raptorbase \
	raptorhtmlpars \
	$(NATIVE_RAPTOR_WEB) \
	$(NATIVE_RAPTOR_GFX) \
	$(NATIVE_RAPTOR_WIDGET) \
	xpcom$(MOZ_BITS) \
	netlib \
	$(NATIVE_MSG_COMM_LIB) \
	$(NATIVE_SMTP_LIB) \
	$(XP_REG_LIB)

AR_LIBS += \
              core \
              core_$(PLATFORM_DIRECTORY) \
              canvas \
              command \
              chrome \
              chrome_$(PLATFORM_DIRECTORY) \
			  dialog \
              layout \
              commandserver \
              observer \
              parser \
              shell \
              util \
              toolkit \
			  widget \
              xpfc_user \
			  xpfc_msg \
              $(NULL)

OS_LIBS += $(GUI_LIBS) $(MATH_LIB)

EXTRA_LIBS += $(NSPR_LIBS)

