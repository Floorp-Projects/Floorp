# This file requires the clang build system, at least for now. So to use this
# Makefile, you should execute the following commands to copy this directory
# into a clang checkout:
#
#   cp -R <this directory> third_party/llvm/tools/clang/tools/chrome-plugin
#   cd third_party/llvm/tools/clang/tools/chrome-plugin
#   make

CLANG_LEVEL := ../..
LIBRARYNAME = FindBadConstructs

LINK_LIBS_IN_SHARED = 0
SHARED_LIBRARY = 1

include $(CLANG_LEVEL)/Makefile

ifeq ($(OS),Darwin)
  LDFLAGS=-Wl,-undefined,dynamic_lookup
endif
