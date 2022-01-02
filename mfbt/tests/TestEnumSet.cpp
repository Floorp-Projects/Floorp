/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/BitSet.h"
#include "mozilla/EnumSet.h"
#include "mozilla/Vector.h"

#include <type_traits>

using namespace mozilla;

enum SeaBird {
  PENGUIN,
  ALBATROSS,
  FULMAR,
  PRION,
  SHEARWATER,
  GADFLY_PETREL,
  TRUE_PETREL,
  DIVING_PETREL,
  STORM_PETREL,
  PELICAN,
  GANNET,
  BOOBY,
  CORMORANT,
  FRIGATEBIRD,
  TROPICBIRD,
  SKUA,
  GULL,
  TERN,
  SKIMMER,
  AUK,

  SEA_BIRD_COUNT
};

enum class SmallEnum : uint8_t {
  Foo,
  Bar,
};

enum class BigEnum : uint64_t {
  Foo,
  Bar = 35,
};

template <typename Storage = typename std::make_unsigned<
              typename std::underlying_type<SeaBird>::type>::type>
class EnumSetSuite {
 public:
  using EnumSetSeaBird = EnumSet<SeaBird, Storage>;

  EnumSetSuite()
      : mAlcidae(),
        mDiomedeidae(ALBATROSS),
        mPetrelProcellariidae(GADFLY_PETREL, TRUE_PETREL),
        mNonPetrelProcellariidae(FULMAR, PRION, SHEARWATER),
        mPetrels(GADFLY_PETREL, TRUE_PETREL, DIVING_PETREL, STORM_PETREL) {}

  void runTests() {
    testSize();
    testContains();
    testAddTo();
    testAdd();
    testAddAll();
    testUnion();
    testRemoveFrom();
    testRemove();
    testRemoveAllFrom();
    testRemoveAll();
    testIntersect();
    testInsersection();
    testEquality();
    testDuplicates();
    testIteration();
    testInitializerListConstuctor();
    testBigEnum();
  }

 private:
  void testEnumSetLayout() {
#ifndef DEBUG
    static_assert(sizeof(EnumSet<SmallEnum>) == sizeof(SmallEnum),
                  "EnumSet should be no bigger than the enum by default");
    static_assert(sizeof(EnumSet<SmallEnum, uint32_t>) == sizeof(uint32_t),
                  "EnumSet should be able to have its size overriden.");
    static_assert(std::is_trivially_copyable_v<EnumSet<SmallEnum>>,
                  "EnumSet should be lightweight outside of debug.");
#endif
  }

  void testSize() {
    MOZ_RELEASE_ASSERT(mAlcidae.size() == 0);
    MOZ_RELEASE_ASSERT(mDiomedeidae.size() == 1);
    MOZ_RELEASE_ASSERT(mPetrelProcellariidae.size() == 2);
    MOZ_RELEASE_ASSERT(mNonPetrelProcellariidae.size() == 3);
    MOZ_RELEASE_ASSERT(mPetrels.size() == 4);
  }

  void testContains() {
    MOZ_RELEASE_ASSERT(!mPetrels.contains(PENGUIN));
    MOZ_RELEASE_ASSERT(!mPetrels.contains(ALBATROSS));
    MOZ_RELEASE_ASSERT(!mPetrels.contains(FULMAR));
    MOZ_RELEASE_ASSERT(!mPetrels.contains(PRION));
    MOZ_RELEASE_ASSERT(!mPetrels.contains(SHEARWATER));
    MOZ_RELEASE_ASSERT(mPetrels.contains(GADFLY_PETREL));
    MOZ_RELEASE_ASSERT(mPetrels.contains(TRUE_PETREL));
    MOZ_RELEASE_ASSERT(mPetrels.contains(DIVING_PETREL));
    MOZ_RELEASE_ASSERT(mPetrels.contains(STORM_PETREL));
    MOZ_RELEASE_ASSERT(!mPetrels.contains(PELICAN));
    MOZ_RELEASE_ASSERT(!mPetrels.contains(GANNET));
    MOZ_RELEASE_ASSERT(!mPetrels.contains(BOOBY));
    MOZ_RELEASE_ASSERT(!mPetrels.contains(CORMORANT));
    MOZ_RELEASE_ASSERT(!mPetrels.contains(FRIGATEBIRD));
    MOZ_RELEASE_ASSERT(!mPetrels.contains(TROPICBIRD));
    MOZ_RELEASE_ASSERT(!mPetrels.contains(SKUA));
    MOZ_RELEASE_ASSERT(!mPetrels.contains(GULL));
    MOZ_RELEASE_ASSERT(!mPetrels.contains(TERN));
    MOZ_RELEASE_ASSERT(!mPetrels.contains(SKIMMER));
    MOZ_RELEASE_ASSERT(!mPetrels.contains(AUK));
  }

  void testCopy() {
    EnumSetSeaBird likes = mPetrels;
    likes -= TRUE_PETREL;
    MOZ_RELEASE_ASSERT(mPetrels.size() == 4);
    MOZ_RELEASE_ASSERT(mPetrels.contains(TRUE_PETREL));

    MOZ_RELEASE_ASSERT(likes.size() == 3);
    MOZ_RELEASE_ASSERT(likes.contains(GADFLY_PETREL));
    MOZ_RELEASE_ASSERT(likes.contains(DIVING_PETREL));
    MOZ_RELEASE_ASSERT(likes.contains(STORM_PETREL));
  }

  void testAddTo() {
    EnumSetSeaBird seen = mPetrels;
    seen += CORMORANT;
    seen += TRUE_PETREL;
    MOZ_RELEASE_ASSERT(mPetrels.size() == 4);
    MOZ_RELEASE_ASSERT(!mPetrels.contains(CORMORANT));
    MOZ_RELEASE_ASSERT(seen.size() == 5);
    MOZ_RELEASE_ASSERT(seen.contains(GADFLY_PETREL));
    MOZ_RELEASE_ASSERT(seen.contains(TRUE_PETREL));
    MOZ_RELEASE_ASSERT(seen.contains(DIVING_PETREL));
    MOZ_RELEASE_ASSERT(seen.contains(STORM_PETREL));
    MOZ_RELEASE_ASSERT(seen.contains(CORMORANT));
  }

  void testAdd() {
    EnumSetSeaBird seen = mPetrels + CORMORANT + STORM_PETREL;
    MOZ_RELEASE_ASSERT(mPetrels.size() == 4);
    MOZ_RELEASE_ASSERT(!mPetrels.contains(CORMORANT));
    MOZ_RELEASE_ASSERT(seen.size() == 5);
    MOZ_RELEASE_ASSERT(seen.contains(GADFLY_PETREL));
    MOZ_RELEASE_ASSERT(seen.contains(TRUE_PETREL));
    MOZ_RELEASE_ASSERT(seen.contains(DIVING_PETREL));
    MOZ_RELEASE_ASSERT(seen.contains(STORM_PETREL));
    MOZ_RELEASE_ASSERT(seen.contains(CORMORANT));
  }

  void testAddAll() {
    EnumSetSeaBird procellariidae;
    procellariidae += mPetrelProcellariidae;
    procellariidae += mNonPetrelProcellariidae;
    MOZ_RELEASE_ASSERT(procellariidae.size() == 5);

    // Both procellariidae and mPetrels include GADFLY_PERTEL and TRUE_PETREL
    EnumSetSeaBird procellariiformes;
    procellariiformes += mDiomedeidae;
    procellariiformes += procellariidae;
    procellariiformes += mPetrels;
    MOZ_RELEASE_ASSERT(procellariiformes.size() == 8);
  }

  void testUnion() {
    EnumSetSeaBird procellariidae =
        mPetrelProcellariidae + mNonPetrelProcellariidae;
    MOZ_RELEASE_ASSERT(procellariidae.size() == 5);

    // Both procellariidae and mPetrels include GADFLY_PETREL and TRUE_PETREL
    EnumSetSeaBird procellariiformes = mDiomedeidae + procellariidae + mPetrels;
    MOZ_RELEASE_ASSERT(procellariiformes.size() == 8);
  }

  void testRemoveFrom() {
    EnumSetSeaBird likes = mPetrels;
    likes -= TRUE_PETREL;
    likes -= DIVING_PETREL;
    MOZ_RELEASE_ASSERT(likes.size() == 2);
    MOZ_RELEASE_ASSERT(likes.contains(GADFLY_PETREL));
    MOZ_RELEASE_ASSERT(likes.contains(STORM_PETREL));
  }

  void testRemove() {
    EnumSetSeaBird likes = mPetrels - TRUE_PETREL - DIVING_PETREL;
    MOZ_RELEASE_ASSERT(likes.size() == 2);
    MOZ_RELEASE_ASSERT(likes.contains(GADFLY_PETREL));
    MOZ_RELEASE_ASSERT(likes.contains(STORM_PETREL));
  }

  void testRemoveAllFrom() {
    EnumSetSeaBird likes = mPetrels;
    likes -= mPetrelProcellariidae;
    MOZ_RELEASE_ASSERT(likes.size() == 2);
    MOZ_RELEASE_ASSERT(likes.contains(DIVING_PETREL));
    MOZ_RELEASE_ASSERT(likes.contains(STORM_PETREL));
  }

  void testRemoveAll() {
    EnumSetSeaBird likes = mPetrels - mPetrelProcellariidae;
    MOZ_RELEASE_ASSERT(likes.size() == 2);
    MOZ_RELEASE_ASSERT(likes.contains(DIVING_PETREL));
    MOZ_RELEASE_ASSERT(likes.contains(STORM_PETREL));
  }

  void testIntersect() {
    EnumSetSeaBird likes = mPetrels;
    likes &= mPetrelProcellariidae;
    MOZ_RELEASE_ASSERT(likes.size() == 2);
    MOZ_RELEASE_ASSERT(likes.contains(GADFLY_PETREL));
    MOZ_RELEASE_ASSERT(likes.contains(TRUE_PETREL));
  }

  void testInsersection() {
    EnumSetSeaBird likes = mPetrels & mPetrelProcellariidae;
    MOZ_RELEASE_ASSERT(likes.size() == 2);
    MOZ_RELEASE_ASSERT(likes.contains(GADFLY_PETREL));
    MOZ_RELEASE_ASSERT(likes.contains(TRUE_PETREL));
  }

  void testEquality() {
    EnumSetSeaBird likes = mPetrels & mPetrelProcellariidae;
    MOZ_RELEASE_ASSERT(likes == EnumSetSeaBird(GADFLY_PETREL, TRUE_PETREL));
  }

  void testDuplicates() {
    EnumSetSeaBird likes = mPetrels;
    likes += GADFLY_PETREL;
    likes += TRUE_PETREL;
    likes += DIVING_PETREL;
    likes += STORM_PETREL;
    MOZ_RELEASE_ASSERT(likes.size() == 4);
    MOZ_RELEASE_ASSERT(likes == mPetrels);
  }

  void testIteration() {
    EnumSetSeaBird birds;
    Vector<SeaBird> vec;

    for (auto bird : birds) {
      MOZ_RELEASE_ASSERT(vec.append(bird));
    }
    MOZ_RELEASE_ASSERT(vec.length() == 0);

    birds += DIVING_PETREL;
    birds += GADFLY_PETREL;
    birds += STORM_PETREL;
    birds += TRUE_PETREL;
    for (auto bird : birds) {
      MOZ_RELEASE_ASSERT(vec.append(bird));
    }

    MOZ_RELEASE_ASSERT(vec.length() == 4);
    MOZ_RELEASE_ASSERT(vec[0] == GADFLY_PETREL);
    MOZ_RELEASE_ASSERT(vec[1] == TRUE_PETREL);
    MOZ_RELEASE_ASSERT(vec[2] == DIVING_PETREL);
    MOZ_RELEASE_ASSERT(vec[3] == STORM_PETREL);
  }

  void testInitializerListConstuctor() {
    EnumSetSeaBird empty{};
    MOZ_RELEASE_ASSERT(empty.size() == 0);
    MOZ_RELEASE_ASSERT(empty.isEmpty());

    EnumSetSeaBird someBirds{SKIMMER, GULL, BOOBY};
    MOZ_RELEASE_ASSERT(someBirds.size() == 3);
    MOZ_RELEASE_ASSERT(someBirds.contains(SKIMMER));
    MOZ_RELEASE_ASSERT(someBirds.contains(GULL));
    MOZ_RELEASE_ASSERT(someBirds.contains(BOOBY));
  }

  void testBigEnum() {
    EnumSet<BigEnum> set;
    set += BigEnum::Bar;
    MOZ_RELEASE_ASSERT(set.serialize() ==
                       (uint64_t(1) << uint64_t(BigEnum::Bar)));
  }

  EnumSetSeaBird mAlcidae;
  EnumSetSeaBird mDiomedeidae;
  EnumSetSeaBird mPetrelProcellariidae;
  EnumSetSeaBird mNonPetrelProcellariidae;
  EnumSetSeaBird mPetrels;
};

int main() {
  EnumSetSuite<uint32_t> suite1;
  suite1.runTests();

  EnumSetSuite<BitSet<SEA_BIRD_COUNT>> suite2;
  suite2.runTests();
  return 0;
}
