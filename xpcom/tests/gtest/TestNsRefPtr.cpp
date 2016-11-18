/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/RefPtr.h"
#include "nsCOMPtr.h"
#include "nsISupports.h"
#include "nsQueryObject.h"
#include "mozilla/Unused.h"

#include "gtest/gtest.h"

namespace TestNsRefPtr
{

#define NS_FOO_IID \
{ 0x6f7652e0,  0xee43, 0x11d1, \
  { 0x9c, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

class Foo : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_FOO_IID)

public:
  Foo();
  // virtual dtor because Bar uses our Release()
  virtual ~Foo();

  NS_IMETHOD_(MozExternalRefCountType) AddRef() override;
  NS_IMETHOD_(MozExternalRefCountType) Release() override;
  NS_IMETHOD QueryInterface( const nsIID&, void** ) override;
  void MemberFunction( int, int*, int& );
  virtual void VirtualMemberFunction( int, int*, int& );
  virtual void VirtualConstMemberFunction( int, int*, int& ) const;

  void NonconstMethod() {}
  void ConstMethod() const {}

  int refcount_;

  static int total_constructions_;
  static int total_destructions_;
  static int total_addrefs_;
  static int total_queries_;
};

NS_DEFINE_STATIC_IID_ACCESSOR(Foo, NS_FOO_IID)

int Foo::total_constructions_;
int Foo::total_destructions_;
int Foo::total_addrefs_;
int Foo::total_queries_;

Foo::Foo()
  : refcount_(0)
{
  ++total_constructions_;
}

Foo::~Foo()
{
  ++total_destructions_;
}

MozExternalRefCountType
Foo::AddRef()
{
  ++refcount_;
  ++total_addrefs_;
  return refcount_;
}

MozExternalRefCountType
Foo::Release()
{
  int newcount = --refcount_;
  if ( newcount == 0 )
  {
    delete this;
  }

  return newcount;
}

nsresult
Foo::QueryInterface( const nsIID& aIID, void** aResult )
{
  ++total_queries_;

  nsISupports* rawPtr = 0;
  nsresult status = NS_OK;

  if ( aIID.Equals(NS_GET_IID(Foo)) )
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

void
Foo::MemberFunction( int aArg1, int* aArgPtr, int& aArgRef )
{
}

void
Foo::VirtualMemberFunction( int aArg1, int* aArgPtr, int& aArgRef )
{
}

void
Foo::VirtualConstMemberFunction( int aArg1, int* aArgPtr, int& aArgRef ) const
{
}

nsresult
CreateFoo( void** result )
  // a typical factory function (that calls AddRef)
{
  auto* foop = new Foo;

  foop->AddRef();
  *result = foop;

  return NS_OK;
}

void
set_a_Foo( RefPtr<Foo>* result )
{
  assert(result);

  RefPtr<Foo> foop( do_QueryObject(new Foo) );
  *result = foop;
}

RefPtr<Foo>
return_a_Foo()
{
  RefPtr<Foo> foop( do_QueryObject(new Foo) );
  return foop;
}

#define NS_BAR_IID \
{ 0x6f7652e1,  0xee43, 0x11d1, \
  { 0x9c, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

class Bar : public Foo
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_BAR_IID)

public:
  Bar();
  ~Bar() override;

  NS_IMETHOD QueryInterface( const nsIID&, void** ) override;

  void VirtualMemberFunction( int, int*, int& ) override;
  void VirtualConstMemberFunction( int, int*, int& ) const override;

  static int total_constructions_;
  static int total_destructions_;
  static int total_queries_;
};

NS_DEFINE_STATIC_IID_ACCESSOR(Bar, NS_BAR_IID)

int Bar::total_constructions_;
int Bar::total_destructions_;
int Bar::total_queries_;

Bar::Bar()
{
  ++total_constructions_;
}

Bar::~Bar()
{
  ++total_destructions_;
}

nsresult
Bar::QueryInterface( const nsID& aIID, void** aResult )
{
  ++total_queries_;

  nsISupports* rawPtr = 0;
  nsresult status = NS_OK;

  if ( aIID.Equals(NS_GET_IID(Bar)) )
    rawPtr = this;
  else if ( aIID.Equals(NS_GET_IID(Foo)) )
    rawPtr = static_cast<Foo*>(this);
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

void
Bar::VirtualMemberFunction( int aArg1, int* aArgPtr, int& aArgRef )
{
}
void
Bar::VirtualConstMemberFunction( int aArg1, int* aArgPtr, int& aArgRef ) const
{
}

} // namespace TestNsRefPtr

using namespace TestNsRefPtr;

TEST(nsRefPtr, AddRefAndRelease)
{
  Foo::total_constructions_ = 0;
  Foo::total_destructions_ = 0;

  {
    RefPtr<Foo> foop( do_QueryObject(new Foo) );
    ASSERT_EQ(Foo::total_constructions_, 1);
    ASSERT_EQ(Foo::total_destructions_, 0);
    ASSERT_EQ(foop->refcount_, 1);

    foop = do_QueryObject(new Foo);
    ASSERT_EQ(Foo::total_constructions_, 2);
    ASSERT_EQ(Foo::total_destructions_, 1);

    // [Shouldn't compile] Is it a compile time error to try to |AddRef| by hand?
    //foop->AddRef();

    // [Shouldn't compile] Is it a compile time error to try to |Release| be hand?
    //foop->Release();

    // [Shouldn't compile] Is it a compile time error to try to |delete| an |nsCOMPtr|?
    //delete foop;

    static_cast<Foo*>(foop)->AddRef();
    ASSERT_EQ(foop->refcount_, 2);

    static_cast<Foo*>(foop)->Release();
    ASSERT_EQ(foop->refcount_, 1);
  }

  ASSERT_EQ(Foo::total_destructions_, 2);

  {
    RefPtr<Foo> fooP( do_QueryObject(new Foo) );
    ASSERT_EQ(Foo::total_constructions_, 3);
    ASSERT_EQ(Foo::total_destructions_, 2);
    ASSERT_EQ(fooP->refcount_, 1);

    Foo::total_addrefs_ = 0;
    RefPtr<Foo> fooP2( fooP.forget() );
    ASSERT_EQ(Foo::total_addrefs_, 0);
  }
}

TEST(nsRefPtr, VirtualDestructor)
{
  Bar::total_destructions_ = 0;

  {
    RefPtr<Foo> foop( do_QueryObject(new Bar) );
    mozilla::Unused << foop;
  }

  ASSERT_EQ(Bar::total_destructions_, 1);
}

TEST(nsRefPtr, Equality)
{
  Foo::total_constructions_ = 0;
  Foo::total_destructions_ = 0;

  {
    RefPtr<Foo> foo1p( do_QueryObject(new Foo) );
    RefPtr<Foo> foo2p( do_QueryObject(new Foo) );

    ASSERT_EQ(Foo::total_constructions_, 2);
    ASSERT_EQ(Foo::total_destructions_, 0);

    ASSERT_NE(foo1p, foo2p);

    ASSERT_NE(foo1p, nullptr);
    ASSERT_NE(nullptr, foo1p);
    ASSERT_FALSE(foo1p == nullptr);
    ASSERT_FALSE(nullptr == foo1p);

    ASSERT_NE(foo1p, foo2p.get());

    foo1p = foo2p;

    ASSERT_EQ(Foo::total_constructions_, 2);
    ASSERT_EQ(Foo::total_destructions_, 1);
    ASSERT_EQ(foo1p, foo2p);

    ASSERT_EQ(foo2p, foo2p.get());

    ASSERT_EQ(RefPtr<Foo>(foo2p.get()), foo2p);

    ASSERT_TRUE(foo1p);
  }

  ASSERT_EQ(Foo::total_constructions_, 2);
  ASSERT_EQ(Foo::total_destructions_, 2);
}

TEST(nsRefPtr, AddRefHelpers)
{
  Foo::total_addrefs_ = 0;

  {
    auto* raw_foo1p = new Foo;
    raw_foo1p->AddRef();

    auto* raw_foo2p = new Foo;
    raw_foo2p->AddRef();

    ASSERT_EQ(Foo::total_addrefs_, 2);

    RefPtr<Foo> foo1p( dont_AddRef(raw_foo1p) );

    ASSERT_EQ(Foo::total_addrefs_, 2);

    RefPtr<Foo> foo2p;
    foo2p = dont_AddRef(raw_foo2p);

    ASSERT_EQ(Foo::total_addrefs_, 2);
  }

  {
    // Test that various assignment helpers compile.
    RefPtr<Foo> foop;
    CreateFoo( RefPtrGetterAddRefs<Foo>(foop) );
    CreateFoo( getter_AddRefs(foop) );
    set_a_Foo(address_of(foop));
    foop = return_a_Foo();
  }
}

TEST(nsRefPtr, QueryInterface)
{
  Foo::total_queries_ = 0;
  Bar::total_queries_ = 0;

  {
    RefPtr<Foo> fooP;
    fooP = do_QueryObject(new Foo);
    ASSERT_EQ(Foo::total_queries_, 1);
  }

  {
    RefPtr<Foo> fooP;
    fooP = do_QueryObject(new Foo);
    ASSERT_EQ(Foo::total_queries_, 2);

    RefPtr<Foo> foo2P;
    foo2P = fooP;
    ASSERT_EQ(Foo::total_queries_, 2);
  }

  {
    RefPtr<Bar> barP( do_QueryObject(new Bar) );
    ASSERT_EQ(Bar::total_queries_, 1);

    RefPtr<Foo> fooP( do_QueryObject(barP) );
    ASSERT_TRUE(fooP);
    ASSERT_EQ(Foo::total_queries_, 2);
    ASSERT_EQ(Bar::total_queries_, 2);
  }
}

// -------------------------------------------------------------------------
// TODO(ER): The following tests should be moved to MFBT.

#define NS_INLINE_DECL_THREADSAFE_MUTABLE_REFCOUNTING(_class)               \
public:                                                                     \
NS_METHOD_(MozExternalRefCountType) AddRef(void) const {                    \
  MOZ_ASSERT_TYPE_OK_FOR_REFCOUNTING(_class)                                \
  MOZ_RELEASE_ASSERT(int32_t(mRefCnt) >= 0, "illegal refcnt");              \
  nsrefcnt count = ++mRefCnt;                                               \
  return (nsrefcnt) count;                                                  \
}                                                                           \
NS_METHOD_(MozExternalRefCountType) Release(void) const {                   \
  MOZ_RELEASE_ASSERT(int32_t(mRefCnt) > 0, "dup release");                  \
  nsrefcnt count = --mRefCnt;                                               \
  if (count == 0) {                                                         \
    delete (this);                                                          \
    return 0;                                                               \
  }                                                                         \
  return count;                                                             \
}                                                                           \
protected:                                                                  \
mutable ::mozilla::ThreadSafeAutoRefCnt mRefCnt;                            \
public:

class ObjectForConstPtr
{
  private:
    // Reference-counted classes cannot have public destructors.
    ~ObjectForConstPtr() = default;
  public:
    NS_INLINE_DECL_THREADSAFE_MUTABLE_REFCOUNTING(ObjectForConstPtr)
      void ConstMemberFunction( int aArg1, int* aArgPtr, int& aArgRef ) const
      {
      }
};
#undef NS_INLINE_DECL_THREADSAFE_MUTABLE_REFCOUNTING

namespace TestNsRefPtr
{
void AnFooPtrPtrContext(Foo**) { }
void AVoidPtrPtrContext(void**) { }
} // namespace TestNsRefPtr

TEST(nsRefPtr, RefPtrCompilationTests)
{

  {
    RefPtr<Foo> fooP;

    AnFooPtrPtrContext( getter_AddRefs(fooP) );
    AVoidPtrPtrContext( getter_AddRefs(fooP) );
  }

  {
    RefPtr<Foo> fooP(new Foo);
    RefPtr<const Foo> constFooP = fooP;
    constFooP->ConstMethod();

    // [Shouldn't compile] Is it a compile time error to call a non-const method on an |RefPtr<const T>|?
    //constFooP->NonconstMethod();

    // [Shouldn't compile] Is it a compile time error to construct an |RefPtr<T> from an |RefPtr<const T>|?
    //RefPtr<Foo> otherFooP(constFooP);
  }

  {
    RefPtr<Foo> foop = new Foo;
    RefPtr<Foo> foop2 = new Bar;
    RefPtr<const ObjectForConstPtr> foop3 = new ObjectForConstPtr;
    int test = 1;
    void (Foo::*fPtr)( int, int*, int& ) = &Foo::MemberFunction;
    void (Foo::*fVPtr)( int, int*, int& ) = &Foo::VirtualMemberFunction;
    void (Foo::*fVCPtr)( int, int*, int& ) const = &Foo::VirtualConstMemberFunction;
    void (ObjectForConstPtr::*fCPtr2)( int, int*, int& ) const = &ObjectForConstPtr::ConstMemberFunction;

    (foop->*fPtr)(test, &test, test);
    (foop2->*fVPtr)(test, &test, test);
    (foop2->*fVCPtr)(test, &test, test);
    (foop3->*fCPtr2)(test, &test, test);
  }

  // Looks like everything ran.
  ASSERT_TRUE(true);
}
