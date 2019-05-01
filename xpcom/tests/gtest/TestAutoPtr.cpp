/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoPtr.h"
#include "gtest/gtest.h"

class TestObjectBaseA {
 public:
  // Virtual dtor for deleting through base class pointer
  virtual ~TestObjectBaseA() {}
  void MemberFunction(int, int*, int&) {}
  virtual void VirtualMemberFunction(int, int*, int&){};
  virtual void VirtualConstMemberFunction(int, int*, int&) const {};
  int fooA;
};

class TestObjectBaseB {
 public:
  // Virtual dtor for deleting through base class pointer
  virtual ~TestObjectBaseB() {}
  int fooB;
};

class TestObject : public TestObjectBaseA, public TestObjectBaseB {
 public:
  TestObject() {}

  // Virtual dtor for deleting through base class pointer
  virtual ~TestObject() { destructed++; }

  virtual void VirtualMemberFunction(int, int*, int&) override {}
  virtual void VirtualConstMemberFunction(int, int*, int&) const override {}

  static int destructed;
  static const void* last_ptr;
};

int TestObject::destructed = 0;
const void* TestObject::last_ptr = nullptr;

static void CreateTestObject(TestObject** aResult) {
  *aResult = new TestObject();
}

static void DoSomethingWithTestObject(TestObject* aIn) {
  TestObject::last_ptr = static_cast<void*>(aIn);
}

static void DoSomethingWithConstTestObject(const TestObject* aIn) {
  TestObject::last_ptr = static_cast<const void*>(aIn);
}

static void DoSomethingWithTestObjectBaseB(TestObjectBaseB* aIn) {
  TestObject::last_ptr = static_cast<void*>(aIn);
}

static void DoSomethingWithConstTestObjectBaseB(const TestObjectBaseB* aIn) {
  TestObject::last_ptr = static_cast<const void*>(aIn);
}

TEST(AutoPtr, Assignment)
{
  TestObject::destructed = 0;

  { nsAutoPtr<TestObject> pobj(new TestObject()); }

  ASSERT_EQ(1, TestObject::destructed);

  {
    nsAutoPtr<TestObject> pobj(new TestObject());
    pobj = new TestObject();
    ASSERT_EQ(2, TestObject::destructed);
  }

  ASSERT_EQ(3, TestObject::destructed);
}

TEST(AutoPtr, getter_Transfers)
{
  TestObject::destructed = 0;

  {
    nsAutoPtr<TestObject> ptr;
    CreateTestObject(getter_Transfers(ptr));
  }

  ASSERT_EQ(1, TestObject::destructed);
}

TEST(AutoPtr, Casting)
{
  // This comparison is always false, as it should be. The extra parens
  // suppress a -Wunreachable-code warning about printf being unreachable.
  ASSERT_NE(((void*)(TestObject*)0x1000),
            ((void*)(TestObjectBaseB*)(TestObject*)0x1000));

  {
    nsAutoPtr<TestObject> p1(new TestObject());
    TestObjectBaseB* p2 = p1;
    ASSERT_NE(static_cast<void*>(p1), static_cast<void*>(p2));
    ASSERT_EQ(p1, p2);
    ASSERT_FALSE(p1 != p2);
    ASSERT_EQ(p2, p1);
    ASSERT_FALSE(p2 != p1);
  }

  {
    TestObject* p1 = new TestObject();
    nsAutoPtr<TestObjectBaseB> p2(p1);
    ASSERT_EQ(p1, p2);
    ASSERT_FALSE(p1 != p2);
    ASSERT_EQ(p2, p1);
    ASSERT_FALSE(p2 != p1);
  }
}

TEST(AutoPtr, Forget)
{
  TestObject::destructed = 0;

  {
    nsAutoPtr<TestObject> pobj(new TestObject());
    nsAutoPtr<TestObject> pobj2(pobj.forget());
    ASSERT_EQ(0, TestObject::destructed);
  }

  ASSERT_EQ(1, TestObject::destructed);
}

TEST(AutoPtr, Construction)
{
  TestObject::destructed = 0;

  { nsAutoPtr<TestObject> pobj(new TestObject()); }

  ASSERT_EQ(1, TestObject::destructed);
}

TEST(AutoPtr, ImplicitConversion)
{
  // This test is basically successful if it builds. We add a few assertions
  // to make gtest happy.
  TestObject::destructed = 0;

  {
    nsAutoPtr<TestObject> pobj(new TestObject());
    DoSomethingWithTestObject(pobj);
    DoSomethingWithConstTestObject(pobj);
  }

  ASSERT_EQ(1, TestObject::destructed);

  {
    nsAutoPtr<TestObject> pobj(new TestObject());
    DoSomethingWithTestObjectBaseB(pobj);
    DoSomethingWithConstTestObjectBaseB(pobj);
  }

  ASSERT_EQ(2, TestObject::destructed);

  {
    const nsAutoPtr<TestObject> pobj(new TestObject());
    DoSomethingWithTestObject(pobj);
    DoSomethingWithConstTestObject(pobj);
  }

  ASSERT_EQ(3, TestObject::destructed);

  {
    const nsAutoPtr<TestObject> pobj(new TestObject());
    DoSomethingWithTestObjectBaseB(pobj);
    DoSomethingWithConstTestObjectBaseB(pobj);
  }

  ASSERT_EQ(4, TestObject::destructed);
}

TEST(AutoPtr, ArrowOperator)
{
  // This test is basically successful if it builds. We add a few assertions
  // to make gtest happy.
  TestObject::destructed = 0;

  {
    int test = 1;
    void (TestObjectBaseA::*fPtr)(int, int*, int&) =
        &TestObjectBaseA::MemberFunction;
    void (TestObjectBaseA::*fVPtr)(int, int*, int&) =
        &TestObjectBaseA::VirtualMemberFunction;
    void (TestObjectBaseA::*fVCPtr)(int, int*, int&) const =
        &TestObjectBaseA::VirtualConstMemberFunction;
    nsAutoPtr<TestObjectBaseA> pobj(new TestObject());
    (pobj->*fPtr)(test, &test, test);
    (pobj->*fVPtr)(test, &test, test);
    (pobj->*fVCPtr)(test, &test, test);
  }

  ASSERT_EQ(1, TestObject::destructed);
}
