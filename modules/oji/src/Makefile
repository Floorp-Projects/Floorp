#//------------------------------------------------------------------------
#//
#// Makefile to build OJI
#//
#//------------------------------------------------------------------------

DEPTH        = ../../..

MODULE		= ojiimpl

ifdef MOZ_OJI
LIBRARY_NAME	= oji

CPPSRCS         = jvmmgr.cpp \
                  scd.cpp

REQUIRES	= img java js lay layer plugin plugimpl pref style util xpcom raptor oji
endif # MOZ_OJI

EXPORTS         = jvmmgr.h

include $(DEPTH)/config/rules.mk

