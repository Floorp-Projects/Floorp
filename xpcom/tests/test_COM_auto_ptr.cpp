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

#define NOT_PRODUCTION_CODE
#define USE_EXPERIMENTAL_SMART_POINTERS

#include <iostream.h>
#include "COM_auto_ptr.h"



class IFoo // : public ISupports
	{
		public:
			IFoo();
			virtual ~IFoo();

			virtual unsigned long AddRef();
			virtual unsigned long Release();
			// virtual NS_RESULT QueryInterface

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
				 NS_RESULT	CreateIFoo( void** );
				 NS_RESULT	CreateIBar( void** result );
							void	AnIFooPtrPtrContext( IFoo** );
							void	AVoidPtrPtrContext( void** );
							void	set_a_IFoo( COM_auto_ptr<IFoo>* result );
COM_auto_ptr<IFoo>	return_a_IFoo();




unsigned int IFoo::total_constructions_;
unsigned int IFoo::total_destructions_;

class test_message
	{
		public:
			test_message()
				{
					cout << "BEGIN unit tests for |COM_auto_ptr|, compiled " __DATE__ << endl;
				}

		 ~test_message()
				{
					IFoo::print_totals();
					cout << "END unit tests for |COM_auto_ptr|." << endl;
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

unsigned long
IFoo::AddRef()
	{
		++refcount_;
		cout << "IFoo@" << STATIC_CAST(void*, this) << "::AddRef(), refcount --> " << refcount_ << endl;
		return refcount_;
	}

unsigned long
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

NS_RESULT
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

NS_RESULT
CreateIFooVoidPtrRef( void*& result )
		// a typical factory function (that calls AddRef)
	{
		cout << ">>CreateIFooVoidPtrRef() --> ";
		IFoo* foop = new IFoo;
		cout << "IFoo@" << STATIC_CAST(void*, foop) << endl;

		foop->AddRef();
		result = foop;

		cout << "<<CreateIFooVoidPtrRef()" << endl;
		return 0;
	}

void
set_a_IFoo( COM_auto_ptr<IFoo>* result )
	{
		cout << ">>set_a_IFoo()" << endl;
		assert(result);

		COM_auto_ptr<IFoo> foop( new IFoo );
		*result = foop;
		cout << "<<set_a_IFoo()" << endl;
	}

COM_auto_ptr<IFoo>
return_a_IFoo()
	{
		cout << ">>return_a_IFoo()" << endl;
		COM_auto_ptr<IFoo> foop( new IFoo );
		cout << "<<return_a_IFoo()" << endl;
		return foop;
	}




class IBar : public IFoo
	{
		public:
			IBar();
			virtual ~IBar();
	};

IBar::IBar()
	{
		cout << "  new IBar@" << STATIC_CAST(void*, this) << endl;
	}

IBar::~IBar()
	{
		cout << "IBar@" << STATIC_CAST(void*, this) << "::~IBar()" << endl;
	}



NS_RESULT
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



COM_auto_ptr<IFoo> gFoop;

int
main()
	{
		cout << ">>main()" << endl;


		{
			cout << endl << "### Test  1: will a |COM_auto_ptr| call |AddRef| on a pointer assigned into it?" << endl;
			COM_auto_ptr<IFoo> foop( new IFoo );

			cout << endl << "### Test  2: will a |COM_auto_ptr| |Release| its old pointer when a new one is assigned in?" << endl;
			foop = new IFoo;

				// [Shouldn't compile] Is it a compile time error to try to |AddRef| by hand?
			//foop->AddRef();

				// [Shouldn't compile] Is it a compile time error to try to |Release| be hand?
			//foop->Release();

			cout << endl << "### Test  3: can you |AddRef| if you must?" << endl;
			STATIC_CAST(IFoo*, foop)->AddRef();

			cout << endl << "### Test  4: can you |Release| if you must?" << endl;
			STATIC_CAST(IFoo*, foop)->Release();

			cout << endl << "### Test  5: will a |COM_auto_ptr| |Release| when it goes out of scope?" << endl;
		}

		{
			cout << endl << "### Test  6: will a |COM_auto_ptr| call the correct destructor?" << endl;
			COM_auto_ptr<IFoo> foop( new IBar );
		}

		{
			cout << endl << "### Test  7: can you compare one |COM_auto_ptr| with another [!=]?" << endl;

			COM_auto_ptr<IFoo> foo1p( new IFoo );

				// [Shouldn't compile] Is it a compile time error to omit |func_[doesnt_]AddRef[s]|?
			//AnIFooPtrPtrContext(&foo1p);

				// [Shouldn't compile] Is it a compile time error to omit |func_[doesnt_]AddRef[s]|?
			//AVoidPtrPtrContext(&foo1p);

			COM_auto_ptr<IFoo> foo2p( new IFoo );

			if ( foo1p != foo2p )
				cout << "foo1p != foo2p" << endl;
			else
				cout << "foo1p == foo2p" << endl;

			IFoo* raw_foo2p = foo2p.get();

			cout << endl << "### Test  8: can you compare a |COM_auto_ptr| with a raw interface pointer [!=]?" << endl;
			if ( foo1p.get() != raw_foo2p )
				cout << "foo1p.get() != raw_foo2p" << endl;
			else
				cout << "foo1p.get() == raw_foo2p" << endl;


			cout << endl << "### Test  9: can you assign one |COM_auto_ptr| into another?" << endl;
			foo1p = foo2p;

			cout << endl << "### Test 10: can you compare one |COM_auto_ptr| with another [==]?" << endl;
			if ( foo1p == foo2p )
				cout << "foo1p == foo2p" << endl;
			else
				cout << "foo1p != foo2p" << endl;

			cout << endl << "### Test 11: can you compare a |COM_auto_ptr| with a raw interface pointer [==]?" << endl;
			if ( raw_foo2p == foo2p.get() )
				cout << "raw_foo2p == foo2p.get()" << endl;
			else
				cout << "raw_foo2p != foo2p.get()" << endl;

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

			cout << endl << "### Test 14: how about when two |COM_auto_ptr|s refering to the same object go out of scope?" << endl;
		}

		{
			cout << endl << "### Test 15,16 ...setup..." << endl;
			IFoo* raw_foo1p = new IFoo;
			raw_foo1p->AddRef();

			IFoo* raw_foo2p = new IFoo;
			raw_foo2p->AddRef();

			cout << endl << "### Test 15: what if I don't want to |AddRef| when I construct?" << endl;
			COM_auto_ptr<IFoo> foo1p( dont_AddRef(raw_foo1p) );
			//COM_auto_ptr<IFoo> foo1p = dont_AddRef(raw_foo1p);

			cout << endl << "### Test 16: what if I don't want to |AddRef| when I assign in?" << endl;
			COM_auto_ptr<IFoo> foo2p;
			foo2p = dont_AddRef(raw_foo2p);
		}







		{
			cout << endl << "### Test 17: basic parameter behavior?" << endl;
			COM_auto_ptr<IFoo> foop;
			CreateIFoo( (void **)(IFoo **)func_AddRefs_t<IFoo>(foop) );
		}


		{
			cout << endl << "### Test 18: basic parameter behavior, using the short form?" << endl;
			COM_auto_ptr<IFoo> foop;
			CreateIFoo( (void **)(IFoo **)func_AddRefs(foop) );
		}

		{
			cout << endl << "### Test 18: basic parameter behavior, using the short form?" << endl;
			COM_auto_ptr<IFoo> foop;
			CreateIFooVoidPtrRef( *(void **)func_AddRefs(foop) );
		}

		{
			cout << endl << "### Test 19: reference parameter behavior?" << endl;
			COM_auto_ptr<IFoo> foop;
			set_a_IFoo(&foop);

			cout << endl << "### Test 20: return value behavior?" << endl;
			foop = return_a_IFoo();
		}


		cout << endl << "### Test 21: will a static |COM_auto_ptr| |Release| before program termination?" << endl;
		gFoop = new IFoo;
		
		cout << "<<main()" << endl;
		return 0;
	}

