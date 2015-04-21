/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <assert.h>
#include <stdio.h>
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsISupports.h"
#include "nsQueryObject.h"

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

      NS_IMETHOD_(MozExternalRefCountType) AddRef();
      NS_IMETHOD_(MozExternalRefCountType) Release();
      NS_IMETHOD QueryInterface( const nsIID&, void** );

      static void print_totals();

    private:
      unsigned int refcount_;

      static unsigned int total_constructions_;
      static unsigned int total_destructions_;
  };

NS_DEFINE_STATIC_IID_ACCESSOR(Foo, NS_FOO_IID)

class Bar;

  // some types I'll need
typedef unsigned long NS_RESULT;

  // some functions I'll need (and define below)
          nsresult  CreateFoo( void** );
          nsresult  CreateBar( void** result );
              void  AnFooPtrPtrContext( Foo** );
              void	AnISupportsPtrPtrContext( nsISupports** );
              void  AVoidPtrPtrContext( void** );
              void  set_a_Foo( nsRefPtr<Foo>* result );
nsRefPtr<Foo>  return_a_Foo();




unsigned int Foo::total_constructions_;
unsigned int Foo::total_destructions_;

class test_message
  {
    public:
      test_message()
        {
          printf("BEGIN unit tests for |nsRefPtr|, compiled " __DATE__ "\n");
        }

     ~test_message()
        {
          Foo::print_totals();
          printf("END unit tests for |nsRefPtr|.\n");
        }
  };

test_message gTestMessage;


  /*
    ...
  */

void
Foo::print_totals()
  {
    printf("total constructions/destructions --> %d/%d\n", 
           total_constructions_, total_destructions_);
  }

Foo::Foo()
    : refcount_(0)
  {
    ++total_constructions_;
    printf("  new Foo@%p [#%d]\n",
           static_cast<void*>(this), total_constructions_);
  }

Foo::~Foo()
  {
    ++total_destructions_;
    printf("Foo@%p::~Foo() [#%d]\n",
           static_cast<void*>(this), total_destructions_);
  }

MozExternalRefCountType
Foo::AddRef()
  {
    ++refcount_;
    printf("Foo@%p::AddRef(), refcount --> %d\n",
           static_cast<void*>(this), refcount_);
    return refcount_;
  }

MozExternalRefCountType
Foo::Release()
  {
    int newcount = --refcount_;
    if ( newcount == 0 )
      printf(">>");

    printf("Foo@%p::Release(), refcount --> %d\n",
           static_cast<void*>(this), refcount_);

    if ( newcount == 0 )
      {
        printf("  delete Foo@%p\n", static_cast<void*>(this));
        printf("<<Foo@%p::Release()\n", static_cast<void*>(this));
        delete this;
      }

    return newcount;
  }

nsresult
Foo::QueryInterface( const nsIID& aIID, void** aResult )
	{
    printf("Foo@%p::QueryInterface()\n", static_cast<void*>(this));
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

nsresult
CreateFoo( void** result )
    // a typical factory function (that calls AddRef)
  {
    printf(">>CreateFoo() --> ");
    Foo* foop = new Foo;
    printf("Foo@%p\n", static_cast<void*>(foop));

    foop->AddRef();
    *result = foop;

    printf("<<CreateFoo()\n");
    return NS_OK;
  }

void
set_a_Foo( nsRefPtr<Foo>* result )
  {
    printf(">>set_a_Foo()\n");
    assert(result);

    nsRefPtr<Foo> foop( do_QueryObject(new Foo) );
    *result = foop;
    printf("<<set_a_Foo()\n");
  }

nsRefPtr<Foo>
return_a_Foo()
  {
    printf(">>return_a_Foo()\n");
    nsRefPtr<Foo> foop( do_QueryObject(new Foo) );
    printf("<<return_a_Foo()\n");
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
      virtual ~Bar();

      NS_IMETHOD QueryInterface( const nsIID&, void** );
  };

NS_DEFINE_STATIC_IID_ACCESSOR(Bar, NS_BAR_IID)

Bar::Bar()
  {
    printf("  new Bar@%p\n", static_cast<void*>(this));
  }

Bar::~Bar()
  {
    printf("Bar@%p::~Bar()\n", static_cast<void*>(this));
  }

nsresult
Bar::QueryInterface( const nsID& aIID, void** aResult )
	{
    printf("Bar@%p::QueryInterface()\n", static_cast<void*>(this));
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



nsresult
CreateBar( void** result )
    // a typical factory function (that calls AddRef)
  {
    printf(">>CreateBar() --> ");
    Bar* barp = new Bar;
    printf("Bar@%p\n", static_cast<void*>(barp));

    barp->AddRef();
    *result = barp;

    printf("<<CreateBar()\n");
    return NS_OK;
  }

void
AnFooPtrPtrContext( Foo** )
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

nsresult
TestBloat_Raw_Unsafe()
	{
		Bar* barP = 0;
		nsresult result = CreateBar(reinterpret_cast<void**>(&barP));

		if ( barP )
			{
				Foo* fooP = 0;
				if ( NS_SUCCEEDED( result = barP->QueryInterface(NS_GET_IID(Foo), reinterpret_cast<void**>(&fooP)) ) )
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
		nsRefPtr<Bar> barP;
		nsresult result = CreateBar( getter_AddRefs(barP) );

		nsRefPtr<Foo> fooP( do_QueryObject(barP, &result) );

		if ( fooP )
			fooP->print_totals();

		return result;
	}




nsRefPtr<Foo> gFoop;

int
main()
  {
    printf(">>main()\n");

		printf("sizeof(nsRefPtr<Foo>) --> %u\n", unsigned(sizeof(nsRefPtr<Foo>)));

		TestBloat_Raw_Unsafe();
		TestBloat_Smart();


    {
      printf("\n### Test  1: will a |nsCOMPtr| call |AddRef| on a pointer assigned into it?\n");
      nsRefPtr<Foo> foop( do_QueryObject(new Foo) );

      printf("\n### Test  2: will a |nsCOMPtr| |Release| its old pointer when a new one is assigned in?\n");
      foop = do_QueryObject(new Foo);

        // [Shouldn't compile] Is it a compile time error to try to |AddRef| by hand?
      //foop->AddRef();

        // [Shouldn't compile] Is it a compile time error to try to |Release| be hand?
      //foop->Release();

				// [Shouldn't compile] Is it a compile time error to try to |delete| an |nsCOMPtr|?
			//delete foop;

      printf("\n### Test  3: can you |AddRef| if you must?\n");
      static_cast<Foo*>(foop)->AddRef();

      printf("\n### Test  4: can you |Release| if you must?\n");
      static_cast<Foo*>(foop)->Release();

      printf("\n### Test  5: will a |nsCOMPtr| |Release| when it goes out of scope?\n");
    }

    {
      printf("\n### Test  6: will a |nsCOMPtr| call the correct destructor?\n");
      nsRefPtr<Foo> foop( do_QueryObject(new Bar) );
    }

    {
      printf("\n### Test  7: can you compare one |nsCOMPtr| with another [!=]?\n");

      nsRefPtr<Foo> foo1p( do_QueryObject(new Foo) );

        // [Shouldn't compile] Is it a compile time error to omit |getter_[doesnt_]AddRef[s]|?
      //AnFooPtrPtrContext(&foo1p);

        // [Shouldn't compile] Is it a compile time error to omit |getter_[doesnt_]AddRef[s]|?
      //AVoidPtrPtrContext(&foo1p);

      nsRefPtr<Foo> foo2p( do_QueryObject(new Foo) );

      if ( foo1p != foo2p )
        printf("foo1p != foo2p\n");
      else
        printf("foo1p == foo2p\n");

      printf("\n### Test  7.5: can you compare a |nsCOMPtr| with NULL, 0, nullptr [!=]?\n");
      if ( foo1p != 0 )
      	printf("foo1p != 0\n");
      if ( 0 != foo1p )
      	printf("0 != foo1p\n");
      if ( foo1p == 0 )
      	printf("foo1p == 0\n");
      if ( 0 == foo1p )
      	printf("0 == foo1p\n");
			

      Foo* raw_foo2p = foo2p.get();

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
      if ( nsRefPtr<Foo>( raw_foo2p ) == foo2p )
        printf("raw_foo2p == foo2p\n");
      else
        printf("raw_foo2p != foo2p\n");
#endif

      printf("\n### Test 12: bare pointer test?\n");
      if ( foo1p )
        printf("foo1p is not NULL\n");
      else
        printf("foo1p is NULL\n");

      printf("\n### Test 13: numeric pointer test?\n");
      if ( foo1p == 0 )
        printf("foo1p is NULL\n");
      else
        printf("foo1p is not NULL\n");

#if 0
			if ( foo1p == 1 )
				printf("foo1p allowed compare with in\n");
#endif

      printf("\n### Test 14: how about when two |nsCOMPtr|s referring to the same object go out of scope?\n");
    }

    {
      printf("\n### Test 15,16 ...setup...\n");
      Foo* raw_foo1p = new Foo;
      raw_foo1p->AddRef();

      Foo* raw_foo2p = new Foo;
      raw_foo2p->AddRef();

      printf("\n### Test 15: what if I don't want to |AddRef| when I construct?\n");
      nsRefPtr<Foo> foo1p( dont_AddRef(raw_foo1p) );
      //nsRefPtr<Foo> foo1p = dont_AddRef(raw_foo1p);

      printf("\n### Test 16: what if I don't want to |AddRef| when I assign in?\n");
      nsRefPtr<Foo> foo2p;
      foo2p = dont_AddRef(raw_foo2p);
    }







    {
    	printf("\n### setup for Test 17\n");
      nsRefPtr<Foo> foop;
      printf("### Test 17: basic parameter behavior?\n");
      CreateFoo( nsRefPtrGetterAddRefs<Foo>(foop) );
    }
    printf("### End Test 17\n");


    {
    	printf("\n### setup for Test 18\n");
      nsRefPtr<Foo> foop;
      printf("### Test 18: basic parameter behavior, using the short form?\n");
      CreateFoo( getter_AddRefs(foop) );
    }
    printf("### End Test 18\n");


    {
    	printf("\n### setup for Test 19, 20\n");
      nsRefPtr<Foo> foop;
      printf("### Test 19: reference parameter behavior?\n");
      set_a_Foo(address_of(foop));

      printf("### Test 20: return value behavior?\n");
      foop = return_a_Foo();
    }
    printf("### End Test 19, 20\n");

		{
    	printf("\n### setup for Test 21\n");
			nsRefPtr<Foo> fooP;

			printf("### Test 21: is |QueryInterface| called on assigning in a raw pointer?\n");
			fooP = do_QueryObject(new Foo);
		}
    printf("### End Test 21\n");

		{
    	printf("\n### setup for Test 22\n");
			nsRefPtr<Foo> fooP;
			fooP = do_QueryObject(new Foo);

			nsRefPtr<Foo> foo2P;

			printf("### Test 22: is |QueryInterface| _not_ called when assigning in a smart-pointer of the same type?\n");
			foo2P = fooP;
		}
    printf("### End Test 22\n");

		{
    	printf("\n### setup for Test 23\n");
			nsRefPtr<Bar> barP( do_QueryObject(new Bar) );

			printf("### Test 23: is |QueryInterface| called when assigning in a smart-pointer of a different type?\n");

			nsRefPtr<Foo> fooP( do_QueryObject(barP) );
			if ( fooP )
				printf("an Bar* is an Foo*\n");
		}
    printf("### End Test 23\n");


		{
    	printf("\n### setup for Test 24\n");
			nsRefPtr<Foo> fooP( do_QueryObject(new Foo) );

			printf("### Test 24: does |forget| avoid an AddRef/Release when assigning to another nsCOMPtr?\n");
      nsRefPtr<Foo> fooP2( fooP.forget() );
		}
    printf("### End Test 24\n");

		{
			nsRefPtr<Foo> fooP;

			AnFooPtrPtrContext( getter_AddRefs(fooP) );
			AVoidPtrPtrContext( getter_AddRefs(fooP) );
		}


    printf("\n### Test 25: will a static |nsCOMPtr| |Release| before program termination?\n");
    gFoop = do_QueryObject(new Foo);
    
    printf("<<main()\n");
    return 0;
  }

