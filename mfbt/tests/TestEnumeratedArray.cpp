/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EnumeratedArray.h"

enum class AnimalSpecies
{
  Cow,
  Sheep,
  Pig,
  Count
};

void TestInitialValueByConstructor()
{
  using namespace mozilla;
  // Style 1
  EnumeratedArray<AnimalSpecies, AnimalSpecies::Count, int> headCount(1, 2, 3);
  MOZ_RELEASE_ASSERT(headCount[AnimalSpecies::Cow] == 1);
  MOZ_RELEASE_ASSERT(headCount[AnimalSpecies::Sheep] == 2);
  MOZ_RELEASE_ASSERT(headCount[AnimalSpecies::Pig] == 3);
  // Style 2
  EnumeratedArray<AnimalSpecies, AnimalSpecies::Count, int> headCount2{5, 6, 7};
  MOZ_RELEASE_ASSERT(headCount2[AnimalSpecies::Cow] == 5);
  MOZ_RELEASE_ASSERT(headCount2[AnimalSpecies::Sheep] == 6);
  MOZ_RELEASE_ASSERT(headCount2[AnimalSpecies::Pig] == 7);
  // Style 3
  EnumeratedArray<AnimalSpecies, AnimalSpecies::Count, int> headCount3({8, 9, 10});
  MOZ_RELEASE_ASSERT(headCount3[AnimalSpecies::Cow] == 8);
  MOZ_RELEASE_ASSERT(headCount3[AnimalSpecies::Sheep] == 9);
  MOZ_RELEASE_ASSERT(headCount3[AnimalSpecies::Pig] == 10);
}

int
main()
{
  TestInitialValueByConstructor();
  return 0;
}