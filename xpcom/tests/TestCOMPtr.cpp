/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <stdio.h>
#include "nsCOMPtr.h"
#include "nsISupports.h"

#define NS_IFOO_IID \
{ 0x6f7652e0,  0xee43, 0x11d1, \
 { 0x9c, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

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

      static void print_totals();

    private:
      unsigned int refcount_;

      static unsigned int total_constructions_;
      static unsigned int total_destructions_;
  };

NS_DEFINE_STATIC_IID_ACCESSOR(IFoo, NS_IFOO_IID)

class IBar;

  // some types I'll need
typedef unsigned long NS_RESULT;

  // some functions I'll need (and define below)
          nsresult  CreateIFoo( void** );
          nsresult  CreateIBar( void** result );
              void  AnIFooPtrPtrContext( IFoo** );
              void	AnISupportsPtrPtrContext( nsISupports** );
              void  AVoidPtrPtrContext( void** );
              void  set_a_IFoo( nsCOMPtr<IFoo>* result );
nsCOMPtr<IFoo>  return_a_IFoo();




unsigned int IFoo::total_constructions_;
unsigned int IFoo::total_destructions_;

class test_message
  {
    public:
      test_message()
        {
          printf("BEGIN unit tests for |nsCOMPtr|, compiled " __DATE__ "\n");
        }

     ~test_message()
        {
          IFoo::print_totals();
          printf("END unit tests for |nsCOMPtr|.\n");
        }
  };

test_message gTestMessage;


  /*
    ...
  */

void
IFoo::print_totals()
  {
    printf("total constructions/destructions --> %d/%d\n", 
           total_constructions_, total_destructions_);
  }

IFoo::IFoo()
    : refcount_(0)
  {
    ++total_constructions_;
    printf("  new IFoo@%p [#%d]\n",
           static_cast<void*>(this), total_constructions_);
  }

IFoo::~IFoo()
  {
    ++total_destructions_;
    printf("IFoo@%p::~IFoo() [#%d]\n",
           static_cast<void*>(this), total_destructions_);
  }

MozExternalRefCountType
IFoo::AddRef()
  {
    ++refcount_;
    printf("IFoo@%p::AddRef(), refcount --> %d\n", 
           static_cast<void*>(this), refcount_);
    return refcount_;
  }

MozExternalRefCountType
IFoo::Release()
  {
    int newcount = --refcount_;
    if ( newcount == 0 )
      printf(">>");

    printf("IFoo@%p::Release(), refcount --> %d\n",
           static_cast<void*>(this), refcount_);

    if ( newcount == 0 )
      {
        printf("  delete IFoo@%p\n", static_cast<void*>(this));
        printf("<<IFoo@%p::Release()\n", static_cast<void*>(this));
        delete this;
      }

    return newcount;
  }

nsresult
IFoo::QueryInterface( const nsIID& aIID, void** aResult )
	{
    printf("IFoo@%p::QueryInterface()\n", static_cast<void*>(this));
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
    printf(">>CreateIFoo() --> ");
    IFoo* foop = new IFoo;
    printf("IFoo@%p\n", static_cast<void*>(foop));

    foop->AddRef();
    *result = foop;

    printf("<<CreateIFoo()\n");
    return NS_OK;
  }

void
set_a_IFoo( nsCOMPtr<IFoo>* result )
  {
    printf(">>set_a_IFoo()\n");
    assert(result);

    nsCOMPtr<IFoo> foop( do_QueryInterface(new IFoo) );
    *result = foop;
    printf("<<set_a_IFoo()\n");
  }

nsCOMPtr<IFoo>
return_a_IFoo()
  {
    printf(">>return_a_IFoo()\n");
    nsCOMPtr<IFoo> foop( do_QueryInterface(new IFoo) );
    printf("<<return_a_IFoo()\n");
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
  };

NS_DEFINE_STATIC_IID_ACCESSOR(IBar, NS_IBAR_IID)

IBar::IBar()
  {
    printf("  new IBar@%p\n", static_cast<void*>(this));
  }

IBar::~IBar()
  {
    printf("IBar@%p::~IBar()\n", static_cast<void*>(this));
  }

nsresult
IBar::QueryInterface( const nsID& aIID, void** aResult )
	{
    printf("IBar@%p::QueryInterface()\n", static_cast<void*>(this));
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
    printf(">>CreateIBar() --> ");
    IBar* barp = new IBar;
    printf("IBar@%p\n", static_cast<void*>(barp));

    barp->AddRef();
    *result = barp;

    printf("<<CreateIBar()\n");
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

static
nsresult
TestBloat_Raw_Unsafe()
	{
		IBar* barP = 0;
		nsresult result = CreateIBar(reinterpret_cast<void**>(&barP));

		if ( barP )
			{
				IFoo* fooP = 0;
				if ( NS_SUCCEEDED( result = barP->QueryInterface(NS_GET_IID(IFoo), reinterpret_cast<void**>(&fooP)) ) )
					{
						fooP->print_totals();
						NS_RELEASE(fooP);
					}

				NS_RELEASE(barP);
			}

		return result;
	}


static
nsresult
TestBloat_Smart()
	{
		nsCOMPtr<IBar> barP;
		nsresult result = CreateIBar( getter_AddRefs(barP) );

		nsCOMPtr<IFoo> fooP( do_QueryInterface(barP, &result) );

		if ( fooP )
			fooP->print_totals();

		return result;
	}




nsCOMPtr<IFoo> gFoop;

int
main()
  {
    printf(">>main()\n");

		printf("sizeof(nsCOMPtr<IFoo>) --> %u\n", unsigned(sizeof(nsCOMPtr<IFoo>)));

		TestBloat_Raw_Unsafe();
		TestBloat_Smart();


    {
      printf("\n### Test  1: will a |nsCOMPtr| call |AddRef| on a pointer assigned into it?\n");
      nsCOMPtr<IFoo> foop( do_QueryInterface(new IFoo) );

      printf("\n### Test  2: will a |nsCOMPtr| |Release| its old pointer when a new one is assigned in?\n");
      foop = do_QueryInterface(new IFoo);

        // [Shouldn't compile] Is it a compile time error to try to |AddRef| by hand?
      //foop->AddRef();

        // [Shouldn't compile] Is it a compile time error to try to |Release| be hand?
      //foop->Release();

				// [Shouldn't compile] Is it a compile time error to try to |delete| an |nsCOMPtr|?
			//delete foop;

      printf("\n### Test  3: can you |AddRef| if you must?\n");
      static_cast<IFoo*>(foop)->AddRef();

      printf("\n### Test  4: can you |Release| if you must?\n");
      static_cast<IFoo*>(foop)->Release();

      printf("\n### Test  5: will a |nsCOMPtr| |Release| when it goes out of scope?\n");
    }

    {
      printf("\n### Test  6: will a |nsCOMPtr| call the correct destructor?\n");
      nsCOMPtr<IFoo> foop( do_QueryInterface(new IBar) );
    }

    {
      printf("\n### Test  7: can you compare one |nsCOMPtr| with another [!=]?\n");

      nsCOMPtr<IFoo> foo1p( do_QueryInterface(new IFoo) );

        // [Shouldn't compile] Is it a compile time error to omit |getter_[doesnt_]AddRef[s]|?
      //AnIFooPtrPtrContext(&foo1p);

        // [Shouldn't compile] Is it a compile time error to omit |getter_[doesnt_]AddRef[s]|?
      //AVoidPtrPtrContext(&foo1p);

      nsCOMPtr<IFoo> foo2p( do_QueryInterface(new IFoo) );

      if ( foo1p != foo2p )
        printf("foo1p != foo2p\n");
      else
        printf("foo1p == foo2p\n");


      IFoo* raw_foo2p = foo2p.get();

      printf("\n### Test  8: can you compare a |nsCOMPtr| with a raw interface pointer [!=]?\n");
      if ( foo1p.get() != raw_foo2p )
        printf("foo1p != raw_foo2p\n");
      else
        printf("foo1p == raw_foo2p\n");


      printf("\n### Test  9: can you assign one |nsCOMPtr| into another?\n");
      foo1p = foo2p;

      printf("\n### Test 10: can you compare one |nsCOMPtr| with another [==]?\n");
      if ( foo1p == foo2p )
        printf("foo1p == foo2p\n");
      else
        printf("foo1p != foo2p\n");

      printf("\n### Test 11: can you compare a |nsCOMPtr| with a raw interface pointer [==]?\n");
      if ( raw_foo2p == foo2p.get() )
        printf("raw_foo2p == foo2p\n");
      else
        printf("raw_foo2p != foo2p\n");

#if 1
      printf("\n### Test 11.5: can you compare a |nsCOMPtr| with a raw interface pointer [==]?\n");
      if ( nsCOMPtr<IFoo>( raw_foo2p ) == foo2p )
        printf("raw_foo2p == foo2p\n");
      else
        printf("raw_foo2p != foo2p\n");
#endif

      printf("\n### Test 12: bare pointer test?\n");
      if ( foo1p )
        printf("foo1p is not NULL\n");
      else
        printf("foo1p is NULL\n");

#if 0
			if ( foo1p == 1 )
				printf("foo1p allowed compare with in\n");
#endif

      printf("\n### Test 13: how about when two |nsCOMPtr|s referring to the same object go out of scope?\n");
    }

    {
      printf("\n### Test 14,15 ...setup...\n");
      IFoo* raw_foo1p = new IFoo;
      raw_foo1p->AddRef();

      IFoo* raw_foo2p = new IFoo;
      raw_foo2p->AddRef();

      printf("\n### Test 14: what if I don't want to |AddRef| when I construct?\n");
      nsCOMPtr<IFoo> foo1p( dont_AddRef(raw_foo1p) );
      //nsCOMPtr<IFoo> foo1p = dont_AddRef(raw_foo1p);

      printf("\n### Test 15: what if I don't want to |AddRef| when I assign in?\n");
      nsCOMPtr<IFoo> foo2p;
      foo2p = dont_AddRef(raw_foo2p);
    }







    {
      printf("\n### setup for Test 16\n");
      nsCOMPtr<IFoo> foop;
      printf("### Test 16: basic parameter behavior?\n");
      CreateIFoo( nsGetterAddRefs<IFoo>(foop) );
    }
    printf("### End Test 16\n");


    {
      printf("\n### setup for Test 17\n");
      nsCOMPtr<IFoo> foop;
      printf("### Test 17: basic parameter behavior, using the short form?\n");
      CreateIFoo( getter_AddRefs(foop) );
    }
    printf("### End Test 17\n");


    {
      printf("\n### setup for Test 18, 19\n");
      nsCOMPtr<IFoo> foop;
      printf("### Test 18: reference parameter behavior?\n");
      set_a_IFoo(address_of(foop));

      printf("### Test 19: return value behavior?\n");
      foop = return_a_IFoo();
    }
    printf("### End Test 18, 19\n");

		{
      printf("\n### setup for Test 20\n");
			nsCOMPtr<IFoo> fooP;

			printf("### Test 20: is |QueryInterface| called on assigning in a raw pointer?\n");
			fooP = do_QueryInterface(new IFoo);
		}
    printf("### End Test 20\n");

		{
      printf("\n### setup for Test 21\n");
			nsCOMPtr<IFoo> fooP;
			fooP = do_QueryInterface(new IFoo);

			nsCOMPtr<IFoo> foo2P;

			printf("### Test 21: is |QueryInterface| _not_ called when assigning in a smart-pointer of the same type?\n");
			foo2P = fooP;
		}
    printf("### End Test 21\n");

		{
      printf("\n### setup for Test 22\n");
			nsCOMPtr<IBar> barP( do_QueryInterface(new IBar) );

			printf("### Test 22: is |QueryInterface| called when assigning in a smart-pointer of a different type?\n");

			nsCOMPtr<IFoo> fooP( do_QueryInterface(barP) );
			if ( fooP )
				printf("an IBar* is an IFoo*\n");
		}
    printf("### End Test 22\n");


		{
      printf("\n### setup for Test 23\n");
			nsCOMPtr<IFoo> fooP( do_QueryInterface(new IFoo) );

			printf("### Test 23: does |forget| avoid an AddRef/Release when assigning to another nsCOMPtr?\n");
      nsCOMPtr<IFoo> fooP2( fooP.forget() );
		}
    printf("### End Test 23\n");

		{
			nsCOMPtr<IFoo> fooP;

			AnIFooPtrPtrContext( getter_AddRefs(fooP) );
			AVoidPtrPtrContext( getter_AddRefs(fooP) );
		}


		{
			nsCOMPtr<nsISupports> supportsP;

			AVoidPtrPtrContext( getter_AddRefs(supportsP) );
			AnISupportsPtrPtrContext( getter_AddRefs(supportsP) );
		}


    printf("\n### Test 24: will a static |nsCOMPtr| |Release| before program termination?\n");
    gFoop = do_QueryInterface(new IFoo);
    
    printf("<<main()\n");
    return 0;
  }

