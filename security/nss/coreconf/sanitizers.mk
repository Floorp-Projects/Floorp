# Address Sanitizer support; include this in OS-specific .mk files
# *after* defining the variables that are appended to here.

ifeq ($(USE_ASAN), 1)
SANITIZER_FLAGS_COMMON = -fsanitize=address

ifeq ($(USE_UBSAN), 1)
SANITIZER_FLAGS_COMMON += -fsanitize=undefined -fno-sanitize-recover=undefined
endif

ifeq ($(FUZZ), 1)
SANITIZER_FLAGS_COMMON += -fsanitize-coverage=edge
endif

SANITIZER_FLAGS_COMMON += $(EXTRA_SANITIZER_FLAGS)
SANITIZER_CFLAGS = $(SANITIZER_FLAGS_COMMON)
SANITIZER_LDFLAGS = $(SANITIZER_FLAGS_COMMON)
OS_CFLAGS += $(SANITIZER_CFLAGS)
LDFLAGS += $(SANITIZER_LDFLAGS)

# ASan needs frame pointers to save stack traces for allocation/free sites.
# (Warning: some platforms, like ARM Linux in Thumb mode, don't have useful
# frame pointers even with this option.)
SANITIZER_CFLAGS += -fno-omit-frame-pointer -fno-optimize-sibling-calls

ifdef BUILD_OPT
# You probably want to be able to get debug info for failures, even with an
# optimized build.
OPTIMIZER += -g
else
# Try maintaining reasonable performance, ASan and UBSan slow things down.
OPTIMIZER += -O1
endif

endif
