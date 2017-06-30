// Copyright 2016 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/fxcrt/cfx_shared_copy_on_write.h"

#include <map>
#include <string>

#include "testing/fx_string_testhelpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class Observer {
 public:
  void OnConstruct(const std::string& name) { construction_counts_[name]++; }
  void OnDestruct(const std::string& name) { destruction_counts_[name]++; }
  int GetConstructionCount(const std::string& name) {
    return construction_counts_[name];
  }
  int GetDestructionCount(const std::string& name) {
    return destruction_counts_[name];
  }

 private:
  std::map<std::string, int> construction_counts_;
  std::map<std::string, int> destruction_counts_;
};

class Object {
 public:
  Object(Observer* observer, const std::string& name)
      : name_(name), observer_(observer) {
    observer->OnConstruct(name_);
  }
  Object(const Object& that) : name_(that.name_), observer_(that.observer_) {
    observer_->OnConstruct(name_);
  }
  ~Object() { observer_->OnDestruct(name_); }

 private:
  std::string name_;
  Observer* observer_;
};

}  // namespace

TEST(fxcrt, SharedCopyOnWriteNull) {
  Observer observer;
  {
    CFX_SharedCopyOnWrite<Object> ptr;
    EXPECT_EQ(nullptr, ptr.GetObject());
  }
}

TEST(fxcrt, SharedCopyOnWriteCopy) {
  Observer observer;
  {
    CFX_SharedCopyOnWrite<Object> ptr1;
    ptr1.Emplace(&observer, std::string("one"));
    {
      CFX_SharedCopyOnWrite<Object> ptr2 = ptr1;
      EXPECT_EQ(1, observer.GetConstructionCount("one"));
      EXPECT_EQ(0, observer.GetDestructionCount("one"));
    }
    {
      CFX_SharedCopyOnWrite<Object> ptr3(ptr1);
      EXPECT_EQ(1, observer.GetConstructionCount("one"));
      EXPECT_EQ(0, observer.GetDestructionCount("one"));
    }
    EXPECT_EQ(1, observer.GetConstructionCount("one"));
    EXPECT_EQ(0, observer.GetDestructionCount("one"));
  }
  EXPECT_EQ(1, observer.GetDestructionCount("one"));
}

TEST(fxcrt, SharedCopyOnWriteAssignOverOld) {
  Observer observer;
  {
    CFX_SharedCopyOnWrite<Object> ptr1;
    ptr1.Emplace(&observer, std::string("one"));
    ptr1.Emplace(&observer, std::string("two"));
    EXPECT_EQ(1, observer.GetConstructionCount("one"));
    EXPECT_EQ(1, observer.GetConstructionCount("two"));
    EXPECT_EQ(1, observer.GetDestructionCount("one"));
    EXPECT_EQ(0, observer.GetDestructionCount("two"));
  }
  EXPECT_EQ(1, observer.GetDestructionCount("two"));
}

TEST(fxcrt, SharedCopyOnWriteAssignOverRetained) {
  Observer observer;
  {
    CFX_SharedCopyOnWrite<Object> ptr1;
    ptr1.Emplace(&observer, std::string("one"));
    CFX_SharedCopyOnWrite<Object> ptr2(ptr1);
    ptr1.Emplace(&observer, std::string("two"));
    EXPECT_EQ(1, observer.GetConstructionCount("one"));
    EXPECT_EQ(1, observer.GetConstructionCount("two"));
    EXPECT_EQ(0, observer.GetDestructionCount("one"));
    EXPECT_EQ(0, observer.GetDestructionCount("two"));
  }
  EXPECT_EQ(1, observer.GetDestructionCount("one"));
  EXPECT_EQ(1, observer.GetDestructionCount("two"));
}

TEST(fxcrt, SharedCopyOnWriteGetModify) {
  Observer observer;
  {
    CFX_SharedCopyOnWrite<Object> ptr;
    EXPECT_NE(nullptr, ptr.GetPrivateCopy(&observer, std::string("one")));
    EXPECT_EQ(1, observer.GetConstructionCount("one"));
    EXPECT_EQ(0, observer.GetDestructionCount("one"));

    EXPECT_NE(nullptr, ptr.GetPrivateCopy(&observer, std::string("one")));
    EXPECT_EQ(1, observer.GetConstructionCount("one"));
    EXPECT_EQ(0, observer.GetDestructionCount("one"));
    {
      CFX_SharedCopyOnWrite<Object> other(ptr);
      EXPECT_NE(nullptr, ptr.GetPrivateCopy(&observer, std::string("one")));
      EXPECT_EQ(2, observer.GetConstructionCount("one"));
      EXPECT_EQ(0, observer.GetDestructionCount("one"));
    }
    EXPECT_EQ(2, observer.GetConstructionCount("one"));
    EXPECT_EQ(1, observer.GetDestructionCount("one"));
  }
  EXPECT_EQ(2, observer.GetDestructionCount("one"));
}
