/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAutoPtr.h"
#include <stdio.h>
#include "nscore.h"

class TestObjectBaseA {
    public:
        // Virtual dtor for deleting through base class pointer
        virtual ~TestObjectBaseA() { };
        int fooA;
};

class TestObjectBaseB {
    public:
        // Virtual dtor for deleting through base class pointer
        virtual ~TestObjectBaseB() { };
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

class TestRefObject : public TestRefObjectBaseA, public TestRefObjectBaseB {
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
        PRUint32 mRefCount;

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
        printf("Should create 3 |TestObject|s:\n");
        nsAutoArrayPtr<TestObject> pobj( new TestObject[3] );
        printf("Should create 5 |TestObject|s and then destroy 3:\n");
        pobj = new TestObject[5];
        printf("Should destroy 5 |TestObject|s:\n");
    }

    {
        printf("Should create and AddRef one |TestRefObject|:\n");
        nsRefPtr<TestRefObject> pobj( new TestRefObject() );
        printf("Should Release and destroy one |TestRefObject|:\n");
    }

    {
        printf("Should create and AddRef one |TestRefObject|:\n");
        nsRefPtr<TestRefObject> pobj( new TestRefObject() );
        printf("Should create and AddRef one |TestRefObject| and then Release and destroy one:\n");
        pobj = new TestRefObject();
        printf("Should Release and destroy one |TestRefObject|:\n");
    }

    {
        printf("Should create and AddRef one |TestRefObject|:\n");
        nsRefPtr<TestRefObject> p1( new TestRefObject() );
        printf("Should AddRef one |TestRefObject|:\n");
        nsRefPtr<TestRefObject> p2( p1 );
        printf("Should Release twice and destroy one |TestRefObject|:\n");
    }

    printf("\nTesting equality (with all const-ness combinations):\n");

    {
        nsRefPtr<TestRefObject> p1( new TestRefObject() );
        nsRefPtr<TestRefObject> p2( p1 );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<TestRefObject> p1( new TestRefObject() );
        nsRefPtr<TestRefObject> p2( p1 );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        nsRefPtr<TestRefObject> p1( new TestRefObject() );
        const nsRefPtr<TestRefObject> p2( p1 );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<TestRefObject> p1( new TestRefObject() );
        const nsRefPtr<TestRefObject> p2( p1 );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        nsRefPtr<TestRefObject> p1( new TestRefObject() );
        TestRefObject * p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<TestRefObject> p1( new TestRefObject() );
        TestRefObject * p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

#if 0 /* MSVC++ 6.0 can't be coaxed to accept this */
    {
        nsRefPtr<TestRefObject> p1( new TestRefObject() );
        TestRefObject * const p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<TestRefObject> p1( new TestRefObject() );
        TestRefObject * const p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }
#endif /* Things that MSVC++ 6.0 can't be coaxed to accept */

    {
        nsRefPtr<TestRefObject> p1( new TestRefObject() );
        const TestRefObject * p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<TestRefObject> p1( new TestRefObject() );
        const TestRefObject * p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

    {
        nsRefPtr<TestRefObject> p1( new TestRefObject() );
        const TestRefObject * const p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<TestRefObject> p1( new TestRefObject() );
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
        nsRefPtr<TestRefObject> ptr;
        printf("Should create and AddRef one |TestRefObject|:\n");
        CreateTestRefObject(getter_AddRefs(ptr));
        printf("Should Release and destroy one |TestRefObject|:\n");
    }

    printf("\nTesting casts and equality tests.\n");

    if ((void*)(TestObject*)0x1000 ==
        (void*)(TestObjectBaseB*)(TestObject*)0x1000)
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
        nsRefPtr<TestRefObject> p1 = new TestRefObject();
        // nsCOMPtr requires a |get| for something like this as well
        nsRefPtr<TestRefObjectBaseB> p2 = p1.get();
        printf("equality %s.\n",
               ((static_cast<void*>(p1) != static_cast<void*>(p2)) &&
                (p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1))
               ? "OK" : "broken");
    }

    {
        nsRefPtr<TestRefObject> p1 = new TestRefObject();
        TestRefObjectBaseB *p2 = p1;
        printf("equality %s.\n",
               ((static_cast<void*>(p1) != static_cast<void*>(p2)) &&
                (p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1))
               ? "OK" : "broken");
    }

    {
        TestRefObject *p1 = new TestRefObject();
        nsRefPtr<TestRefObjectBaseB> p2 = p1;
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
        printf("Should create 3 |TestObject|s:\n");
        nsAutoArrayPtr<TestObject> pobj( new TestObject[3] );
        printf("Should do nothing:\n");
        nsAutoArrayPtr<TestObject> pobj2( pobj.forget() );
        printf("Should destroy 3 |TestObject|s:\n");
    }

    {
        printf("Should create one |TestRefObject|:\n");
        nsRefPtr<TestRefObject> pobj( new TestRefObject() );
        printf("Should do nothing:\n");
        nsRefPtr<TestRefObject> pobj2( pobj.forget() );
        printf("Should destroy one |TestRefObject|:\n");
    }


    printf("\nTesting construction.\n");

    {
        printf("Should create one |TestObject|:\n");
        nsAutoPtr<TestObject> pobj(new TestObject());
        printf("Should destroy one |TestObject|:\n");
    }

    {
        printf("Should create 3 |TestObject|s:\n");
        nsAutoArrayPtr<TestObject> pobj(new TestObject[3]);
        printf("Should destroy 3 |TestObject|s:\n");
    }

    {
        printf("Should create and AddRef one |TestRefObject|:\n");
        nsRefPtr<TestRefObject> pobj = new TestRefObject();
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
        printf("Should create 3 |TestObject|s:\n");
        nsAutoArrayPtr<TestObject> pobj(new TestObject[3]);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithTestObject(&pobj[2]);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithConstTestObject(&pobj[1]);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithTestObject(pobj + 2);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithConstTestObject(pobj + 1);
        printf("Should destroy 3 |TestObject|s:\n");
    }

    {
        printf("Should create and AddRef one |TestRefObject|:\n");
        nsRefPtr<TestRefObject> pobj = new TestRefObject();
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
        printf("Should create 3 |TestObject|s:\n");
        nsAutoArrayPtr<TestObject> pobj(new TestObject[3]);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithTestObjectBaseB(&pobj[2]);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithConstTestObjectBaseB(&pobj[1]);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithTestObjectBaseB(pobj + 2);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithConstTestObjectBaseB(pobj + 1);
        printf("Should destroy 3 |TestObject|s:\n");
    }

    {
        printf("Should create and AddRef one |TestRefObject|:\n");
        nsRefPtr<TestRefObject> pobj = new TestRefObject();
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
        printf("Should create 3 |TestObject|s:\n");
        const nsAutoArrayPtr<TestObject> pobj(new TestObject[3]);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithTestObject(&pobj[2]);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithConstTestObject(&pobj[1]);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithTestObject(pobj + 2);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithConstTestObject(pobj + 1);
        printf("Should destroy 3 |TestObject|s:\n");
    }

    {
        printf("Should create and AddRef one |TestRefObject|:\n");
        const nsRefPtr<TestRefObject> pobj = new TestRefObject();
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
        printf("Should create 3 |TestObject|s:\n");
        const nsAutoArrayPtr<TestObject> pobj(new TestObject[3]);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithTestObjectBaseB(&pobj[2]);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithConstTestObjectBaseB(&pobj[1]);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithTestObjectBaseB(pobj + 2);
        printf("Should do something with one |TestObject|:\n");
        DoSomethingWithConstTestObjectBaseB(pobj + 1);
        printf("Should destroy 3 |TestObject|s:\n");
    }

    {
        printf("Should create and AddRef one |TestRefObject|:\n");
        const nsRefPtr<TestRefObject> pobj = new TestRefObject();
        printf("Should do something with one |TestRefObject|:\n");
        DoSomethingWithTestRefObjectBaseB(pobj);
        printf("Should do something with one |TestRefObject|:\n");
        DoSomethingWithConstTestRefObjectBaseB(pobj);
        printf("Should Release and destroy one |TestRefObject|:\n");
    }

    return 0;
}
