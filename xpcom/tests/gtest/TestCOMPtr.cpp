/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsCOMPtr.h"
#include "gtest/gtest.h"

#include "mozilla/Unused.h"

#define NS_IFOO_IID \
{ 0x6f7652e0,  0xee43, 0x11d1, \
  { 0x9c, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

namespace TestCOMPtr
{

class IFoo : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFOO_IID)

public:
  IFoo();
  // virtual dtor because IBar uses our Release()
  virtual ~IFoo();

  NS_IMETHOD_(MozExternalRefCountType) AddRef();
  NS_IMETHOD_(MozExternalRefCountType) Release();
  NS_IMETHOD QueryInterface( const nsIID&, void** );

  unsigned int refcount_;

  static int total_constructions_;
  static int total_destructions_;
  static int total_queries_;
};

NS_DEFINE_STATIC_IID_ACCESSOR(IFoo, NS_IFOO_IID)

int IFoo::total_constructions_;
int IFoo::total_destructions_;
int IFoo::total_queries_;

IFoo::IFoo()
  : refcount_(0)
{
  ++total_constructions_;
}

IFoo::~IFoo()
{
  ++total_destructions_;
}

MozExternalRefCountType
IFoo::AddRef()
{
  ++refcount_;
  return refcount_;
}

MozExternalRefCountType
IFoo::Release()
{
  int newcount = --refcount_;

  if ( newcount == 0 )
  {
    delete this;
  }

  return newcount;
}

nsresult
IFoo::QueryInterface( const nsIID& aIID, void** aResult )
{
  total_queries_++;

  nsISupports* rawPtr = 0;
  nsresult status = NS_OK;

  if ( aIID.Equals(NS_GET_IID(IFoo)) )
    rawPtr = this;
  else
  {
    nsID iid_of_ISupports = NS_ISUPPORTS_IID;
    if ( aIID.Equals(iid_of_ISupports) )
      rawPtr = static_cast<nsISupports*>(this);
    else
      status = NS_ERROR_NO_INTERFACE;
  }

  NS_IF_ADDREF(rawPtr);
  *aResult = rawPtr;

  return status;
}

nsresult
CreateIFoo( void** result )
// a typical factory function (that calls AddRef)
{
  IFoo* foop = new IFoo;

  foop->AddRef();
  *result = foop;

  return NS_OK;
}

void
set_a_IFoo( nsCOMPtr<IFoo>* result )
{
  nsCOMPtr<IFoo> foop( do_QueryInterface(new IFoo) );
  *result = foop;
}

nsCOMPtr<IFoo>
return_a_IFoo()
{
  nsCOMPtr<IFoo> foop( do_QueryInterface(new IFoo) );
  return foop;
}

#define NS_IBAR_IID \
{ 0x6f7652e1,  0xee43, 0x11d1, \
  { 0x9c, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

class IBar : public IFoo
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IBAR_IID)

public:
  IBar();
  virtual ~IBar();

  NS_IMETHOD QueryInterface( const nsIID&, void** );

  static int total_destructions_;
  static int total_queries_;
};

NS_DEFINE_STATIC_IID_ACCESSOR(IBar, NS_IBAR_IID)

int IBar::total_destructions_;
int IBar::total_queries_;

IBar::IBar()
{
}

IBar::~IBar()
{
  total_destructions_++;
}

nsresult
IBar::QueryInterface( const nsID& aIID, void** aResult )
{
  total_queries_++;

  nsISupports* rawPtr = 0;
  nsresult status = NS_OK;

  if ( aIID.Equals(NS_GET_IID(IBar)) )
    rawPtr = this;
  else if ( aIID.Equals(NS_GET_IID(IFoo)) )
    rawPtr = static_cast<IFoo*>(this);
  else
  {
    nsID iid_of_ISupports = NS_ISUPPORTS_IID;
    if ( aIID.Equals(iid_of_ISupports) )
      rawPtr = static_cast<nsISupports*>(this);
    else
      status = NS_ERROR_NO_INTERFACE;
  }

  NS_IF_ADDREF(rawPtr);
  *aResult = rawPtr;

  return status;
}



nsresult
CreateIBar( void** result )
  // a typical factory function (that calls AddRef)
{
  IBar* barp = new IBar;

  barp->AddRef();
  *result = barp;

  return NS_OK;
}

void
AnIFooPtrPtrContext( IFoo** )
{
}

void
AVoidPtrPtrContext( void** )
{
}

void
AnISupportsPtrPtrContext( nsISupports** )
{
}

} // namespace TestCOMPtr

using namespace TestCOMPtr;

TEST(COMPtr, Bloat_Raw_Unsafe)
{
  // ER: I'm not sure what this is testing...
  IBar* barP = 0;
  nsresult rv = CreateIBar(reinterpret_cast<void**>(&barP));
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(barP);

  IFoo* fooP = 0;
  rv = barP->QueryInterface(NS_GET_IID(IFoo), reinterpret_cast<void**>(&fooP));
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(fooP);

  NS_RELEASE(fooP);
  NS_RELEASE(barP);
}

TEST(COMPtr, Bloat_Smart)
{
  // ER: I'm not sure what this is testing...
  nsCOMPtr<IBar> barP;
  nsresult rv = CreateIBar( getter_AddRefs(barP) );
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(barP);

  nsCOMPtr<IFoo> fooP( do_QueryInterface(barP, &rv) );
  ASSERT_TRUE(NS_SUCCEEDED(rv));
  ASSERT_TRUE(fooP);
}

TEST(COMPtr, AddRefAndRelease)
{
  IFoo::total_constructions_ = 0;
  IFoo::total_destructions_ = 0;
  IBar::total_destructions_ = 0;

  {
    nsCOMPtr<IFoo> foop( do_QueryInterface(new IFoo) );
    ASSERT_EQ(foop->refcount_, (unsigned int)1);
    ASSERT_EQ(IFoo::total_constructions_, 1);
    ASSERT_EQ(IFoo::total_destructions_, 0);

    foop = do_QueryInterface(new IFoo);
    ASSERT_EQ(foop->refcount_, (unsigned int)1);
    ASSERT_EQ(IFoo::total_constructions_, 2);
    ASSERT_EQ(IFoo::total_destructions_, 1);

    // [Shouldn't compile] Is it a compile time error to try to |AddRef| by hand?
    //foop->AddRef();

    // [Shouldn't compile] Is it a compile time error to try to |Release| be hand?
    //foop->Release();

    // [Shouldn't compile] Is it a compile time error to try to |delete| an |nsCOMPtr|?
    //delete foop;

    static_cast<IFoo*>(foop)->AddRef();
    ASSERT_EQ(foop->refcount_, (unsigned int)2);
    ASSERT_EQ(IFoo::total_constructions_, 2);
    ASSERT_EQ(IFoo::total_destructions_, 1);

    static_cast<IFoo*>(foop)->Release();
    ASSERT_EQ(foop->refcount_, (unsigned int)1);
    ASSERT_EQ(IFoo::total_constructions_, 2);
    ASSERT_EQ(IFoo::total_destructions_, 1);
  }

  ASSERT_EQ(IFoo::total_constructions_, 2);
  ASSERT_EQ(IFoo::total_destructions_, 2);

  {
    nsCOMPtr<IFoo> foop( do_QueryInterface(new IBar) );
    mozilla::Unused << foop;
  }

  ASSERT_EQ(IBar::total_destructions_, 1);
}

void Comparison()
{
  IFoo::total_constructions_ = 0;
  IFoo::total_destructions_ = 0;

  {
    nsCOMPtr<IFoo> foo1p( do_QueryInterface(new IFoo) );
    nsCOMPtr<IFoo> foo2p( do_QueryInterface(new IFoo) );

    ASSERT_EQ(IFoo::total_constructions_, 2);

    // Test != operator
    ASSERT_NE(foo1p, foo2p);
    ASSERT_NE(foo1p, foo2p.get());

    // Test == operator
    foo1p = foo2p;

    ASSERT_EQ(IFoo::total_destructions_, 1);

    ASSERT_EQ(foo1p, foo2p);
    ASSERT_EQ(foo2p, foo2p.get());
    ASSERT_EQ(foo2p.get(), foo2p);

    // Test () operator
    ASSERT_TRUE(foo1p);

    ASSERT_EQ(foo1p->refcount_, (unsigned int)2);
    ASSERT_EQ(foo2p->refcount_, (unsigned int)2);
  }

  ASSERT_EQ(IFoo::total_destructions_, 2);
}

void DontAddRef()
{
  {
    IFoo* raw_foo1p = new IFoo;
    raw_foo1p->AddRef();

    IFoo* raw_foo2p = new IFoo;
    raw_foo2p->AddRef();

    nsCOMPtr<IFoo> foo1p( dont_AddRef(raw_foo1p) );
    ASSERT_EQ(raw_foo1p, foo1p);
    ASSERT_EQ(foo1p->refcount_, (unsigned int)1);

    nsCOMPtr<IFoo> foo2p;
    foo2p = dont_AddRef(raw_foo2p);
    ASSERT_EQ(raw_foo2p, foo2p);
    ASSERT_EQ(foo2p->refcount_, (unsigned int)1);
  }
}

TEST(COMPtr, AssignmentHelpers)
{
  IFoo::total_constructions_ = 0;
  IFoo::total_destructions_ = 0;

  {
    nsCOMPtr<IFoo> foop;
    ASSERT_FALSE(foop);
    CreateIFoo( nsGetterAddRefs<IFoo>(foop) );
    ASSERT_TRUE(foop);
  }

  ASSERT_EQ(IFoo::total_constructions_, 1);
  ASSERT_EQ(IFoo::total_destructions_, 1);

  {
    nsCOMPtr<IFoo> foop;
    ASSERT_FALSE(foop);
    CreateIFoo( getter_AddRefs(foop) );
    ASSERT_TRUE(foop);
  }

  ASSERT_EQ(IFoo::total_constructions_, 2);
  ASSERT_EQ(IFoo::total_destructions_, 2);

  {
    nsCOMPtr<IFoo> foop;
    ASSERT_FALSE(foop);
    set_a_IFoo(address_of(foop));
    ASSERT_TRUE(foop);

    ASSERT_EQ(IFoo::total_constructions_, 3);
    ASSERT_EQ(IFoo::total_destructions_, 2);

    foop = return_a_IFoo();
    ASSERT_TRUE(foop);

    ASSERT_EQ(IFoo::total_constructions_, 4);
    ASSERT_EQ(IFoo::total_destructions_, 3);
  }

  ASSERT_EQ(IFoo::total_constructions_, 4);
  ASSERT_EQ(IFoo::total_destructions_, 4);

  {
    nsCOMPtr<IFoo> fooP( do_QueryInterface(new IFoo) );
    ASSERT_TRUE(fooP);

    ASSERT_EQ(IFoo::total_constructions_, 5);
    ASSERT_EQ(IFoo::total_destructions_, 4);

    nsCOMPtr<IFoo> fooP2( fooP.forget() );
    ASSERT_TRUE(fooP2);

    ASSERT_EQ(IFoo::total_constructions_, 5);
    ASSERT_EQ(IFoo::total_destructions_, 4);
  }

  ASSERT_EQ(IFoo::total_constructions_, 5);
  ASSERT_EQ(IFoo::total_destructions_, 5);
}

TEST(COMPtr, QueryInterface)
{
  IFoo::total_queries_ = 0;
  IBar::total_queries_ = 0;

  {
    nsCOMPtr<IFoo> fooP;
    ASSERT_FALSE(fooP);
    fooP = do_QueryInterface(new IFoo);
    ASSERT_TRUE(fooP);
    ASSERT_EQ(IFoo::total_queries_, 1);

    nsCOMPtr<IFoo> foo2P;

    // Test that |QueryInterface| _not_ called when assigning a smart-pointer
    // of the same type.);
    foo2P = fooP;
    ASSERT_EQ(IFoo::total_queries_, 1);
  }

  {
    nsCOMPtr<IBar> barP( do_QueryInterface(new IBar) );
    ASSERT_EQ(IBar::total_queries_, 1);

    // Test that |QueryInterface| is called when assigning a smart-pointer of
    // a different type.
    nsCOMPtr<IFoo> fooP( do_QueryInterface(barP) );
    ASSERT_EQ(IBar::total_queries_, 2);
    ASSERT_EQ(IFoo::total_queries_, 1);
    ASSERT_TRUE(fooP);
  }
}

TEST(COMPtr, GetterConversions)
{
  // This is just a compilation test. We add a few asserts to keep gtest happy.
  {
    nsCOMPtr<IFoo> fooP;
    ASSERT_FALSE(fooP);

    AnIFooPtrPtrContext( getter_AddRefs(fooP) );
    AVoidPtrPtrContext( getter_AddRefs(fooP) );
  }


  {
    nsCOMPtr<nsISupports> supportsP;
    ASSERT_FALSE(supportsP);

    AVoidPtrPtrContext( getter_AddRefs(supportsP) );
    AnISupportsPtrPtrContext( getter_AddRefs(supportsP) );
  }
}
