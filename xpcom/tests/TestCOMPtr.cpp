/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
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

#include <iostream.h>
#include <assert.h>
#include "nsCOMPtr.h"
#include "nsISupports.h"

#ifdef HAVE_CPP_NEW_CASTS
  #define STATIC_CAST(T,x)  static_cast<T>(x)
  #define REINTERPRET_CAST(T,x) reinterpret_cast<T>(x)
#else
  #define STATIC_CAST(T,x)  ((T)(x))
  #define REINTERPRET_CAST(T,x) ((T)(x))
#endif


#define NS_IFOO_IID \
{ 0x6f7652e0,  0xee43, 0x11d1, \
 { 0x9c, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

class IFoo : public nsISupports
  {
		public:
			static const nsIID& GetIID() { static nsIID iid = NS_IFOO_IID; return iid; }

		public:
      IFoo();
      virtual ~IFoo();

      NS_IMETHOD_(nsrefcnt) AddRef();
      NS_IMETHOD_(nsrefcnt) Release();
      NS_IMETHOD QueryInterface( const nsIID&, void** );

      static void print_totals();

    private:
      unsigned int refcount_;

      static unsigned int total_constructions_;
      static unsigned int total_destructions_;
  };

class IBar;

  // some types I'll need
typedef unsigned long NS_RESULT;

  // some functions I'll need (and define below)
          nsresult  CreateIFoo( void** );
          nsresult  CreateIBar( void** result );
              void  AnIFooPtrPtrContext( IFoo** );
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
          cout << "BEGIN unit tests for |nsCOMPtr|, compiled " __DATE__ << endl;
        }

     ~test_message()
        {
          IFoo::print_totals();
          cout << "END unit tests for |nsCOMPtr|." << endl;
        }
  };

test_message gTestMessage;


  /*
    ...
  */

void
IFoo::print_totals()
  {
    cout << "total constructions/destructions --> " << total_constructions_ << "/" << total_destructions_ << endl;
  }

IFoo::IFoo()
    : refcount_(0)
  {
    ++total_constructions_;
    cout << "  new IFoo@" << STATIC_CAST(void*, this) << " [#" << total_constructions_ << "]" << endl;
  }

IFoo::~IFoo()
  {
    ++total_destructions_;
    cout << "IFoo@" << STATIC_CAST(void*, this) << "::~IFoo()" << " [#" << total_destructions_ << "]" << endl;
  }

nsrefcnt
IFoo::AddRef()
  {
    ++refcount_;
    cout << "IFoo@" << STATIC_CAST(void*, this) << "::AddRef(), refcount --> " << refcount_ << endl;
    return refcount_;
  }

nsrefcnt
IFoo::Release()
  {
    int wrap_message = (refcount_ == 1);
    if ( wrap_message )
      cout << ">>";
      
    --refcount_;
    cout << "IFoo@" << STATIC_CAST(void*, this) << "::Release(), refcount --> " << refcount_ << endl;

    if ( !refcount_ )
      {
        cout << "  delete IFoo@" << STATIC_CAST(void*, this) << endl;
        delete this;
      }

    if ( wrap_message )
      cout << "<<IFoo@" << STATIC_CAST(void*, this) << "::Release()" << endl;

    return refcount_;
  }

nsresult
IFoo::QueryInterface( const nsIID& aIID, void** aResult )
	{
    cout << "IFoo@" << STATIC_CAST(void*, this) << "::QueryInterface()" << endl;
		nsISupports* rawPtr = 0;
		nsresult status = NS_OK;

		if ( aIID.Equals(GetIID()) )
			rawPtr = this;
		else
			{
				nsID iid_of_ISupports = NS_ISUPPORTS_IID;
				if ( aIID.Equals(iid_of_ISupports) )
					rawPtr = STATIC_CAST(nsISupports*, this);
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
    cout << ">>CreateIFoo() --> ";
    IFoo* foop = new IFoo;
    cout << "IFoo@" << STATIC_CAST(void*, foop) << endl;

    foop->AddRef();
    *result = foop;

    cout << "<<CreateIFoo()" << endl;
    return 0;
  }

void
set_a_IFoo( nsCOMPtr<IFoo>* result )
  {
    cout << ">>set_a_IFoo()" << endl;
    assert(result);

    nsCOMPtr<IFoo> foop( do_QueryInterface(new IFoo) );
    *result = foop;
    cout << "<<set_a_IFoo()" << endl;
  }

nsCOMPtr<IFoo>
return_a_IFoo()
  {
    cout << ">>return_a_IFoo()" << endl;
    nsCOMPtr<IFoo> foop( do_QueryInterface(new IFoo) );
    cout << "<<return_a_IFoo()" << endl;
    return foop;
  }




#define NS_IBAR_IID \
{ 0x6f7652e1,  0xee43, 0x11d1, \
 { 0x9c, 0xc3, 0x00, 0x60, 0x08, 0x8c, 0xa6, 0xb3 } }

class IBar : public IFoo
  {
  	public:
  		static const nsIID& GetIID() { static nsIID iid = NS_IBAR_IID; return iid; }

    public:
      IBar();
      virtual ~IBar();

      NS_IMETHOD QueryInterface( const nsIID&, void** );
  };

IBar::IBar()
  {
    cout << "  new IBar@" << STATIC_CAST(void*, this) << endl;
  }

IBar::~IBar()
  {
    cout << "IBar@" << STATIC_CAST(void*, this) << "::~IBar()" << endl;
  }

nsresult
IBar::QueryInterface( const nsID& aIID, void** aResult )
	{
    cout << "IBar@" << STATIC_CAST(void*, this) << "::QueryInterface()" << endl;
		nsISupports* rawPtr = 0;
		nsresult status = NS_OK;

		if ( aIID.Equals(GetIID()) )
			rawPtr = this;
		else if ( aIID.Equals(IFoo::GetIID()) )
			rawPtr = STATIC_CAST(IFoo*, this);
		else
			{
				nsID iid_of_ISupports = NS_ISUPPORTS_IID;
				if ( aIID.Equals(iid_of_ISupports) )
					rawPtr = STATIC_CAST(nsISupports*, this);
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
    cout << ">>CreateIBar() --> ";
    IBar* barp = new IBar;
    cout << "IBar@" << STATIC_CAST(void*, barp) << endl;

    barp->AddRef();
    *result = barp;

    cout << "<<CreateIBar()" << endl;
    return 0;
  }

void
AnIFooPtrPtrContext( IFoo** )
  {
  }

void
AVoidPtrPtrContext( void** )
  {
  }


// Optimism
#define TEST_EXCEPTIONS 1

// HAVE_CPP_EXCEPTIONS is defined automagically on unix
#if defined(XP_UNIX)
#if !defined(HAVE_CPP_EXCEPTIONS)
#undef TEST_EXCEPTIONS
#endif
#endif

#ifdef TEST_EXCEPTIONS
static
nsresult
TestBloat_Raw()
	{
		IBar* barP = 0;
		nsresult result = CreateIBar(REINTERPRET_CAST(void**, &barP));

		if ( barP )
			{
				try
					{
						IFoo* fooP = 0;
						if ( NS_SUCCEEDED( result = barP->QueryInterface(IFoo::GetIID(), REINTERPRET_CAST(void**, &fooP)) ) )
							{
								try
									{
										fooP->print_totals();
									}
								catch( ... )
									{
										NS_RELEASE(fooP);
										throw;
									}

								NS_RELEASE(fooP);
							}
					}
				catch( ... )
					{
						NS_RELEASE(barP);
						throw;
					}

				NS_RELEASE(barP);
			}

		return result;
	}
#endif // TEST_EXCEPTIONS

static
nsresult
TestBloat_Raw_Unsafe()
	{
		IBar* barP = 0;
		nsresult result = CreateIBar(REINTERPRET_CAST(void**, &barP));

		if ( barP )
			{
				IFoo* fooP = 0;
				if ( NS_SUCCEEDED( result = barP->QueryInterface(IFoo::GetIID(), REINTERPRET_CAST(void**, &fooP)) ) )
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
    cout << ">>main()" << endl;

		cout << "sizeof(nsCOMPtr<IFoo>) --> " << sizeof(nsCOMPtr<IFoo>) << endl;

#ifdef TEST_EXCEPTIONS
		TestBloat_Raw();
#endif // TEST_EXCEPTIONS
		TestBloat_Raw_Unsafe();
		TestBloat_Smart();


    {
      cout << endl << "### Test  1: will a |nsCOMPtr| call |AddRef| on a pointer assigned into it?" << endl;
      nsCOMPtr<IFoo> foop( do_QueryInterface(new IFoo) );

      cout << endl << "### Test  2: will a |nsCOMPtr| |Release| its old pointer when a new one is assigned in?" << endl;
      foop = do_QueryInterface(new IFoo);

        // [Shouldn't compile] Is it a compile time error to try to |AddRef| by hand?
      //foop->AddRef();

        // [Shouldn't compile] Is it a compile time error to try to |Release| be hand?
      //foop->Release();

	  /* Solaris Workshop compiler fails to compile this. */
#if !defined(XP_UNIX) || !(defined(SOLARIS) && !defined(__GNUG__))
      cout << endl << "### Test  3: can you |AddRef| if you must?" << endl;
      STATIC_CAST(IFoo*, foop)->AddRef();

      cout << endl << "### Test  4: can you |Release| if you must?" << endl;
      STATIC_CAST(IFoo*, foop)->Release();
#endif

      cout << endl << "### Test  5: will a |nsCOMPtr| |Release| when it goes out of scope?" << endl;
    }

    {
      cout << endl << "### Test  6: will a |nsCOMPtr| call the correct destructor?" << endl;
      nsCOMPtr<IFoo> foop( do_QueryInterface(new IBar) );
    }

    {
      cout << endl << "### Test  7: can you compare one |nsCOMPtr| with another [!=]?" << endl;

      nsCOMPtr<IFoo> foo1p( do_QueryInterface(new IFoo) );

        // [Shouldn't compile] Is it a compile time error to omit |getter_[doesnt_]AddRef[s]|?
      //AnIFooPtrPtrContext(&foo1p);

        // [Shouldn't compile] Is it a compile time error to omit |getter_[doesnt_]AddRef[s]|?
      //AVoidPtrPtrContext(&foo1p);

      nsCOMPtr<IFoo> foo2p( do_QueryInterface(new IFoo) );

      if ( foo1p != foo2p )
        cout << "foo1p != foo2p" << endl;
      else
        cout << "foo1p == foo2p" << endl;

      IFoo* raw_foo2p = foo2p.get();

      cout << endl << "### Test  8: can you compare a |nsCOMPtr| with a raw interface pointer [!=]?" << endl;
      if ( foo1p.get() != raw_foo2p )
        cout << "foo1p != raw_foo2p" << endl;
      else
        cout << "foo1p == raw_foo2p" << endl;


      cout << endl << "### Test  9: can you assign one |nsCOMPtr| into another?" << endl;
      foo1p = foo2p;

      cout << endl << "### Test 10: can you compare one |nsCOMPtr| with another [==]?" << endl;
      if ( foo1p == foo2p )
        cout << "foo1p == foo2p" << endl;
      else
        cout << "foo1p != foo2p" << endl;

      cout << endl << "### Test 11: can you compare a |nsCOMPtr| with a raw interface pointer [==]?" << endl;
      if ( raw_foo2p == foo2p.get() )
        cout << "raw_foo2p == foo2p" << endl;
      else
        cout << "raw_foo2p != foo2p" << endl;

#if 1
      cout << endl << "### Test 11.5: can you compare a |nsCOMPtr| with a raw interface pointer [==]?" << endl;
      if ( nsCOMPtr<IFoo>( dont_QueryInterface(raw_foo2p) ) == foo2p )
        cout << "raw_foo2p == foo2p" << endl;
      else
        cout << "raw_foo2p != foo2p" << endl;
#endif

      cout << endl << "### Test 12: bare pointer test?" << endl;
      if ( foo1p )
        cout << "foo1p is not NULL" << endl;
      else
        cout << "foo1p is NULL" << endl;

      cout << endl << "### Test 13: numeric pointer test?" << endl;
      if ( foo1p == 0 )
        cout << "foo1p is NULL" << endl;
      else
        cout << "foo1p is not NULL" << endl;

      cout << endl << "### Test 14: how about when two |nsCOMPtr|s refering to the same object go out of scope?" << endl;
    }

    {
      cout << endl << "### Test 15,16 ...setup..." << endl;
      IFoo* raw_foo1p = new IFoo;
      raw_foo1p->AddRef();

      IFoo* raw_foo2p = new IFoo;
      raw_foo2p->AddRef();

      cout << endl << "### Test 15: what if I don't want to |AddRef| when I construct?" << endl;
      nsCOMPtr<IFoo> foo1p( dont_AddRef(raw_foo1p) );
      //nsCOMPtr<IFoo> foo1p = dont_AddRef(raw_foo1p);

      cout << endl << "### Test 16: what if I don't want to |AddRef| when I assign in?" << endl;
      nsCOMPtr<IFoo> foo2p;
      foo2p = dont_AddRef(raw_foo2p);
    }







    {
    	cout << endl << "### setup for Test 17" << endl;
      nsCOMPtr<IFoo> foop;
      cout << "### Test 17: basic parameter behavior?" << endl;
      CreateIFoo( nsGetterAddRefs<IFoo>(foop) );
    }
    cout << "### End Test 17" << endl;


    {
    	cout << endl << "### setup for Test 18" << endl;
      nsCOMPtr<IFoo> foop;
      cout << "### Test 18: basic parameter behavior, using the short form?" << endl;
      CreateIFoo( getter_AddRefs(foop) );
    }
    cout << "### End Test 18" << endl;


    {
    	cout << endl << "### setup for Test 19, 20" << endl;
      nsCOMPtr<IFoo> foop;
      cout << "### Test 19: reference parameter behavior?" << endl;
      set_a_IFoo(&foop);

      cout << "### Test 20: return value behavior?" << endl;
      foop = return_a_IFoo();
    }
    cout << "### End Test 19, 20" << endl;

		{
    	cout << endl << "### setup for Test 21" << endl;
			nsCOMPtr<IFoo> fooP;

			cout << "### Test 21: is |QueryInterface| called on assigning in a raw pointer?" << endl;
			fooP = do_QueryInterface(new IFoo);
		}
    cout << "### End Test 21" << endl;

		{
    	cout << endl << "### setup for Test 22" << endl;
			nsCOMPtr<IFoo> fooP;
			fooP = do_QueryInterface(new IFoo);

			nsCOMPtr<IFoo> foo2P;

			cout << "### Test 22: is |QueryInterface| _not_ called when assigning in a smart-pointer of the same type?" << endl;
			foo2P = fooP;
		}
    cout << "### End Test 22" << endl;

		{
    	cout << endl << "### setup for Test 23" << endl;
			nsCOMPtr<IBar> barP( do_QueryInterface(new IBar) );

			cout << "### Test 23: is |QueryInterface| called when assigning in a smart-pointer of a different type?" << endl;

			nsCOMPtr<IFoo> fooP( do_QueryInterface(barP) );
			if ( fooP )
				cout << "an IBar* is an IFoo*" << endl;
		}
    cout << "### End Test 23" << endl;


		{
    	cout << endl << "### setup for Test 24" << endl;
			nsCOMPtr<IFoo> fooP;
			IFoo* rawFooP = new IFoo;

			cout << "### Test 24: is |QueryInterface| _not_ called when explicitly barred?" << endl;
			fooP = dont_QueryInterface(rawFooP);
			cout << "### cleanup for Test 24" << endl;
		}
    cout << "### End Test 24" << endl;


    cout << endl << "### Test 25: will a static |nsCOMPtr| |Release| before program termination?" << endl;
    gFoop = do_QueryInterface(new IFoo);
    
    cout << "<<main()" << endl;
    return 0;
  }

