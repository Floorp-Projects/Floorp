// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/cfx_weak_ptr.h"

#include <memory>
#include <utility>

#include "core/fxcrt/fx_memory.h"
#include "testing/fx_string_testhelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PseudoDeletable;
using WeakPtr = CFX_WeakPtr<PseudoDeletable, ReleaseDeleter<PseudoDeletable>>;
using UniquePtr =
    std::unique_ptr<PseudoDeletable, ReleaseDeleter<PseudoDeletable>>;

class PseudoDeletable {
 public:
  PseudoDeletable() : delete_count_(0) {}
  void Release() {
    ++delete_count_;
    next_.Reset();
  }
  void SetNext(const WeakPtr& next) { next_ = next; }
  int delete_count() const { return delete_count_; }

 private:
  int delete_count_;
  WeakPtr next_;
};

}  // namespace

TEST(fxcrt, WeakPtrNull) {
  WeakPtr ptr1;
  EXPECT_FALSE(ptr1);

  WeakPtr ptr2;
  EXPECT_TRUE(ptr1 == ptr2);
  EXPECT_FALSE(ptr1 != ptr2);

  WeakPtr ptr3(ptr1);
  EXPECT_TRUE(ptr1 == ptr3);
  EXPECT_FALSE(ptr1 != ptr3);

  WeakPtr ptr4 = ptr1;
  EXPECT_TRUE(ptr1 == ptr4);
  EXPECT_FALSE(ptr1 != ptr4);
}

TEST(fxcrt, WeakPtrNonNull) {
  PseudoDeletable thing;
  EXPECT_EQ(0, thing.delete_count());
  {
    UniquePtr unique(&thing);
    WeakPtr ptr1(std::move(unique));
    EXPECT_TRUE(ptr1);
    EXPECT_EQ(&thing, ptr1.Get());

    WeakPtr ptr2;
    EXPECT_FALSE(ptr1 == ptr2);
    EXPECT_TRUE(ptr1 != ptr2);
    {
      WeakPtr ptr3(ptr1);
      EXPECT_TRUE(ptr1 == ptr3);
      EXPECT_FALSE(ptr1 != ptr3);
      EXPECT_EQ(&thing, ptr3.Get());
      {
        WeakPtr ptr4 = ptr1;
        EXPECT_TRUE(ptr1 == ptr4);
        EXPECT_FALSE(ptr1 != ptr4);
        EXPECT_EQ(&thing, ptr4.Get());
      }
    }
    EXPECT_EQ(0, thing.delete_count());
  }
  EXPECT_EQ(1, thing.delete_count());
}

TEST(fxcrt, WeakPtrResetNull) {
  PseudoDeletable thing;
  {
    UniquePtr unique(&thing);
    WeakPtr ptr1(std::move(unique));
    WeakPtr ptr2 = ptr1;
    ptr1.Reset();
    EXPECT_FALSE(ptr1);
    EXPECT_EQ(nullptr, ptr1.Get());
    EXPECT_TRUE(ptr2);
    EXPECT_EQ(&thing, ptr2.Get());
    EXPECT_FALSE(ptr1 == ptr2);
    EXPECT_TRUE(ptr1 != ptr2);
    EXPECT_EQ(0, thing.delete_count());
  }
  EXPECT_EQ(1, thing.delete_count());
}

TEST(fxcrt, WeakPtrResetNonNull) {
  PseudoDeletable thing1;
  PseudoDeletable thing2;
  {
    UniquePtr unique1(&thing1);
    WeakPtr ptr1(std::move(unique1));
    WeakPtr ptr2 = ptr1;
    UniquePtr unique2(&thing2);
    ptr2.Reset(std::move(unique2));
    EXPECT_TRUE(ptr1);
    EXPECT_EQ(&thing1, ptr1.Get());
    EXPECT_TRUE(ptr2);
    EXPECT_EQ(&thing2, ptr2.Get());
    EXPECT_FALSE(ptr1 == ptr2);
    EXPECT_TRUE(ptr1 != ptr2);
    EXPECT_EQ(0, thing1.delete_count());
    EXPECT_EQ(0, thing2.delete_count());
  }
  EXPECT_EQ(1, thing1.delete_count());
  EXPECT_EQ(1, thing2.delete_count());
}

TEST(fxcrt, WeakPtrDeleteObject) {
  PseudoDeletable thing;
  {
    UniquePtr unique(&thing);
    WeakPtr ptr1(std::move(unique));
    WeakPtr ptr2 = ptr1;
    ptr1.DeleteObject();
    EXPECT_FALSE(ptr1);
    EXPECT_EQ(nullptr, ptr1.Get());
    EXPECT_FALSE(ptr2);
    EXPECT_EQ(nullptr, ptr2.Get());
    EXPECT_FALSE(ptr1 == ptr2);
    EXPECT_TRUE(ptr1 != ptr2);
    EXPECT_EQ(1, thing.delete_count());
  }
  EXPECT_EQ(1, thing.delete_count());
}

TEST(fxcrt, WeakPtrCyclic) {
  PseudoDeletable thing1;
  PseudoDeletable thing2;
  {
    UniquePtr unique1(&thing1);
    UniquePtr unique2(&thing2);
    WeakPtr ptr1(std::move(unique1));
    WeakPtr ptr2(std::move(unique2));
    ptr1->SetNext(ptr2);
    ptr2->SetNext(ptr1);
  }
  // Leaks without explicit clear.
  EXPECT_EQ(0, thing1.delete_count());
  EXPECT_EQ(0, thing2.delete_count());
}

TEST(fxcrt, WeakPtrCyclicDeleteObject) {
  PseudoDeletable thing1;
  PseudoDeletable thing2;
  {
    UniquePtr unique1(&thing1);
    UniquePtr unique2(&thing2);
    WeakPtr ptr1(std::move(unique1));
    WeakPtr ptr2(std::move(unique2));
    ptr1->SetNext(ptr2);
    ptr2->SetNext(ptr1);
    ptr1.DeleteObject();
    EXPECT_EQ(1, thing1.delete_count());
    EXPECT_EQ(0, thing2.delete_count());
  }
  EXPECT_EQ(1, thing1.delete_count());
  EXPECT_EQ(1, thing2.delete_count());
}
