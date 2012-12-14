# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file defines the headers exported by mfbt.  It is included by mfbt
# itself and by the JS engine, which, when built standalone, must install
# mfbt's exported headers itself.

EXPORTS_NAMESPACES += mozilla

EXPORTS_mozilla += \
  Assertions.h \
  Attributes.h \
  BloomFilter.h \
  CheckedInt.h \
  Constants.h \
  DebugOnly.h \
  EnumSet.h \
  FloatingPoint.h \
  GuardObjects.h \
  HashFunctions.h \
  Likely.h \
  LinkedList.h \
  MathAlgorithms.h \
  MSStdInt.h \
  NullPtr.h \
  RangedPtr.h \
  RefPtr.h \
  Scoped.h \
  StandardInteger.h \
  SHA1.h \
  ThreadLocal.h \
  TypeTraits.h \
  Types.h \
  Util.h \
  WeakPtr.h \
  $(NULL)
