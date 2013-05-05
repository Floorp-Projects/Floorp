# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

ifndef MFBT_ROOT
$(error Before including this file, you must define MFBT_ROOT to point to \
the MFBT source directory)
endif

CPPSRCS += \
  HashFunctions.cpp \
  SHA1.cpp \
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

# Imported decimal sources.
VPATH += $(MFBT_ROOT)/decimal \
  $(NULL)

CPPSRCS += \
  Decimal.cpp

