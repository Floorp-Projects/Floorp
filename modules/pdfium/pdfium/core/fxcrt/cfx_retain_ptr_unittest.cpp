// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/cfx_retain_ptr.h"

#include <utility>

#include "testing/fx_string_testhelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class PseudoRetainable {
 public:
  PseudoRetainable() : retain_count_(0), release_count_(0) {}
  void Retain() { ++retain_count_; }
  void Release() { ++release_count_; }
  int retain_count() const { return retain_count_; }
  int release_count() const { return release_count_; }

 private:
  int retain_count_;
  int release_count_;
};

}  // namespace

TEST(fxcrt, RetainPtrNull) {
  CFX_RetainPtr<PseudoRetainable> ptr;
  EXPECT_EQ(nullptr, ptr.Get());
}

TEST(fxcrt, RetainPtrNormal) {
  PseudoRetainable obj;
  {
    CFX_RetainPtr<PseudoRetainable> ptr(&obj);
    EXPECT_EQ(&obj, ptr.Get());
    EXPECT_EQ(1, obj.retain_count());
    EXPECT_EQ(0, obj.release_count());
  }
  EXPECT_EQ(1, obj.retain_count());
  EXPECT_EQ(1, obj.release_count());
}

TEST(fxcrt, RetainPtrCopyCtor) {
  PseudoRetainable obj;
  {
    CFX_RetainPtr<PseudoRetainable> ptr1(&obj);
    {
      CFX_RetainPtr<PseudoRetainable> ptr2(ptr1);
      EXPECT_EQ(2, obj.retain_count());
      EXPECT_EQ(0, obj.release_count());
    }
    EXPECT_EQ(2, obj.retain_count());
    EXPECT_EQ(1, obj.release_count());
  }
  EXPECT_EQ(2, obj.retain_count());
  EXPECT_EQ(2, obj.release_count());
}

TEST(fxcrt, RetainPtrMoveCtor) {
  PseudoRetainable obj;
  {
    CFX_RetainPtr<PseudoRetainable> ptr1(&obj);
    {
      CFX_RetainPtr<PseudoRetainable> ptr2(std::move(ptr1));
      EXPECT_EQ(1, obj.retain_count());
      EXPECT_EQ(0, obj.release_count());
    }
    EXPECT_EQ(1, obj.retain_count());
    EXPECT_EQ(1, obj.release_count());
  }
  EXPECT_EQ(1, obj.retain_count());
  EXPECT_EQ(1, obj.release_count());
}

TEST(fxcrt, RetainPtrResetNull) {
  PseudoRetainable obj;
  {
    CFX_RetainPtr<PseudoRetainable> ptr(&obj);
    ptr.Reset();
    EXPECT_EQ(1, obj.retain_count());
    EXPECT_EQ(1, obj.release_count());
  }
  EXPECT_EQ(1, obj.retain_count());
  EXPECT_EQ(1, obj.release_count());
}

TEST(fxcrt, RetainPtrReset) {
  PseudoRetainable obj1;
  PseudoRetainable obj2;
  {
    CFX_RetainPtr<PseudoRetainable> ptr(&obj1);
    ptr.Reset(&obj2);
    EXPECT_EQ(1, obj1.retain_count());
    EXPECT_EQ(1, obj1.release_count());
    EXPECT_EQ(1, obj2.retain_count());
    EXPECT_EQ(0, obj2.release_count());
  }
  EXPECT_EQ(1, obj1.retain_count());
  EXPECT_EQ(1, obj1.release_count());
  EXPECT_EQ(1, obj2.retain_count());
  EXPECT_EQ(1, obj2.release_count());
}

TEST(fxcrt, RetainPtrSwap) {
  PseudoRetainable obj1;
  PseudoRetainable obj2;
  {
    CFX_RetainPtr<PseudoRetainable> ptr1(&obj1);
    {
      CFX_RetainPtr<PseudoRetainable> ptr2(&obj2);
      ptr1.Swap(ptr2);
      EXPECT_EQ(1, obj1.retain_count());
      EXPECT_EQ(0, obj1.release_count());
      EXPECT_EQ(1, obj2.retain_count());
      EXPECT_EQ(0, obj2.release_count());
    }
    EXPECT_EQ(1, obj1.retain_count());
    EXPECT_EQ(1, obj1.release_count());
    EXPECT_EQ(1, obj2.retain_count());
    EXPECT_EQ(0, obj2.release_count());
  }
  EXPECT_EQ(1, obj1.retain_count());
  EXPECT_EQ(1, obj1.release_count());
  EXPECT_EQ(1, obj2.retain_count());
  EXPECT_EQ(1, obj2.release_count());
}

TEST(fxcrt, RetainPtrLeak) {
  PseudoRetainable obj;
  PseudoRetainable* leak;
  {
    CFX_RetainPtr<PseudoRetainable> ptr(&obj);
    leak = ptr.Leak();
    EXPECT_EQ(1, obj.retain_count());
    EXPECT_EQ(0, obj.release_count());
  }
  EXPECT_EQ(1, obj.retain_count());
  EXPECT_EQ(0, obj.release_count());
  {
    CFX_RetainPtr<PseudoRetainable> ptr;
    ptr.Unleak(leak);
    EXPECT_EQ(1, obj.retain_count());
    EXPECT_EQ(0, obj.release_count());
  }
  EXPECT_EQ(1, obj.retain_count());
  EXPECT_EQ(1, obj.release_count());
}

TEST(fxcrt, RetainPtrSwapNull) {
  PseudoRetainable obj1;
  {
    CFX_RetainPtr<PseudoRetainable> ptr1(&obj1);
    {
      CFX_RetainPtr<PseudoRetainable> ptr2;
      ptr1.Swap(ptr2);
      EXPECT_FALSE(ptr1);
      EXPECT_TRUE(ptr2);
      EXPECT_EQ(1, obj1.retain_count());
      EXPECT_EQ(0, obj1.release_count());
    }
    EXPECT_EQ(1, obj1.retain_count());
    EXPECT_EQ(1, obj1.release_count());
  }
  EXPECT_EQ(1, obj1.retain_count());
  EXPECT_EQ(1, obj1.release_count());
}

TEST(fxcrt, RetainPtrAssign) {
  PseudoRetainable obj;
  {
    CFX_RetainPtr<PseudoRetainable> ptr(&obj);
    {
      CFX_RetainPtr<PseudoRetainable> ptr2;
      ptr2 = ptr;
      EXPECT_EQ(2, obj.retain_count());
      EXPECT_EQ(0, obj.release_count());
    }
    EXPECT_EQ(2, obj.retain_count());
    EXPECT_EQ(1, obj.release_count());
  }
  EXPECT_EQ(2, obj.retain_count());
  EXPECT_EQ(2, obj.release_count());
}

TEST(fxcrt, RetainPtrEquals) {
  PseudoRetainable obj1;
  PseudoRetainable obj2;
  CFX_RetainPtr<PseudoRetainable> null_ptr1;
  CFX_RetainPtr<PseudoRetainable> obj1_ptr1(&obj1);
  CFX_RetainPtr<PseudoRetainable> obj2_ptr1(&obj2);
  {
    CFX_RetainPtr<PseudoRetainable> null_ptr2;
    EXPECT_TRUE(null_ptr1 == null_ptr2);

    CFX_RetainPtr<PseudoRetainable> obj1_ptr2(&obj1);
    EXPECT_TRUE(obj1_ptr1 == obj1_ptr2);

    CFX_RetainPtr<PseudoRetainable> obj2_ptr2(&obj2);
    EXPECT_TRUE(obj2_ptr1 == obj2_ptr2);
  }
  EXPECT_FALSE(null_ptr1 == obj1_ptr1);
  EXPECT_FALSE(null_ptr1 == obj2_ptr1);
  EXPECT_FALSE(obj1_ptr1 == obj2_ptr1);
}

TEST(fxcrt, RetainPtrNotEquals) {
  PseudoRetainable obj1;
  PseudoRetainable obj2;
  CFX_RetainPtr<PseudoRetainable> null_ptr1;
  CFX_RetainPtr<PseudoRetainable> obj1_ptr1(&obj1);
  CFX_RetainPtr<PseudoRetainable> obj2_ptr1(&obj2);
  {
    CFX_RetainPtr<PseudoRetainable> null_ptr2;
    CFX_RetainPtr<PseudoRetainable> obj1_ptr2(&obj1);
    CFX_RetainPtr<PseudoRetainable> obj2_ptr2(&obj2);
    EXPECT_FALSE(null_ptr1 != null_ptr2);
    EXPECT_FALSE(obj1_ptr1 != obj1_ptr2);
    EXPECT_FALSE(obj2_ptr1 != obj2_ptr2);
  }
  EXPECT_TRUE(null_ptr1 != obj1_ptr1);
  EXPECT_TRUE(null_ptr1 != obj2_ptr1);
  EXPECT_TRUE(obj1_ptr1 != obj2_ptr1);
}

TEST(fxcrt, RetainPtrLessThan) {
  PseudoRetainable objs[2];
  CFX_RetainPtr<PseudoRetainable> obj1_ptr(&objs[0]);
  CFX_RetainPtr<PseudoRetainable> obj2_ptr(&objs[1]);
  EXPECT_TRUE(obj1_ptr < obj2_ptr);
  EXPECT_FALSE(obj2_ptr < obj1_ptr);
}

TEST(fxcrt, RetainPtrBool) {
  PseudoRetainable obj1;
  CFX_RetainPtr<PseudoRetainable> null_ptr;
  CFX_RetainPtr<PseudoRetainable> obj1_ptr(&obj1);
  bool null_bool = !!null_ptr;
  bool obj1_bool = !!obj1_ptr;
  EXPECT_FALSE(null_bool);
  EXPECT_TRUE(obj1_bool);
}

TEST(fxcrt, RetainPtrMakeRetained) {
  auto ptr = pdfium::MakeRetain<CFX_Retainable>();
  EXPECT_TRUE(ptr->HasOneRef());
  {
    CFX_RetainPtr<CFX_Retainable> other = ptr;
    EXPECT_FALSE(ptr->HasOneRef());
  }
  EXPECT_TRUE(ptr->HasOneRef());
}
