/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Assertions.h"
#include "mozilla/RollingMean.h"

using mozilla::RollingMean;

class MyClass
{
  public:
    uint32_t value;

    explicit MyClass(uint32_t val = 0) : value(val) {
    }

    bool operator==(const MyClass& other) const {
      return value == other.value;
    }

    MyClass operator+(const MyClass& other) const {
      return MyClass(value + other.value);
    }

    MyClass operator-(const MyClass& other) const {
      return MyClass(value - other.value);
    }

    MyClass operator/(uint32_t div) const {
      return MyClass(value / div);
    }
};

class RollingMeanSuite
{
  public:
    RollingMeanSuite()
    { }

    void runTests() {
      testZero();
      testClear();
      testRolling();
      testClass();
      testMove();
    }

  private:
    void testZero() {
      RollingMean<uint32_t, uint64_t> mean(3);
      MOZ_RELEASE_ASSERT(mean.empty());
    }

    void testClear() {
      RollingMean<uint32_t, uint64_t> mean(3);

      mean.insert(4);
      MOZ_RELEASE_ASSERT(mean.mean() == 4);

      mean.clear();
      MOZ_RELEASE_ASSERT(mean.empty());

      mean.insert(3);
      MOZ_RELEASE_ASSERT(mean.mean() == 3);
    }

    void testRolling() {
      RollingMean<uint32_t, uint64_t> mean(3);

      mean.insert(10);
      MOZ_RELEASE_ASSERT(mean.mean() == 10);

      mean.insert(20);
      MOZ_RELEASE_ASSERT(mean.mean() == 15);

      mean.insert(35);
      MOZ_RELEASE_ASSERT(mean.mean() == 21);

      mean.insert(5);
      MOZ_RELEASE_ASSERT(mean.mean() == 20);

      mean.insert(10);
      MOZ_RELEASE_ASSERT(mean.mean() == 16);
    }

    void testClass() {
      RollingMean<MyClass, MyClass> mean(3);

      mean.insert(MyClass(4));
      MOZ_RELEASE_ASSERT(mean.mean() == MyClass(4));

      mean.clear();
      MOZ_RELEASE_ASSERT(mean.empty());
    }

    void testMove() {
      RollingMean<uint32_t, uint64_t> mean(3);
      mean = RollingMean<uint32_t, uint64_t>(4);
      MOZ_RELEASE_ASSERT(mean.maxValues() == 4);

      mean.insert(10);
      MOZ_RELEASE_ASSERT(mean.mean() == 10);

      mean = RollingMean<uint32_t, uint64_t>(3);
      mean.insert(30);
      mean.insert(40);
      mean.insert(50);
      mean.insert(60);
      MOZ_RELEASE_ASSERT(mean.mean() == 50);
    }

};

int main()
{
  RollingMeanSuite suite;
  suite.runTests();
  return 0;
}
