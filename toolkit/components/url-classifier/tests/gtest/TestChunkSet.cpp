/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>

#include <set>

#include "ChunkSet.h"
#include "mozilla/ArrayUtils.h"

#include "Common.h"

TEST(UrlClassifierChunkSet, Empty)
{
  mozilla::safebrowsing::ChunkSet chunkSet;
  mozilla::safebrowsing::ChunkSet removeSet;

  removeSet.Set(0);

  ASSERT_FALSE(chunkSet.Has(0));
  ASSERT_FALSE(chunkSet.Has(1));
  ASSERT_TRUE(chunkSet.Remove(removeSet) == NS_OK);
  ASSERT_TRUE(chunkSet.Length() == 0);

  chunkSet.Set(0);

  ASSERT_TRUE(chunkSet.Has(0));
  ASSERT_TRUE(chunkSet.Length() == 1);
  ASSERT_TRUE(chunkSet.Remove(removeSet) == NS_OK);
  ASSERT_FALSE(chunkSet.Has(0));
  ASSERT_TRUE(chunkSet.Length() == 0);
}

TEST(UrlClassifierChunkSet, Main)
{
  static int testVals[] = {2, 1, 5, 6, 8, 7, 14, 10, 12, 13};

  mozilla::safebrowsing::ChunkSet chunkSet;

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(testVals); i++) {
    chunkSet.Set(testVals[i]);
  }

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(testVals); i++) {
    ASSERT_TRUE(chunkSet.Has(testVals[i]));
  }

  ASSERT_FALSE(chunkSet.Has(3));
  ASSERT_FALSE(chunkSet.Has(4));
  ASSERT_FALSE(chunkSet.Has(9));
  ASSERT_FALSE(chunkSet.Has(11));

  ASSERT_TRUE(chunkSet.Length() == MOZ_ARRAY_LENGTH(testVals));
}

TEST(UrlClassifierChunkSet, Merge)
{
  static int testVals[] = {2, 1, 5, 6, 8, 7, 14, 10, 12, 13};
  static int mergeVals[] = {9, 3, 4, 20, 14, 16};

  mozilla::safebrowsing::ChunkSet chunkSet;
  mozilla::safebrowsing::ChunkSet mergeSet;

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(testVals); i++) {
    chunkSet.Set(testVals[i]);
  }

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(mergeVals); i++) {
    mergeSet.Set(mergeVals[i]);
  }

  chunkSet.Merge(mergeSet);

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(testVals); i++) {
    ASSERT_TRUE(chunkSet.Has(testVals[i]));
  }
  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(mergeVals); i++) {
    ASSERT_TRUE(chunkSet.Has(mergeVals[i]));
  }

  // -1 because 14 is duplicated in both sets
  ASSERT_TRUE(chunkSet.Length() ==
              MOZ_ARRAY_LENGTH(testVals) + MOZ_ARRAY_LENGTH(mergeVals) - 1);

  ASSERT_FALSE(chunkSet.Has(11));
  ASSERT_FALSE(chunkSet.Has(15));
  ASSERT_FALSE(chunkSet.Has(17));
  ASSERT_FALSE(chunkSet.Has(18));
  ASSERT_FALSE(chunkSet.Has(19));
}

TEST(UrlClassifierChunkSet, Merge2)
{
  static int testVals[] = {2, 1, 5, 6, 8, 7, 14, 10, 12, 13};
  static int mergeVals[] = {9, 3, 4, 20, 14, 16};
  static int mergeVals2[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

  mozilla::safebrowsing::ChunkSet chunkSet;
  mozilla::safebrowsing::ChunkSet mergeSet;
  mozilla::safebrowsing::ChunkSet mergeSet2;

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(testVals); i++) {
    chunkSet.Set(testVals[i]);
  }

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(mergeVals); i++) {
    mergeSet.Set(mergeVals[i]);
  }
  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(mergeVals2); i++) {
    mergeSet2.Set(mergeVals2[i]);
  }

  chunkSet.Merge(mergeSet);
  chunkSet.Merge(mergeSet2);

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(testVals); i++) {
    ASSERT_TRUE(chunkSet.Has(testVals[i]));
  }
  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(mergeVals); i++) {
    ASSERT_TRUE(chunkSet.Has(mergeVals[i]));
  }
  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(mergeVals2); i++) {
    ASSERT_TRUE(chunkSet.Has(mergeVals2[i]));
  }

  ASSERT_FALSE(chunkSet.Has(15));
  ASSERT_FALSE(chunkSet.Has(17));
  ASSERT_FALSE(chunkSet.Has(18));
  ASSERT_FALSE(chunkSet.Has(19));
}

TEST(UrlClassifierChunkSet, Stress)
{
  mozilla::safebrowsing::ChunkSet chunkSet;
  mozilla::safebrowsing::ChunkSet mergeSet;
  std::set<int> refSet;
  std::set<int> refMergeSet;
  static const int TEST_ITERS = 7000;
  static const int REMOVE_ITERS = 3000;
  static const int TEST_RANGE = 10000;

  // Construction by Set
  for (int i = 0; i < TEST_ITERS; i++) {
    int chunk = rand() % TEST_RANGE;
    chunkSet.Set(chunk);
    refSet.insert(chunk);
  }

  // Same elements as reference set
  for (auto it = refSet.begin(); it != refSet.end(); ++it) {
    ASSERT_TRUE(chunkSet.Has(*it));
  }

  // Hole punching via Remove
  for (int i = 0; i < REMOVE_ITERS; i++) {
    int chunk = rand() % TEST_RANGE;
    mozilla::safebrowsing::ChunkSet helpChunk;
    helpChunk.Set(chunk);

    chunkSet.Remove(helpChunk);
    refSet.erase(chunk);

    ASSERT_FALSE(chunkSet.Has(chunk));
  }

  // Should have chunks present in reference set
  // Should not have chunks absent in reference set
  for (int it = 0; it < TEST_RANGE; ++it) {
    auto found = refSet.find(it);
    if (chunkSet.Has(it)) {
      ASSERT_FALSE(found == refSet.end());
    } else {
      ASSERT_TRUE(found == refSet.end());
    }
  }

  // Construct set to merge with
  for (int i = 0; i < TEST_ITERS; i++) {
    int chunk = rand() % TEST_RANGE;
    mergeSet.Set(chunk);
    refMergeSet.insert(chunk);
  }

  // Merge set constructed correctly
  for (auto it = refMergeSet.begin(); it != refMergeSet.end(); ++it) {
    ASSERT_TRUE(mergeSet.Has(*it));
  }

  mozilla::safebrowsing::ChunkSet origSet;
  origSet = chunkSet.InfallibleClone();

  chunkSet.Merge(mergeSet);
  refSet.insert(refMergeSet.begin(), refMergeSet.end());

  // Check for presence of elements from both source
  // Should not have chunks absent in reference set
  for (int it = 0; it < TEST_RANGE; ++it) {
    auto found = refSet.find(it);
    if (chunkSet.Has(it)) {
      ASSERT_FALSE(found == refSet.end());
    } else {
      ASSERT_TRUE(found == refSet.end());
    }
  }

  // Unmerge
  chunkSet.Remove(origSet);
  for (int it = 0; it < TEST_RANGE; ++it) {
    if (origSet.Has(it)) {
      ASSERT_FALSE(chunkSet.Has(it));
    } else if (mergeSet.Has(it)) {
      ASSERT_TRUE(chunkSet.Has(it));
    }
  }
}

TEST(UrlClassifierChunkSet, RemoveClear)
{
  static int testVals[] = {2, 1, 5, 6, 8, 7, 14, 10, 12, 13};
  static int mergeVals[] = {3, 4, 9, 16, 20};

  mozilla::safebrowsing::ChunkSet chunkSet;
  mozilla::safebrowsing::ChunkSet mergeSet;
  mozilla::safebrowsing::ChunkSet removeSet;

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(testVals); i++) {
    chunkSet.Set(testVals[i]);
    removeSet.Set(testVals[i]);
  }

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(mergeVals); i++) {
    mergeSet.Set(mergeVals[i]);
  }

  ASSERT_TRUE(chunkSet.Merge(mergeSet) == NS_OK);
  ASSERT_TRUE(chunkSet.Remove(removeSet) == NS_OK);

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(mergeVals); i++) {
    ASSERT_TRUE(chunkSet.Has(mergeVals[i]));
  }
  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(testVals); i++) {
    ASSERT_FALSE(chunkSet.Has(testVals[i]));
  }

  chunkSet.Clear();

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(mergeVals); i++) {
    ASSERT_FALSE(chunkSet.Has(mergeVals[i]));
  }
}

TEST(UrlClassifierChunkSet, Serialize)
{
  static int testVals[] = {2, 1, 5, 6, 8, 7, 14, 10, 12, 13};
  static int mergeVals[] = {3, 4, 9, 16, 20};

  mozilla::safebrowsing::ChunkSet chunkSet;
  mozilla::safebrowsing::ChunkSet mergeSet;

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(testVals); i++) {
    chunkSet.Set(testVals[i]);
  }

  for (size_t i = 0; i < MOZ_ARRAY_LENGTH(mergeVals); i++) {
    mergeSet.Set(mergeVals[i]);
  }

  chunkSet.Merge(mergeSet);

  nsAutoCString mergeResult;
  chunkSet.Serialize(mergeResult);

  printf("mergeResult: %s\n", mergeResult.get());

  nsAutoCString expected(NS_LITERAL_CSTRING("1-10,12-14,16,20"));

  ASSERT_TRUE(mergeResult.Equals(expected));
}
