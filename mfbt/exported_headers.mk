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
  Char16.h \
  CheckedInt.h \
  Compiler.h \
  Constants.h \
  DebugOnly.h \
  EnumSet.h \
  FloatingPoint.h \
  GuardObjects.h \
  HashFunctions.h \
  Likely.h \
  LinkedList.h \
  MathAlgorithms.h \
  MemoryChecking.h \
  MSStdInt.h \
  NullPtr.h \
  PodOperations.h \
  Range.h \
  RangedPtr.h \
  RefPtr.h \
  Scoped.h \
  SHA1.h \
  SplayTree.h \
  StandardInteger.h \
  ThreadLocal.h \
  TypedEnum.h \
  Types.h \
  TypeTraits.h \
  Util.h \
  WeakPtr.h \
  $(NULL)
