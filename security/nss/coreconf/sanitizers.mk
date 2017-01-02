# Address Sanitizer support; include this in OS-specific .mk files
# *after* defining the variables that are appended to here.

ifeq ($(USE_ASAN), 1)
SANITIZER_FLAGS_COMMON = -fsanitize=address $(EXTRA_SANITIZER_FLAGS)
SANITIZER_CFLAGS = $(SANITIZER_FLAGS_COMMON)
SANITIZER_LDFLAGS = $(SANITIZER_FLAGS_COMMON)
OS_CFLAGS += $(SANITIZER_CFLAGS)
LDFLAGS += $(SANITIZER_LDFLAGS)

# ASan needs frame pointers to save stack traces for allocation/free sites.
# (Warning: some platforms, like ARM Linux in Thumb mode, don't have useful
# frame pointers even with this option.)
SANITIZER_CFLAGS += -fno-omit-frame-pointer -fno-optimize-sibling-calls

# You probably want to be able to get debug info for failures, even with an
# optimized build.
OPTIMIZER += -g
endif
