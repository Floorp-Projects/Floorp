#//------------------------------------------------------------------------
#//
#// Makefile to build OJI
#//
#//------------------------------------------------------------------------

DEPTH        = ../../..

include $(DEPTH)/config/config.mk

MODULE		= ojiimpl

ifdef MOZ_OJI
LIBRARY_NAME	= oji

CPPSRCS         = jvmmgr.cpp \
                  scd.cpp  \
                  ProxyJNI.cpp \
                  nsCSecurityContext.cpp

REQUIRES	= img java js lay layer plugin plugimpl pref style util xpcom raptor oji caps
LCFLAGS += -DJSJDLL=\"$(JSJDLL)\" 
INCLUDES += 		-I$(DIST)/include/private

endif # MOZ_OJI

EXPORTS         = jvmmgr.h

include $(DEPTH)/config/rules.mk

