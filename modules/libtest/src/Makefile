#//------------------------------------------------------------------------
#//
#// Makefile to build LIBTEST
#//
#//------------------------------------------------------------------------

DEPTH        = ../../..

include $(DEPTH)/config/config.mk

MODULE		= libtest

ifdef MOZ_LIBTEST
LIBRARY_NAME	= libtest

CPPSRCS         = nsTestEventsApi.cpp \
                  nsTestQueryApi.cpp

REQUIRES	= xpcom libtest layout
endif # MOZ_LIBTEST

EXPORTS         = nsTestEventsApi.h \
		  nsTestQueryApi.h

include $(DEPTH)/config/rules.mk

