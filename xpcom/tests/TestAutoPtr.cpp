/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=4:et:sw=4:
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is TestCOMPtrEq.cpp.
 * 
 * The Initial Developer of the Original Code is L. David Baron.
 * Portions created by L. David Baron are Copyright (C) 2001 L. David
 * Baron.  All Rights Reserved.
 * 
 * Contributor(s): 
 *   L. David Baron <dbaron@fas.harvard.edu> (original author)
 */

#include "../base/nsAutoPtr.h"
#include <stdio.h>
#include "nsCom.h"

class TestObjectBaseA {
    public:
        virtual ~TestObjectBaseA() { };
        int fooA;
};

class TestObjectBaseB {
    public:
        virtual ~TestObjectBaseB() { };
        int fooB;
};

class TestObject : public TestObjectBaseA, public TestObjectBaseB {
    public:
        TestObject()
        {
            printf("  Creating TestObject %p.\n",
                   NS_STATIC_CAST(void*, this));
        }

        virtual ~TestObject()
        {
            printf("  Destroying TestObject %p.\n",
                   NS_STATIC_CAST(void*, this));
        }
};

class TestRefObjectBaseA {
    public:
        int fooA;
        virtual void AddRef() const = 0;
        virtual void Release() const = 0;
};

class TestRefObjectBaseB {
    public:
        int fooB;
        virtual void AddRef() const = 0;
        virtual void Release() const = 0;
};

class TestRefObject : public TestRefObjectBaseA, public TestRefObjectBaseB {
    public:
        TestRefObject()
            : mRefCount(0)
        {
            printf("  Creating TestRefObject %p.\n",
                   NS_STATIC_CAST(void*, this));
        }

        ~TestRefObject()
        {
            printf("  Destroying TestRefObject %p.\n",
                   NS_STATIC_CAST(void*, this));
        }

        // These are |const| as a test -- that's not the normal way of
        // implementing |AddRef| and |Release|, but it's a possibility,
        // so we should test it here.
        void AddRef() const
        {
            ++NS_CONST_CAST(TestRefObject*, this)->mRefCount;
            printf("  AddRef to %d on TestRefObject %p.\n",
                   mRefCount, NS_STATIC_CAST(const void*, this));
        }

        void Release() const
        {
            --NS_CONST_CAST(TestRefObject*, this)->mRefCount;
            printf("  Release to %d on TestRefObject %p.\n",
                   mRefCount, NS_STATIC_CAST(const void*, this));
            if (mRefCount == 0)
                delete NS_CONST_CAST(TestRefObject*, this);
        }

    protected:
        PRUint32 mRefCount;

};

void CreateTestObject(TestObject **aResult)
{
    *aResult = new TestObject();
}

void CreateTestRefObject(TestRefObject **aResult)
{
    (*aResult = new TestRefObject())->AddRef();
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

    printf("\nTesting equality:\n");

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
        nsRefPtr<const TestRefObject> p1( new TestRefObject() );
        nsRefPtr<TestRefObject> p2( NS_CONST_CAST(TestRefObject*, p1.get()) );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<const TestRefObject> p1( new TestRefObject() );
        nsRefPtr<TestRefObject> p2( NS_CONST_CAST(TestRefObject*, p1.get()) );
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
        nsRefPtr<const TestRefObject> p1( new TestRefObject() );
        const nsRefPtr<TestRefObject> p2( NS_CONST_CAST(TestRefObject*, p1.get()) );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<const TestRefObject> p1( new TestRefObject() );
        const nsRefPtr<TestRefObject> p2( NS_CONST_CAST(TestRefObject*, p1.get()) );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        nsRefPtr<TestRefObject> p1( new TestRefObject() );
        nsRefPtr<const TestRefObject> p2( p1 );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<TestRefObject> p1( new TestRefObject() );
        nsRefPtr<const TestRefObject> p2( p1 );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        nsRefPtr<const TestRefObject> p1( new TestRefObject() );
        nsRefPtr<const TestRefObject> p2( p1 );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<const TestRefObject> p1( new TestRefObject() );
        nsRefPtr<const TestRefObject> p2( p1 );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        nsRefPtr<TestRefObject> p1( new TestRefObject() );
        const nsRefPtr<const TestRefObject> p2( p1 );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<TestRefObject> p1( new TestRefObject() );
        const nsRefPtr<const TestRefObject> p2( p1 );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        nsRefPtr<const TestRefObject> p1( new TestRefObject() );
        const nsRefPtr<const TestRefObject> p2( p1 );
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<const TestRefObject> p1( new TestRefObject() );
        const nsRefPtr<const TestRefObject> p2( p1 );
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

    {
        nsRefPtr<const TestRefObject> p1( new TestRefObject() );
        TestRefObject * p2 = NS_CONST_CAST(TestRefObject*, p1.get());
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<const TestRefObject> p1( new TestRefObject() );
        TestRefObject * p2 = NS_CONST_CAST(TestRefObject*, p1.get());
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

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

    {
        nsRefPtr<const TestRefObject> p1( new TestRefObject() );
        TestRefObject * const p2 = NS_CONST_CAST(TestRefObject*, p1.get());
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<const TestRefObject> p1( new TestRefObject() );
        TestRefObject * const p2 = NS_CONST_CAST(TestRefObject*, p1.get());
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

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
        nsRefPtr<const TestRefObject> p1( new TestRefObject() );
        const TestRefObject * p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<const TestRefObject> p1( new TestRefObject() );
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

    {
        nsRefPtr<const TestRefObject> p1( new TestRefObject() );
        const TestRefObject * const p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

    {
        const nsRefPtr<const TestRefObject> p1( new TestRefObject() );
        const TestRefObject * const p2 = p1;
        printf("equality %s.\n",
               ((p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1)) ? "OK" : "broken");
    }

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

    if ((void*)(TestObject*)0x1000 ==
        (void*)(TestObjectBaseB*)(TestObject*)0x1000)
        printf("\n\nAll these tests are meaningless!\n\n\n");

    {
        nsAutoPtr<TestObject> p1 = new TestObject();
        TestObjectBaseB *p2 = p1;
        printf("equality %s.\n",
               ((NS_STATIC_CAST(void*, p1) != NS_STATIC_CAST(void*, p2)) &&
                (p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1))
               ? "OK" : "broken");
    }

    {
        TestObject *p1 = new TestObject();
        nsAutoPtr<TestObjectBaseB> p2 = p1;
        printf("equality %s.\n",
               ((NS_STATIC_CAST(void*, p1) != NS_STATIC_CAST(void*, p2)) &&
                (p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1))
               ? "OK" : "broken");
    }

    {
        nsRefPtr<TestRefObject> p1 = new TestRefObject();
        // nsCOMPtr requires a |get| for something like this as well
        nsRefPtr<TestRefObjectBaseB> p2 = p1.get();
        printf("equality %s.\n",
               ((NS_STATIC_CAST(void*, p1) != NS_STATIC_CAST(void*, p2)) &&
                (p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1))
               ? "OK" : "broken");
    }

    {
        nsRefPtr<TestRefObject> p1 = new TestRefObject();
        TestRefObjectBaseB *p2 = p1;
        printf("equality %s.\n",
               ((NS_STATIC_CAST(void*, p1) != NS_STATIC_CAST(void*, p2)) &&
                (p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1))
               ? "OK" : "broken");
    }

    {
        TestRefObject *p1 = new TestRefObject();
        nsRefPtr<TestRefObjectBaseB> p2 = p1;
        printf("equality %s.\n",
               ((NS_STATIC_CAST(void*, p1) != NS_STATIC_CAST(void*, p2)) &&
                (p1 == p2) && !(p1 != p2) && (p2 == p1) && !(p2 != p1))
               ? "OK" : "broken");
    }

    // XXX Need tests of calling functions, with various implicit casts.

    return 0;
}
