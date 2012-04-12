ifndef MFBT_ROOT
$(error Before including this file, you must define MFBT_ROOT to point to \
the MFBT source directory)
endif

CPPSRCS += \
  Assertions.cpp \
  HashFunctions.cpp \
  $(NULL)

# Imported double-conversion sources.
VPATH += $(MFBT_ROOT)/double-conversion \
  $(NULL)

CPPSRCS += \
  bignum-dtoa.cc \
  bignum.cc \
  cached-powers.cc \
  diy-fp.cc \
  double-conversion.cc \
  fast-dtoa.cc \
  fixed-dtoa.cc \
  strtod.cc \
  $(NULL)
