/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoPtr.h"
#include <stdio.h>
#include "nscore.h"
#include "mozilla/Attributes.h"

class TestObjectBaseA {
    public:
        // Virtual dtor for deleting through base class pointer
        virtual ~TestObjectBaseA() { }
        void MemberFunction( int, int*, int& )
        {
          printf("member function is invoked.\n");
        }
        virtual void VirtualMemberFunction(int, int*, int&) { };
        virtual void VirtualConstMemberFunction(int, int*, int&) const { };
        int fooA;
};

class TestObjectBaseB {
    public:
        // Virtual dtor for deleting through base class pointer
        virtual ~TestObjectBaseB() { }
        int fooB;
};

class TestObject : public TestObjectBaseA, public TestObjectBaseB {
    public:
        TestObject()
        {
            printf("  Creating TestObject %p.\n",
                   static_cast<void*>(this));
        }

        // Virtual dtor for deleting through base class pointer
        virtual ~TestObject()
        {
            printf("  Destroying TestObject %p.\n",
                   static_cast<void*>(this));
        }

        virtual void VirtualMemberFunction(int, int*, int&) override
        {
          printf("override virtual member function is invoked.\n");
        }
        virtual void VirtualConstMemberFunction(int, int*, int&) const override
        {
          printf("override virtual const member function is invoked.\n");
        }
};

static void CreateTestObject(TestObject **aResult)
{
    *aResult = new TestObject();
}

static void DoSomethingWithTestObject(TestObject *aIn)
{
    printf("  Doing something with |TestObject| %p.\n",
           static_cast<void*>(aIn));
}

static void DoSomethingWithConstTestObject(const TestObject *aIn)
{
    printf("  Doing something with |const TestObject| %p.\n",
           static_cast<const void*>(aIn));
}

static void DoSomethingWithTestObjectBaseB(TestObjectBaseB *aIn)
{
    printf("  Doing something with |TestObjectBaseB| %p.\n",
           static_cast<void*>(aIn));
}

static void DoSomethingWithConstTestObjectBaseB(const TestObjectBaseB *aIn)
{
    printf("  Doing something with |const TestObjectBaseB| %p.\n",
           static_cast<const void*>(aIn));
}

void test_assignment()
{
  {
    printf("Should create one |TestObject|:\n");
    nsAutoPtr<TestObject> pobj( new TestObject() );
    printf("Should destroy one |TestObject|:\n");
  }

  {
    printf("Should create one |TestObject|:\n");
    nsAutoPtr<TestObject> pobj( new TestObject() );
    printf("Should create one |TestObject| and then destroy one:\n");
    pobj = new TestObject();
    printf("Should destroy one |TestObject|:\n");
  }
}

void test_getter_Transfers()
{
  printf("\nTesting getter_Transfers.\n");

  {
    nsAutoPtr<TestObject> ptr;
    printf("Should create one |TestObject|:\n");
    CreateTestObject(getter_Transfers(ptr));
    printf("Should destroy one |TestObject|:\n");
  }
}

void test_casting()
{
  printf("\nTesting casts and equality tests.\n");

  // This comparison is always false, as it should be. The extra parens
  // suppress a -Wunreachable-code warning about printf being unreachable.
  if (((void*)(TestObject*)0x1000) ==
      ((void*)(TestObjectBaseB*)(TestObject*)0x1000))
    printf("\n\nAll these tests are meaningless!\n\n\n");

  {
    nsAutoPtr<TestObject> p1(new TestObject());
    TestObjectBaseB *p2 = p1;
    printf("equality %s.\n",
        ((static_cast<void*>(p1) != static_cast<void*>(p2)) &&
         (p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1))
        ? "OK" : "broken");
  }

  {
    TestObject *p1 = new TestObject();
    nsAutoPtr<TestObjectBaseB> p2(p1);
    printf("equality %s.\n",
        ((static_cast<void*>(p1) != static_cast<void*>(p2)) &&
         (p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1))
        ? "OK" : "broken");
  }
}

void test_forget()
{
  printf("\nTesting |forget()|.\n");

  {
    printf("Should create one |TestObject|:\n");
    nsAutoPtr<TestObject> pobj( new TestObject() );
    printf("Should do nothing:\n");
    nsAutoPtr<TestObject> pobj2( pobj.forget() );
    printf("Should destroy one |TestObject|:\n");
  }
}

void test_construction()
{
  printf("\nTesting construction.\n");

  {
    printf("Should create one |TestObject|:\n");
    nsAutoPtr<TestObject> pobj(new TestObject());
    printf("Should destroy one |TestObject|:\n");
  }
}

void test_implicit_conversion()
{
  printf("\nTesting calling of functions (including array access and casts).\n");

  {
    printf("Should create one |TestObject|:\n");
    nsAutoPtr<TestObject> pobj(new TestObject());
    printf("Should do something with one |TestObject|:\n");
    DoSomethingWithTestObject(pobj);
    printf("Should do something with one |TestObject|:\n");
    DoSomethingWithConstTestObject(pobj);
    printf("Should destroy one |TestObject|:\n");
  }

  {
    printf("Should create one |TestObject|:\n");
    nsAutoPtr<TestObject> pobj(new TestObject());
    printf("Should do something with one |TestObject|:\n");
    DoSomethingWithTestObjectBaseB(pobj);
    printf("Should do something with one |TestObject|:\n");
    DoSomethingWithConstTestObjectBaseB(pobj);
    printf("Should destroy one |TestObject|:\n");
  }

  {
    printf("Should create one |TestObject|:\n");
    const nsAutoPtr<TestObject> pobj(new TestObject());
    printf("Should do something with one |TestObject|:\n");
    DoSomethingWithTestObject(pobj);
    printf("Should do something with one |TestObject|:\n");
    DoSomethingWithConstTestObject(pobj);
    printf("Should destroy one |TestObject|:\n");
  }

  {
    printf("Should create one |TestObject|:\n");
    const nsAutoPtr<TestObject> pobj(new TestObject());
    printf("Should do something with one |TestObject|:\n");
    DoSomethingWithTestObjectBaseB(pobj);
    printf("Should do something with one |TestObject|:\n");
    DoSomethingWithConstTestObjectBaseB(pobj);
    printf("Should destroy one |TestObject|:\n");
  }
}

void test_arrow_operator()
{
  {
    int test = 1;
    void (TestObjectBaseA::*fPtr)( int, int*, int& ) = &TestObjectBaseA::MemberFunction;
    void (TestObjectBaseA::*fVPtr)( int, int*, int& ) = &TestObjectBaseA::VirtualMemberFunction;
    void (TestObjectBaseA::*fVCPtr)( int, int*, int& ) const = &TestObjectBaseA::VirtualConstMemberFunction;
    printf("Should create one |TestObject|:\n");
    nsAutoPtr<TestObjectBaseA> pobj(new TestObject());
    printf("Should do something with operator->*:\n");
    (pobj->*fPtr)(test, &test, test);
    (pobj->*fVPtr)(test, &test, test);
    (pobj->*fVCPtr)(test, &test, test);
    printf("Should destroy one |TestObject|:\n");
  }
}

int main()
{
  test_assignment();
  test_getter_Transfers();
  test_casting();
  test_forget();
  test_construction();
  test_implicit_conversion();
  test_arrow_operator(); //???

  return 0;
}
