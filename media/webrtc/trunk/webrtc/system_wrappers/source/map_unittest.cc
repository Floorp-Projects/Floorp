/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/system_wrappers/interface/map_wrapper.h"

#include "testing/gtest/include/gtest/gtest.h"

using ::webrtc::MapWrapper;
using ::webrtc::MapItem;

const int kNumberOfElements = 10;

int* ItemPointer(MapItem* item) {
  if (item == NULL) {
    return NULL;
  }
  return reinterpret_cast<int*>(item->GetItem());
}

bool DeleteItemContent(MapItem* item) {
  if (item == NULL) {
    return false;
  }
  int* value_ptr = ItemPointer(item);
  delete value_ptr;
  return true;
}

int ItemValue(MapItem* item) {
  if (item == NULL) {
    return -1;
  }
  const int* value_ptr = ItemPointer(item);
  if (value_ptr == 0) {
    return -1;
  }
  return *value_ptr;
}

void PrintToConsole(const char* message, bool supress) {
  if (supress) {
    return;
  }
  printf("%s", message);
}

bool CreateAscendingMap(MapWrapper* ascending_map) {
  int* insert_value = NULL;
  for (int i = 0; i < kNumberOfElements; ++i) {
    insert_value = new int;
    if (insert_value == NULL) {
      return false;
    }
    *insert_value = i;
    if (0 != ascending_map->Insert(
          i,
          reinterpret_cast<void*>(insert_value))) {
      return false;
    }
  }
  return true;
}

bool ClearMap(MapWrapper* clear_map) {
  bool success = true;
  while (clear_map->Size() != 0) {
    MapItem* remove_item = clear_map->First();
    if (remove_item == NULL) {
      return false;
    }
    if (!DeleteItemContent(remove_item)) {
      success = false;
    }
    if (clear_map->Erase(remove_item) != 0) {
      return false;
    }
  }
  return success;
}

void PrintMapItem(MapItem* item, bool supress) {
  const int id = item->GetId();
  const int value = ItemValue(item);
  char print_buffer[16];
  sprintf(print_buffer, "(%3i,%3i) ", id, value);
  PrintToConsole(print_buffer, supress);
}

// Succeeds only if all the items were printed.
bool PrintMap(const MapWrapper& print_map, bool supress) {
  const int elements_to_print = print_map.Size();
  int elements_printed = 0;
  MapItem* item = print_map.First();
  PrintToConsole("[", supress);
  while (item != NULL) {
    PrintMapItem(item, supress);
    ++elements_printed;
    item = print_map.Next(item);
  }
  PrintToConsole("]\n", supress);
  return elements_printed == elements_to_print;
}

// Succeeds only if all the items were printed.
bool ReversePrintMap(const MapWrapper& print_map, bool supress) {
  const int elements_to_print = print_map.Size();
  int elements_printed = 0;
  MapItem* item = print_map.Last();
  PrintToConsole("[", supress);
  while (item != NULL) {
    PrintMapItem(item, supress);
    ++elements_printed;
    item = print_map.Previous(item);
  }
  PrintToConsole("]\n", supress);
  return elements_printed == elements_to_print;
}

// Returns true if the map items contain the same item.
bool CompareItems(MapItem* lhs_item, MapItem* rhs_item) {
  if ((lhs_item == NULL) || (rhs_item == NULL)) {
    return false;
  }
  if (lhs_item->GetId() != rhs_item->GetId()) {
    return false;
  }
  return lhs_item->GetItem() == rhs_item->GetItem();
}

// Returns true if the map contains the same items.
bool CompareMaps(const MapWrapper& lhs, const MapWrapper& rhs) {
  const int map_size = lhs.Size();
  if (map_size != rhs.Size()) {
    return false;
  }
  int item_count = 0;
  MapItem* lhs_item = lhs.First();
  while (lhs_item != NULL) {
    MapItem* rhs_item = rhs.Find(lhs_item->GetId());
    if (rhs_item == NULL) {
      return false;
    }
    if (!CompareItems(lhs_item, rhs_item)) {
      return false;
    }
    ++item_count;
    lhs_item = lhs.Next(lhs_item);
  }
  return item_count == map_size;
}

class MapWrapperTest : public ::testing::Test {
protected:
  virtual void SetUp() {
    ASSERT_TRUE(CreateAscendingMap(&ascending_map_));
  }

  virtual void TearDown() {
    EXPECT_TRUE(ClearMap(&ascending_map_));
  }
  MapWrapper ascending_map_;
};

TEST_F(MapWrapperTest, RemoveTest) {
  // Erase using int id
  {
    // Create local scope to avoid accidental re-use
    MapItem* item_first = ascending_map_.First();
    ASSERT_FALSE(item_first == NULL);
    const int first_value_id = item_first->GetId();
    const int first_value = ItemValue(item_first);
    EXPECT_TRUE(first_value == 0);
    EXPECT_EQ(first_value_id, first_value);
    EXPECT_FALSE(NULL == ascending_map_.Find(first_value_id));
    EXPECT_TRUE(DeleteItemContent(item_first));
    ascending_map_.Erase(first_value_id);
    EXPECT_TRUE(NULL == ascending_map_.Find(first_value_id));
    EXPECT_EQ(kNumberOfElements - 1, ascending_map_.Size());
  }

  // Erase using MapItem* item
  MapItem* item_last = ascending_map_.Last();
  ASSERT_FALSE(item_last == NULL);
  const int last_value_id = item_last->GetId();
  const int last_value = ItemValue(item_last);
  EXPECT_TRUE(last_value == kNumberOfElements - 1);
  EXPECT_EQ(last_value_id, last_value);
  EXPECT_FALSE(NULL == ascending_map_.Find(last_value_id));
  EXPECT_TRUE(DeleteItemContent(item_last));
  ascending_map_.Erase(last_value_id);
  EXPECT_TRUE(NULL == ascending_map_.Find(last_value_id));
  EXPECT_EQ(kNumberOfElements - 2, ascending_map_.Size());
}

TEST_F(MapWrapperTest, PrintTest) {
  const bool supress = true; // Don't spam the console

  EXPECT_TRUE(PrintMap(ascending_map_, supress));
  EXPECT_TRUE(ReversePrintMap(ascending_map_, supress));
}

TEST_F(MapWrapperTest, CopyTest) {
  MapWrapper compare_map;
  ASSERT_TRUE(CreateAscendingMap(&compare_map));
  const int map_size = compare_map.Size();
  ASSERT_EQ(ascending_map_.Size(), map_size);
  // CompareMaps compare the pointers not value of the pointers.
  // (the values are the same since both are ascending maps).
  EXPECT_FALSE(CompareMaps(compare_map, ascending_map_));

  int copy_count = 0;
  MapItem* ascend_item = ascending_map_.First();
  while (ascend_item != NULL) {
    MapItem* compare_item = compare_map.Find(ascend_item->GetId());
    ASSERT_FALSE(compare_item == NULL);
    DeleteItemContent(compare_item);
    compare_item->SetItem(ascend_item->GetItem());
    ascend_item = ascending_map_.Next(ascend_item);
    ++copy_count;
  }
  EXPECT_TRUE(CompareMaps(compare_map, ascending_map_));
  while (compare_map.Erase(compare_map.First()) == 0) {
  }
  EXPECT_EQ(map_size, copy_count);
}
