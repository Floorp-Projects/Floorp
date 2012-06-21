/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <stdlib.h>

#include "map_wrapper.h"

const int kNumberOfElements = 10;

void FailTest(bool failed)
{
    if (failed)
    {
        printf("Test failed!\n");
        printf("Press enter to continue:");
        getchar();
        exit(0);
    }
}

int GetStoredIntegerValue(MapItem* map_item)
{
    void* map_item_pointer = map_item->GetItem();
    if (map_item_pointer != NULL)
    {
        return *(reinterpret_cast<int*>(map_item_pointer));
    }
    return static_cast<int>(map_item->GetUnsignedId());
}

void PrintMap(MapWrapper& map)
{
    MapItem* map_item = map.First();
    printf("Map: ");
    while (map_item != NULL)
    {
      int item_value = GetStoredIntegerValue(map_item);
        FailTest(item_value < 0);
        printf(" %d",item_value);
        map_item = map.Next(map_item);
    }
    printf("\n");
}

int main(int /*argc*/, char* /*argv*/[])
{
    int element_array[kNumberOfElements];
    for (int i = 0; i < kNumberOfElements; i++)
    {
        element_array[i] = i;
    }
    // Test insert
    MapWrapper test_map;
    for (int i = 0; i < kNumberOfElements; i++)
    {
        test_map.Insert(i,(void*)&element_array[i]);
    }
    // Test Erase1
    MapItem* remove_item = test_map.Find(2);
    FailTest(remove_item == NULL);
    FailTest(test_map.Erase(remove_item) != 0);
    FailTest(test_map.Find(2) != NULL);
    remove_item = NULL;
    FailTest(test_map.Erase(remove_item) != -1);
    // Test Erase2
    FailTest(test_map.Erase(1) != 0);
    FailTest(test_map.Find(1) != NULL);
    FailTest(test_map.Erase(1) != -1);
    // Test Size
    FailTest(test_map.Size() != kNumberOfElements - 2);
    PrintMap(test_map);
    // Test First
    MapItem* first_item = test_map.First();
    FailTest(first_item == NULL);
    FailTest(GetStoredIntegerValue(first_item) != 0);
    // Test Last
    MapItem* last_item = test_map.Last();
    FailTest(last_item == NULL);
    FailTest(GetStoredIntegerValue(last_item) != 9);
    // Test Next
    MapItem* second_item = test_map.Next(first_item);
    FailTest(second_item == NULL);
    FailTest(GetStoredIntegerValue(second_item) != 3);
    FailTest(test_map.Next(last_item) != NULL);
    // Test Previous
    MapItem* second_to_last_item = test_map.Previous(last_item);
    FailTest(second_to_last_item == NULL);
    FailTest(GetStoredIntegerValue(second_to_last_item) != 8);
    FailTest(test_map.Previous(first_item) != NULL);
    // Test Find (only improper usage untested)
    FailTest(test_map.Find(kNumberOfElements + 2) != NULL);
    // Test GetId
    FailTest(*(reinterpret_cast<int*>(second_to_last_item->GetItem())) !=
         second_to_last_item->GetId());
    FailTest(second_to_last_item->GetUnsignedId() !=
             static_cast<unsigned int>(second_to_last_item->GetId()));
    // Test SetItem
    int swapped_item = kNumberOfElements;
    last_item->SetItem(reinterpret_cast<void*>(&swapped_item));
    FailTest(GetStoredIntegerValue(last_item) !=
             swapped_item);

    printf("Tests passed successfully!\n");
}
