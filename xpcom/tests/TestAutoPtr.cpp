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

class TestRefObjectBaseA {
    public:
        int fooA;
        // Must return |nsrefcnt| to keep |nsDerivedSafe| happy.
        virtual nsrefcnt AddRef() = 0;
        virtual nsrefcnt Release() = 0;
};

class TestRefObjectBaseB {
    public:
        int fooB;
        virtual nsrefcnt AddRef() = 0;
        virtual nsrefcnt Release() = 0;
};

class TestRefObject final : public TestRefObjectBaseA, public TestRefObjectBaseB {
    public:
        TestRefObject()
            : mRefCount(0)
        {
            printf("  Creating TestRefObject %p.\n",
                   static_cast<void*>(this));
        }

        ~TestRefObject()
        {
            printf("  Destroying TestRefObject %p.\n",
                   static_cast<void*>(this));
        }

        nsrefcnt AddRef()
        {
            ++mRefCount;
            printf("  AddRef to %d on TestRefObject %p.\n",
                   mRefCount, static_cast<void*>(this));
            return mRefCount;
        }

        nsrefcnt Release()
        {
            --mRefCount;
            printf("  Release to %d on TestRefObject %p.\n",
                   mRefCount, static_cast<void*>(this));
            if (mRefCount == 0) {
                delete const_cast<TestRefObject*>(this);
                return 0;
            }
            return mRefCount;
        }

    protected:
        uint32_t mRefCount;

};

static void CreateTestObject(TestObject **aResult)
{
    *aResult = new TestObject();
}

static void CreateTestRefObject(TestRefObject **aResult)
{
    (*aResult = new TestRefObject())->AddRef();
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

static void DoSomethingWithTestRefObject(TestRefObject *aIn)
{
    printf("  Doing something with |TestRefObject| %p.\n",
           static_cast<void*>(aIn));
}

static void DoSomethingWithConstTestRefObject(const TestRefObject *aIn)
{
    printf("  Doing something with |const TestRefObject| %p.\n",
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

static void DoSomethingWithTestRefObjectBaseB(TestRefObjectBaseB *aIn)
{
    printf("  Doing something with |TestRefObjectBaseB| %p.\n",
           static_cast<void*>(aIn));
}

static void DoSomethingWithConstTestRefObjectBaseB(const TestRefObjectBaseB *aIn)
{
    printf("  Doing something with |const TestRefObjectBaseB| %p.\n",
           static_cast<const void*>(aIn));
}

int main()
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

    {
        printf("Should create and AddRef one |TestRefObject|:\n");
        RefPtr<TestRefObject> pobj( new TestRefObject() );
        printf("Should Release and destroy one |TestRefObject|:\n");
    }

    {
        printf("Should create and AddRef one |TestRefObject|:\n");
        RefPtr<TestRefObject> pobj( new TestRefObject() );
        printf("Should create and AddRef one |TestRefObject| and then Release and destroy one:\n");
        pobj = new TestRefObject();
        printf("Should Release and destroy one |TestRefObject|:\n");
    }

    {
        printf("Should create and AddRef one |TestRefObject|:\n");
        RefPtr<TestRefObject> p1( new TestRefObject() );
        printf("Should AddRef one |TestRefObject|:\n");
        RefPtr<TestRefObject> p2( p1 );
        printf("Should Release twice and destroy one |TestRefObject|:\n");
    }

    printf("\nTesting equality (with all const-ness combinations):\n");

    {
        RefPtr<TestRefObject> p1( new TestRefObject() );
        RefPtr<TestRefObject> p2( p1 );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        const RefPtr<TestRefObject> p1( new TestRefObject() );
        RefPtr<TestRefObject> p2( p1 );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        RefPtr<TestRefObject> p1( new TestRefObject() );
        const RefPtr<TestRefObject> p2( p1 );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        const RefPtr<TestRefObject> p1( new TestRefObject() );
        const RefPtr<TestRefObject> p2( p1 );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        RefPtr<TestRefObject> p1( new TestRefObject() );
        TestRefObject * p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

    {
        const RefPtr<TestRefObject> p1( new TestRefObject() );
        TestRefObject * p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

#if 0 /* MSVC++ 6.0 can't be coaxed to accept this */
    {
        RefPtr<TestRefObject> p1( new TestRefObject() );
        TestRefObject * const p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

    {
        const RefPtr<TestRefObject> p1( new TestRefObject() );
        TestRefObject * const p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }
#endif /* Things that MSVC++ 6.0 can't be coaxed to accept */

    {
        RefPtr<TestRefObject> p1( new TestRefObject() );
        const TestRefObject * p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

    {
        const RefPtr<TestRefObject> p1( new TestRefObject() );
        const TestRefObject * p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

    {
        RefPtr<TestRefObject> p1( new TestRefObject() );
        const TestRefObject * const p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

    {
        const RefPtr<TestRefObject> p1( new TestRefObject() );
        const TestRefObject * const p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

    printf("\nTesting getter_Transfers and getter_AddRefs.\n");

    {
        nsAutoPtr<TestObject> ptr;
        printf("Should create one |TestObject|:\n");
        CreateTestObject(getter_Transfers(ptr));
        printf("Should destroy one |TestObject|:\n");
    }

    {
        RefPtr<TestRefObject> ptr;
        printf("Should create and AddRef one |TestRefObject|:\n");
        CreateTestRefObject(getter_AddRefs(ptr));
        printf("Should Release and destroy one |TestRefObject|:\n");
    }

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

    {
        RefPtr<TestRefObject> p1 = new TestRefObject();
        // nsCOMPtr requires a |get| for something like this as well
        RefPtr<TestRefObjectBaseB> p2 = p1.get();
        printf("equality %s.\n",
               ((static_cast<void*>(p1) != static_cast<void*>(p2)) &&
                (p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1))
               ? "OK" : "broken");
    }

    {
        RefPtr<TestRefObject> p1 = new TestRefObject();
        TestRefObjectBaseB *p2 = p1;
        printf("equality %s.\n",
               ((static_cast<void*>(p1) != static_cast<void*>(p2)) &&
                (p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1))
               ? "OK" : "broken");
    }

    {
        TestRefObject *p1 = new TestRefObject();
        RefPtr<TestRefObjectBaseB> p2 = p1;
        printf("equality %s.\n",
               ((static_cast<void*>(p1) != static_cast<void*>(p2)) &&
                (p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1))
               ? "OK" : "broken");
    }

    printf("\nTesting |forget()|.\n");

    {
        printf("Should create one |TestObject|:\n");
        nsAutoPtr<TestObject> pobj( new TestObject() );
        printf("Should do nothing:\n");
        nsAutoPtr<TestObject> pobj2( pobj.forget() );
        printf("Should destroy one |TestObject|:\n");
    }

    {
        printf("Should create one |TestRefObject|:\n");
        RefPtr<TestRefObject> pobj( new TestRefObject() );
        printf("Should do nothing:\n");
        RefPtr<TestRefObject> pobj2( pobj.forget() );
        printf("Should destroy one |TestRefObject|:\n");
    }

    printf("\nTesting construction.\n");

    {
        printf("Should create one |TestObject|:\n");
        nsAutoPtr<TestObject> pobj(new TestObject());
        printf("Should destroy one |TestObject|:\n");
    }

    {
        printf("Should create and AddRef one |TestRefObject|:\n");
        RefPtr<TestRefObject> pobj = new TestRefObject();
        printf("Should Release and destroy one |TestRefObject|:\n");
    }

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
        printf("Should create and AddRef one |TestRefObject|:\n");
        RefPtr<TestRefObject> pobj = new TestRefObject();
        printf("Should do something with one |TestRefObject|:\n");
        DoSomethingWithTestRefObject(pobj);
        printf("Should do something with one |TestRefObject|:\n");
        DoSomethingWithConstTestRefObject(pobj);
        printf("Should Release and destroy one |TestRefObject|:\n");
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
        printf("Should create and AddRef one |TestRefObject|:\n");
        RefPtr<TestRefObject> pobj = new TestRefObject();
        printf("Should do something with one |TestRefObject|:\n");
        DoSomethingWithTestRefObjectBaseB(pobj);
        printf("Should do something with one |TestRefObject|:\n");
        DoSomethingWithConstTestRefObjectBaseB(pobj);
        printf("Should Release and destroy one |TestRefObject|:\n");
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
        printf("Should create and AddRef one |TestRefObject|:\n");
        const RefPtr<TestRefObject> pobj = new TestRefObject();
        printf("Should do something with one |TestRefObject|:\n");
        DoSomethingWithTestRefObject(pobj);
        printf("Should do something with one |TestRefObject|:\n");
        DoSomethingWithConstTestRefObject(pobj);
        printf("Should Release and destroy one |TestRefObject|:\n");
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

    {
        printf("Should create and AddRef one |TestRefObject|:\n");
        const RefPtr<TestRefObject> pobj = new TestRefObject();
        printf("Should do something with one |TestRefObject|:\n");
        DoSomethingWithTestRefObjectBaseB(pobj);
        printf("Should do something with one |TestRefObject|:\n");
        DoSomethingWithConstTestRefObjectBaseB(pobj);
        printf("Should Release and destroy one |TestRefObject|:\n");
    }

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

    return 0;
}
