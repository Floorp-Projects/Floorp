/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

/* Invoke tests xptcall. */

#include <stdio.h>
#include "xptcall.h"
#include "prlong.h"

// forward declration
static void DoMultipleInheritenceTest();

// {AAC1FB90-E099-11d2-984E-006008962422}
#define INVOKETESTTARGET_IID \
{ 0xaac1fb90, 0xe099, 0x11d2, \
  { 0x98, 0x4e, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }


class InvokeTestTargetInterface : public nsISupports
{
public:
    NS_IMETHOD AddTwoInts(PRInt32 p1, PRInt32 p2, PRInt32* retval) = 0;
    NS_IMETHOD MultTwoInts(PRInt32 p1, PRInt32 p2, PRInt32* retval) = 0;
    NS_IMETHOD AddTwoLLs(PRInt64 p1, PRInt64 p2, PRInt64* retval) = 0;
    NS_IMETHOD MultTwoLLs(PRInt64 p1, PRInt64 p2, PRInt64* retval) = 0;
};

class InvokeTestTarget : public InvokeTestTargetInterface
{
public:
    NS_DECL_ISUPPORTS
    NS_IMETHOD AddTwoInts(PRInt32 p1, PRInt32 p2, PRInt32* retval);
    NS_IMETHOD MultTwoInts(PRInt32 p1, PRInt32 p2, PRInt32* retval);
    NS_IMETHOD AddTwoLLs(PRInt64 p1, PRInt64 p2, PRInt64* retval);
    NS_IMETHOD MultTwoLLs(PRInt64 p1, PRInt64 p2, PRInt64* retval);

    InvokeTestTarget();
};

static NS_DEFINE_IID(kInvokeTestTargetIID, INVOKETESTTARGET_IID);
NS_IMPL_ISUPPORTS(InvokeTestTarget, kInvokeTestTargetIID);

InvokeTestTarget::InvokeTestTarget()
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

NS_IMETHODIMP
InvokeTestTarget::AddTwoInts(PRInt32 p1, PRInt32 p2, PRInt32* retval)
{
    *retval = p1 + p2;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::MultTwoInts(PRInt32 p1, PRInt32 p2, PRInt32* retval)
{
    *retval = p1 * p2;
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::AddTwoLLs(PRInt64 p1, PRInt64 p2, PRInt64* retval)
{
    LL_ADD(*retval, p1, p2);
    return NS_OK;
}

NS_IMETHODIMP
InvokeTestTarget::MultTwoLLs(PRInt64 p1, PRInt64 p2, PRInt64* retval)
{
    LL_MUL(*retval, p1, p2);
    return NS_OK;
}

int main()
{
    InvokeTestTarget *test = new InvokeTestTarget();

    /* here we make the global 'check for alloc failure' checker happy */
    if(!test)
        return 1;

    PRInt32 out, tmp32 = 0;
    PRInt64 out64;
    printf("calling direct:\n");
    if(NS_SUCCEEDED(test->AddTwoInts(1,1,&out)))
        printf("\t1 + 1 = %d\n", out);
    else
        printf("\tFAILED");
    if(NS_SUCCEEDED(test->AddTwoLLs(LL_INIT(0,1),LL_INIT(0,1),&out64)))
    {
        LL_L2I(tmp32, out64);
        printf("\t1L + 1L = %d\n", (int)tmp32);
    }
    else
        printf("\tFAILED");
    if(NS_SUCCEEDED(test->MultTwoInts(2,2,&out)))
        printf("\t2 * 2 = %d\n", out);
    else
        printf("\tFAILED");
    if(NS_SUCCEEDED(test->MultTwoLLs(LL_INIT(0,2),LL_INIT(0,2),&out64)))
    {
        LL_L2I(tmp32, out64);
        printf("\t2L * 2L = %d\n", (int)tmp32);
    }
    else
        printf("\tFAILED");

    nsXPTCVariant var[3];

    printf("calling via invoke:\n");

    var[0].val.i32 = 1;
    var[0].type = nsXPTType::T_I32;
    var[0].flags = 0;

    var[1].val.i32 = 1;
    var[1].type = nsXPTType::T_I32;
    var[1].flags = 0;

    var[2].val.i32 = 0;
    var[2].type = nsXPTType::T_I32;
    var[2].flags = nsXPTCVariant::PTR_IS_DATA;
    var[2].ptr = &var[2].val.i32;

    if(NS_SUCCEEDED(XPTC_InvokeByIndex(test, 3, 3, var)))
        printf("\t1 + 1 = %d\n", var[2].val.i32);
    else
        printf("\tFAILED");

    LL_I2L(var[0].val.i64, 1);
    var[0].type = nsXPTType::T_I64;
    var[0].flags = 0;

    LL_I2L(var[1].val.i64, 1);
    var[1].type = nsXPTType::T_I64;
    var[1].flags = 0;

    LL_I2L(var[2].val.i64, 0);
    var[2].type = nsXPTType::T_I64;
    var[2].flags = nsXPTCVariant::PTR_IS_DATA;
    var[2].ptr = &var[2].val.i64;

    if(NS_SUCCEEDED(XPTC_InvokeByIndex(test, 5, 3, var)))
        printf("\t1L + 1L = %d\n", (int)var[2].val.i64);
    else
        printf("\tFAILED");

    var[0].val.i32 = 2;
    var[0].type = nsXPTType::T_I32;
    var[0].flags = 0;

    var[1].val.i32 = 2;
    var[1].type = nsXPTType::T_I32;
    var[1].flags = 0;

    var[2].val.i32 = 0;
    var[2].type = nsXPTType::T_I32;
    var[2].flags = nsXPTCVariant::PTR_IS_DATA;
    var[2].ptr = &var[2].val.i32;

    if(NS_SUCCEEDED(XPTC_InvokeByIndex(test, 4, 3, var)))
        printf("\t2 * 2 = %d\n", var[2].val.i32);
    else
        printf("\tFAILED");

    LL_I2L(var[0].val.i64,2);
    var[0].type = nsXPTType::T_I64;
    var[0].flags = 0;

    LL_I2L(var[1].val.i64,2);
    var[1].type = nsXPTType::T_I64;
    var[1].flags = 0;

    LL_I2L(var[2].val.i64,0);
    var[2].type = nsXPTType::T_I64;
    var[2].flags = nsXPTCVariant::PTR_IS_DATA;
    var[2].ptr = &var[2].val.i64;

    if(NS_SUCCEEDED(XPTC_InvokeByIndex(test, 6, 3, var)))
        printf("\t2L * 2L = %d\n", (int)var[2].val.i64);
    else
        printf("\tFAILED");

    DoMultipleInheritenceTest();

    return 0;
}

/***************************************************************************/
/***************************************************************************/
/***************************************************************************/

// {491C65A0-3317-11d3-9885-006008962422}
#define FOO_IID \
{ 0x491c65a0, 0x3317, 0x11d3, \
    { 0x98, 0x85, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }

// {491C65A1-3317-11d3-9885-006008962422}
#define BAR_IID \
{ 0x491c65a1, 0x3317, 0x11d3, \
    { 0x98, 0x85, 0x0, 0x60, 0x8, 0x96, 0x24, 0x22 } }

/***************************/

class nsIFoo : public nsISupports
{
public:
    NS_IMETHOD FooMethod1(PRInt32 i) = 0;
    NS_IMETHOD FooMethod2(PRInt32 i) = 0;
};

class nsIBar : public nsISupports
{
public:
    NS_IMETHOD BarMethod1(PRInt32 i) = 0;
    NS_IMETHOD BarMethod2(PRInt32 i) = 0;
};

/***************************/

class FooImpl : public nsIFoo
{
public:
    NS_IMETHOD FooMethod1(PRInt32 i);
    NS_IMETHOD FooMethod2(PRInt32 i);

    FooImpl();
    virtual ~FooImpl();

    virtual char* ImplName() = 0;

    int SomeData1;
    int SomeData2;
    char* Name;
};

class BarImpl : public nsIBar
{
public:
    NS_IMETHOD BarMethod1(PRInt32 i);
    NS_IMETHOD BarMethod2(PRInt32 i);

    BarImpl();
    virtual ~BarImpl();

    virtual char * ImplName() = 0;

    int SomeData1;
    int SomeData2;
    char* Name;
};

/***************************/

FooImpl::FooImpl() : Name("FooImpl")
{
}
FooImpl::~FooImpl()
{
}

NS_IMETHODIMP FooImpl::FooMethod1(PRInt32 i)
{
    printf("\tFooImpl::FooMethod1 called with i == %d, %s part of a %s\n", 
           i, Name, ImplName());
    return NS_OK;
}

NS_IMETHODIMP FooImpl::FooMethod2(PRInt32 i)
{
    printf("\tFooImpl::FooMethod2 called with i == %d, %s part of a %s\n", 
           i, Name, ImplName());
    return NS_OK;
}

/***************************/

BarImpl::BarImpl() : Name("BarImpl")
{
}
BarImpl::~BarImpl()
{
}

NS_IMETHODIMP BarImpl::BarMethod1(PRInt32 i)
{
    printf("\tBarImpl::BarMethod1 called with i == %d, %s part of a %s\n", 
           i, Name, ImplName());
    return NS_OK;
}

NS_IMETHODIMP BarImpl::BarMethod2(PRInt32 i)
{
    printf("\tBarImpl::BarMethod2 called with i == %d, %s part of a %s\n", 
           i, Name, ImplName());
    return NS_OK;
}

/***************************/

class FooBarImpl : FooImpl, BarImpl
{
public:
    NS_DECL_ISUPPORTS

    char* ImplName();

    FooBarImpl();
    virtual ~FooBarImpl();
    char* MyName;
};

FooBarImpl::FooBarImpl() : MyName("FooBarImpl")
{
    NS_INIT_REFCNT();
    NS_ADDREF_THIS();
}

FooBarImpl::~FooBarImpl()
{
}

char* FooBarImpl::ImplName()
{
    return MyName;
}

static NS_DEFINE_IID(kFooIID, FOO_IID);
static NS_DEFINE_IID(kBarIID, BAR_IID);

NS_IMETHODIMP
FooBarImpl::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtr = NULL;


  if (aIID.Equals(kFooIID)) {
    *aInstancePtr = (void*) NS_STATIC_CAST(nsIFoo*,this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kBarIID)) {
    *aInstancePtr = (void*) NS_STATIC_CAST(nsIBar*,this);
    NS_ADDREF_THIS();
    return NS_OK;
  }

  if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID())) {
    *aInstancePtr = (void*) NS_STATIC_CAST(nsISupports*,
                                           NS_STATIC_CAST(nsIFoo*,this));
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(FooBarImpl)
NS_IMPL_RELEASE(FooBarImpl)


static void DoMultipleInheritenceTest()
{
    FooBarImpl* impl = new FooBarImpl();
    if(!impl)
        return;

    nsIFoo* foo;
    nsIBar* bar;

    nsXPTCVariant var[1];

    printf("\n");
    if(NS_SUCCEEDED(impl->QueryInterface(kFooIID, (void**)&foo)) &&
       NS_SUCCEEDED(impl->QueryInterface(kBarIID, (void**)&bar)))
    {
        printf("impl == %x\n", impl);
        printf("foo  == %x\n", foo);
        printf("bar  == %x\n", bar);

        printf("Calling Foo...\n");
        printf("direct calls:\n");
        foo->FooMethod1(1);
        foo->FooMethod2(2);

        printf("invoke calls:\n");
        var[0].val.i32 = 1;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        XPTC_InvokeByIndex(foo, 3, 1, var);

        var[0].val.i32 = 2;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        XPTC_InvokeByIndex(foo, 4, 1, var);

        printf("\n");

        printf("Calling Bar...\n");
        printf("direct calls:\n");
        bar->BarMethod1(1);
        bar->BarMethod2(2);

        printf("invoke calls:\n");
        var[0].val.i32 = 1;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        XPTC_InvokeByIndex(bar, 3, 1, var);

        var[0].val.i32 = 2;
        var[0].type = nsXPTType::T_I32;
        var[0].flags = 0;
        XPTC_InvokeByIndex(bar, 4, 1, var);

        printf("\n");

        NS_RELEASE(foo);
        NS_RELEASE(bar);
    }
    NS_RELEASE(impl);
}
