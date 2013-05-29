# System platform

# determine if windows
WIN32 := 0
UNAME := $(shell uname -s)
ifneq (,$(findstring MINGW32_NT,$(UNAME)))
WIN32 = 1
endif
