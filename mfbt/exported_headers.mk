# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This file defines the headers exported by mfbt.  It is included by mfbt
# itself and by the JS engine, which, when built standalone, must install
# mfbt's exported headers itself.

EXPORTS_mozilla_DEST := $(DIST)/include/mozilla
EXPORTS_mozilla_TARGET := export
INSTALL_TARGETS += EXPORTS_mozilla
EXPORTS_mozilla_FILES += \
  Alignment.h \
  AllocPolicy.h \
  Array.h \
  Assertions.h \
  Atomics.h \
  Attributes.h \
  BloomFilter.h \
  Casting.h \
  Char16.h \
  CheckedInt.h \
  Compiler.h \
  Constants.h \
  DebugOnly.h \
  decimal/Decimal.h \
  Endian.h \
  EnumSet.h \
  FloatingPoint.h \
  GuardObjects.h \
  HashFunctions.h \
  IntegerPrintfMacros.h \
  Likely.h \
  LinkedList.h \
  MathAlgorithms.h \
  Maybe.h \
  MemoryChecking.h \
  MemoryReporting.h \
  MSIntTypes.h \
  Move.h \
  NullPtr.h \
  NumericLimits.h \
  PodOperations.h \
  Poison.h \
  Range.h \
  RangedPtr.h \
  ReentrancyGuard.h \
  RefPtr.h \
  Scoped.h \
  SHA1.h \
  SplayTree.h \
  TemplateLib.h \
  ThreadLocal.h \
  TypedEnum.h \
  Types.h \
  TypeTraits.h \
  Util.h \
  Vector.h \
  WeakPtr.h \
  $(NULL)
