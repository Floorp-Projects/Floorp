#! gmake

ifdef MOZ_TREX
NS_DEPTH=../../..
else
NS_DEPTH=../..
endif

DEPTH = $(NS_DEPTH)

include $(NS_DEPTH)/config/config.mk

ifdef MOZ_TREX
DIRS    = libcal
else
DIRS    = libjulian
endif


include $(NS_DEPTH)/config/rules.mk
