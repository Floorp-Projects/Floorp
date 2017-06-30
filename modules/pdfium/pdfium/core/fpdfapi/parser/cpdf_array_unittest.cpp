// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fpdfapi/parser/cpdf_array.h"

#include <memory>
#include <utility>

#include "core/fpdfapi/parser/cpdf_number.h"
#include "core/fpdfapi/parser/cpdf_reference.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/base/ptr_util.h"

TEST(cpdf_array, RemoveAt) {
  {
    int elems[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::unique_ptr<CPDF_Array> arr(new CPDF_Array);
    for (size_t i = 0; i < FX_ArraySize(elems); ++i)
      arr->AddNew<CPDF_Number>(elems[i]);
    arr->RemoveAt(3, 3);
    int expected[] = {1, 2, 3, 7, 8, 9, 10};
    EXPECT_EQ(FX_ArraySize(expected), arr->GetCount());
    for (size_t i = 0; i < FX_ArraySize(expected); ++i)
      EXPECT_EQ(expected[i], arr->GetIntegerAt(i));
    arr->RemoveAt(4, 2);
    int expected2[] = {1, 2, 3, 7, 10};
    EXPECT_EQ(FX_ArraySize(expected2), arr->GetCount());
    for (size_t i = 0; i < FX_ArraySize(expected2); ++i)
      EXPECT_EQ(expected2[i], arr->GetIntegerAt(i));
  }
  {
    // When the range is out of bound, RemoveAt has no effect.
    int elems[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    std::unique_ptr<CPDF_Array> arr(new CPDF_Array);
    for (size_t i = 0; i < FX_ArraySize(elems); ++i)
      arr->AddNew<CPDF_Number>(elems[i]);
    arr->RemoveAt(8, 5);
    EXPECT_EQ(FX_ArraySize(elems), arr->GetCount());
    for (size_t i = 0; i < FX_ArraySize(elems); ++i)
      EXPECT_EQ(elems[i], arr->GetIntegerAt(i));
    arr->RemoveAt(0, 12);
    EXPECT_EQ(FX_ArraySize(elems), arr->GetCount());
    arr->RemoveAt(11, 1);
    EXPECT_EQ(FX_ArraySize(elems), arr->GetCount());
  }
}

TEST(cpdf_array, InsertAt) {
  {
    int elems[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto arr = pdfium::MakeUnique<CPDF_Array>();
    for (size_t i = 0; i < FX_ArraySize(elems); ++i)
      arr->InsertNewAt<CPDF_Number>(i, elems[i]);
    EXPECT_EQ(FX_ArraySize(elems), arr->GetCount());
    for (size_t i = 0; i < FX_ArraySize(elems); ++i)
      EXPECT_EQ(elems[i], arr->GetIntegerAt(i));
    arr->InsertNewAt<CPDF_Number>(3, 33);
    arr->InsertNewAt<CPDF_Number>(6, 55);
    arr->InsertNewAt<CPDF_Number>(12, 12);
    int expected[] = {1, 2, 3, 33, 4, 5, 55, 6, 7, 8, 9, 10, 12};
    EXPECT_EQ(FX_ArraySize(expected), arr->GetCount());
    for (size_t i = 0; i < FX_ArraySize(expected); ++i)
      EXPECT_EQ(expected[i], arr->GetIntegerAt(i));
  }
  {
    // When the position to insert is beyond the upper bound,
    // an element is inserted at that position while other unfilled
    // positions have nullptr.
    int elems[] = {1, 2};
    auto arr = pdfium::MakeUnique<CPDF_Array>();
    for (size_t i = 0; i < FX_ArraySize(elems); ++i)
      arr->InsertNewAt<CPDF_Number>(i, elems[i]);
    arr->InsertNewAt<CPDF_Number>(10, 10);
    EXPECT_EQ(11u, arr->GetCount());
    for (size_t i = 0; i < FX_ArraySize(elems); ++i)
      EXPECT_EQ(elems[i], arr->GetIntegerAt(i));
    for (size_t i = FX_ArraySize(elems); i < 10; ++i)
      EXPECT_EQ(nullptr, arr->GetObjectAt(i));
    EXPECT_EQ(10, arr->GetIntegerAt(10));
  }
}

TEST(cpdf_array, Clone) {
  {
    // Basic case.
    int elems[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    auto arr = pdfium::MakeUnique<CPDF_Array>();
    for (size_t i = 0; i < FX_ArraySize(elems); ++i)
      arr->InsertNewAt<CPDF_Number>(i, elems[i]);
    std::unique_ptr<CPDF_Array> arr2 = ToArray(arr->Clone());
    EXPECT_EQ(arr->GetCount(), arr2->GetCount());
    for (size_t i = 0; i < FX_ArraySize(elems); ++i) {
      // Clone() always create new objects.
      EXPECT_NE(arr->GetObjectAt(i), arr2->GetObjectAt(i));
      EXPECT_EQ(arr->GetIntegerAt(i), arr2->GetIntegerAt(i));
    }
  }
  {
    // Clone() with and without dereferencing reference objects.
    static const size_t kNumOfRows = 3;
    static const size_t kNumOfRowElems = 5;
    int elems[kNumOfRows][kNumOfRowElems] = {
        {1, 2, 3, 4, 5}, {10, 9, 8, 7, 6}, {11, 12, 13, 14, 15}};
    std::unique_ptr<CPDF_Array> arr(new CPDF_Array);
    // Indirect references to indirect objects.
    std::unique_ptr<CPDF_IndirectObjectHolder> obj_holder(
        new CPDF_IndirectObjectHolder());
    for (size_t i = 0; i < kNumOfRows; ++i) {
      auto arr_elem = pdfium::MakeUnique<CPDF_Array>();
      for (size_t j = 0; j < kNumOfRowElems; ++j) {
        std::unique_ptr<CPDF_Number> obj(new CPDF_Number(elems[i][j]));
        // Starts object number from 1.
        int obj_num = i * kNumOfRowElems + j + 1;
        obj_holder->ReplaceIndirectObjectIfHigherGeneration(obj_num,
                                                            std::move(obj));
        arr_elem->InsertNewAt<CPDF_Reference>(j, obj_holder.get(), obj_num);
      }
      arr->InsertAt(i, std::move(arr_elem));
    }
    ASSERT_EQ(kNumOfRows, arr->GetCount());
    // Not dereferencing reference objects means just creating new references
    // instead of new copies of direct objects.
    std::unique_ptr<CPDF_Array> arr1 = ToArray(arr->Clone());
    EXPECT_EQ(arr->GetCount(), arr1->GetCount());
    // Dereferencing reference objects creates new copies of direct objects.
    std::unique_ptr<CPDF_Array> arr2 = ToArray(arr->CloneDirectObject());
    EXPECT_EQ(arr->GetCount(), arr2->GetCount());
    for (size_t i = 0; i < kNumOfRows; ++i) {
      CPDF_Array* arr_elem = arr->GetObjectAt(i)->AsArray();
      CPDF_Array* arr1_elem = arr1->GetObjectAt(i)->AsArray();
      CPDF_Array* arr2_elem = arr2->GetObjectAt(i)->AsArray();
      EXPECT_NE(arr_elem, arr1_elem);
      EXPECT_NE(arr_elem, arr2_elem);
      for (size_t j = 0; j < kNumOfRowElems; ++j) {
        auto elem_obj = arr_elem->GetObjectAt(j);
        auto elem_obj1 = arr1_elem->GetObjectAt(j);
        auto elem_obj2 = arr2_elem->GetObjectAt(j);
        // Results from not deferencing reference objects.
        EXPECT_NE(elem_obj, elem_obj1);
        EXPECT_TRUE(elem_obj1->IsReference());
        EXPECT_EQ(elem_obj->GetDirect(), elem_obj1->GetDirect());
        EXPECT_EQ(elem_obj->GetInteger(), elem_obj1->GetInteger());
        // Results from deferencing reference objects.
        EXPECT_NE(elem_obj, elem_obj2);
        EXPECT_TRUE(elem_obj2->IsNumber());
        EXPECT_NE(elem_obj->GetDirect(), elem_obj2);
        EXPECT_EQ(elem_obj->GetObjNum(), elem_obj2->GetObjNum());
        EXPECT_EQ(elem_obj->GetInteger(), elem_obj2->GetInteger());
      }
    }
    arr.reset();
    ASSERT_EQ(kNumOfRows, arr1->GetCount());
    for (size_t i = 0; i < kNumOfRows; ++i) {
      for (size_t j = 0; j < kNumOfRowElems; ++j) {
        // Results from not deferencing reference objects.
        auto elem_obj1 = arr1->GetObjectAt(i)->AsArray()->GetObjectAt(j);
        EXPECT_TRUE(elem_obj1->IsReference());
        EXPECT_EQ(elems[i][j], elem_obj1->GetInteger());
        // Results from deferencing reference objects.
        EXPECT_EQ(elems[i][j],
                  arr2->GetObjectAt(i)->AsArray()->GetIntegerAt(j));
      }
    }
  }
}

TEST(cpdf_array, Iterator) {
  int elems[] = {-23, -11,     3,         455,   2345877,
                 0,   7895330, -12564334, 10000, -100000};
  std::unique_ptr<CPDF_Array> arr(new CPDF_Array);
  for (size_t i = 0; i < FX_ArraySize(elems); ++i)
    arr->InsertNewAt<CPDF_Number>(i, elems[i]);
  size_t index = 0;
  for (const auto& it : *arr)
    EXPECT_EQ(elems[index++], it->AsNumber()->GetInteger());
}
