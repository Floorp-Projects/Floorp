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

#ifndef COM_auto_ptr_h___
#define COM_auto_ptr_h___

	/*
		WARNING:
			The code in this file should be considered EXPERIMENTAL.  It defies several of our
			current coding conventions; in particular, it is based on templates.  It is not to
			be used in production code under any circumstances, until such time as our current
			coding-conventions barring templates can be relaxed.  At that time, this warning
			will be removed.

			It is checked-in only so that concerned parties can experiment, to see if it fills
			a useful (and affordable) role.

			NOT FOR USE IN PRODUCTION CODE!
	*/

#if defined(NOT_PRODUCTION_CODE) && defined(USE_EXPERIMENTAL_SMART_POINTERS)

#include <cassert>


	/* USER MANUAL

		What is |COM_auto_ptr|?

			|COM_auto_ptr| is a `smart-pointer'.  It is a template class that manages
			|AddRef| and |Release| so that you don't have to.  It helps you write code that is
			leak-proof, exception safe, and significantly less verbose than you would with
			straight COM interface pointers.  With |COM_auto_ptr|, you may never have to
			call |AddRef| or |Release| by hand.

			You still have to understand COM.  You still have to know which functions
			return interface pointers that have already been |AddRef|ed and which don't.
			You still have to ensure you program logic doesn't produce circularly referencing
			garbage.  |COM_auto_ptr| is not a panacea.  It is, however, helpful, easy to use,
			well-tested, and polite.  It doesn't require that a function author cooperate with
			you, nor does your use force others to use it.


		Why does |COM_auto_ptr| have such a funny name?  I.e., why doesn't it follow our
		naming conventions?

			The name of this class is very important.  It is designed to communicate the purpose
			of the class easily to any programmer new to the project, who is already familiar with
			|std::auto_ptr| and who knows that COM requires ref-counting.  Relating this class'
			name to |auto_ptr| is more important to clarity than following the local naming
			convention.


		How do I use |COM_auto_ptr|?

			Typically, you can use a |COM_auto_ptr| exactly as you would a standard COM
			interface pointer:

				IFoo* foop;														COM_auto_ptr<IFoo> foop;
				// ...																// ...
				foop->SomeFunction(x, y, z);					foop->SomeFunction(x, y, z);
				AnotherFunction(foop);								AnotherFunction(foop);

				if ( foop )														if ( foop )
					// ...																// ...

				if ( foop == barp )										if ( foop == barp )
					// ...																// ...

			There are some differences, though.  In particular, you can't call |AddRef| or |Release|
			through a |COM_auto_ptr| directly, nor would you need to.  |AddRef| is called for you
			whenever you assign a COM interface pointer _into_ a |COM_auto_ptr|.  |Release| is
			called on the old value, and also when the |COM_auto_ptr| goes out of scope.  Trying
			to call |AddRef| or |Release| yourself will generate a compile-time error.

				foop->AddRef();												// foop->AddRef();	// ERROR: no permission
				foop->Release();											// foop->Release();	// ERROR: no permission

			The final difference is that a bare |COM_auto_ptr| (or rather a pointer to it) can't
			be supplied as an argument to a function that `fills in' a COM interface pointer.
			Rather it must be wrapped with a utility call that says whether the function calls
			|AddRef| before returning, e.g.,

				...->QueryInterface(riid, &foop)			...->QueryInterface(riid, func_AddRefs(foop))

				LookupFoo(&foop);											LookupFoo( func_doesnt_AddRef(foop) );

			Don't worry.  It's a compile-time error if you forget to wrap it.


				IFoo* foo = 0;
				NS_RESULT status = CreateIFoo(&foo);
				if ( NS_SUCCESS(status) )
					{
						IBar* bar = 0;
						if ( NS_SUCCESS(status = foo->QueryInterface(riid, &bar)) )
							{
								IFooBar* foobar = 0;
								if ( NS_SUCCESS(status = CreateIFooBar(foo, bar, &foobar)) )
									{
										foobar->DoTheReallyHardThing();
										foobar->Release();
									}
								bar->Release();
							}
						foo->Release();
					}


			COM_auto_ptr<IFoo> foop;
			NS_RESULT status = CreateIFoo( func_AddRefs(foop) );
			if ( NS_SUCCESS(status) )
				{
					COM_auto_ptr<IBar> barp;
					if ( NS_SUCCESS(status = foo->QueryInterface(riid, func_AddRefs(barp))) )
						{
							COM_auto_ptr<IFooBar> foobarp;
							if ( NS_SUCCESS(status = CreateIFooBar(foop, barp, func_AddRefs(foobarp))) )
								foobarp->DoTheReallyHardThing();
						}
				}

			


		Where should I use |COM_auto_ptr|?


		Is there an easy way to convert my current code?

			
			

		Where _shouldn't_ I use |COM_auto_ptr|?

		What do I have to beware of?
	*/



	/*
		Set up some #defines to turn off a couple of troublesome C++ features.
		Interestingly, none of the compilers barf on template stuff.
	*/

#if defined(__GNUG__) && (__GNUC_MINOR__ <= 90) && !defined(SOLARIS)
	#define NO_MEMBER_USING_DECLARATIONS
#endif

#if defined(_MSC_VER) && (_MSC_VER<1100)
	#define NO_EXPLICIT
#endif

#ifdef NO_EXPLICIT
	#define explicit
#endif


template <class T>
class derived_safe : public T
		/*
			No client should ever see or have to type the name of this class.  It is the
			artifact that makes it a compile-time error to call |AddRef| and |Release|
			on a |COM_auto_ptr|.

			See |COM_auto_ptr::operator->|, |COM_auto_ptr::operator*|, et al.
		*/
	{
		private:
#ifndef NO_MEMBER_USING_DECLARATIONS
			using T::AddRef;
			using T::Release;
#else
			unsigned long AddRef() { }
			unsigned long Release() { }
#endif
	};

#if defined(NO_MEMBER_USING_DECLARATIONS) && defined(NEED_UNUSED_VIRTUAL_IMPLEMENTATIONS)
template <class T>
unsigned long
derived_safe<T>::AddRef()
	{
		return 0;
	}

template <class T>
unsigned long
derived_safe<T>::Release()
	{
		return 0;
	}

#endif





template <class T>
struct dont_AddRef_t
		/*
			...cooperates with |COM_auto_ptr| to allow you to assign in a pointer _without_
			|AddRef|ing it.  You would rarely use this directly, but rather through the
			machinery of |func_AddRefs| in the argument list to functions that |AddRef|
			their results before returning them to the caller.

			See also |func_AddRefs()| and |class func_AddRefs_t|.
		*/
	{
		explicit
		dont_AddRef_t( T* ptr )
				: ptr_(ptr)
			{
				// nothing else to do here
			}

		T* ptr_;
	};

template <class T>
dont_AddRef_t<T>
dont_AddRef( T* ptr )
		/*
			...makes typing easier, because it deduces the template type, e.g., 
			you write |dont_AddRef(foop)| instead of |dont_AddRef_t<IFoo>(foop)|.

			Like the class it is shorthand for, you would rarely use this directly,
			but rather through |func_AddRefs|.
		*/
	{
		return dont_AddRef_t<T>(ptr);
	}





template <class T>
class COM_auto_ptr
		/*
			...

			There is no point in using the |NS_ADDREF|, et al, macros here, since they can't
			be made to print out useful file and line information.  I could be convinced otherwise.
		*/
	{
		public:
			typedef T element_type;

			explicit
			COM_auto_ptr( T* ptr = 0 )
					: ptr_(ptr)
				{
					if ( ptr_ )
						ptr_->AddRef();
				}

			explicit
			COM_auto_ptr( const dont_AddRef_t<T>& P )
					: ptr_(P.ptr_)
				{
					// nothing else to do here
				}

			COM_auto_ptr( const COM_auto_ptr<T>& P )
					: ptr_(P.ptr_)
				{
					if ( ptr_ )
						ptr_->AddRef();
				}

		 ~COM_auto_ptr()
				{
					if ( ptr_ )
						ptr_->Release();
				}

			COM_auto_ptr&
			operator=( T* rhs )
				{
					reset(rhs);
					return *this;
				}

			COM_auto_ptr&
			operator=( const dont_AddRef_t<T>& rhs )
				{
					if ( ptr_ )
						ptr_->Release();
					ptr_ = rhs.ptr_;
					return *this;
				}

			COM_auto_ptr&
			operator=( const COM_auto_ptr& rhs )
				{
					reset(rhs.ptr_);
					return *this;
				}

			derived_safe<T>*
			operator->() const
					// returns a |derived_safe<T>*| to deny clients the use of |AddRef| and |Release|
				{
					return get();
				}

			derived_safe<T>&
			operator*() const
					// returns a |derived_safe<T>*| to deny clients the use of |AddRef| and |Release|
				{
					assert( ptr_ != 0 ); // you're not allowed to dereference a NULL pointer
					return *get();
				}

			operator derived_safe<T>*() const
					// Is this really a good idea?  Why, again, doesn't |std::auto_ptr| do this?
				{
					return get();
				}

			derived_safe<T>*
			get() const
					// returns a |derived_safe<T>*| to deny clients the use of |AddRef| and |Release|
				{
					return reinterpret_cast<derived_safe<T>*>(ptr_);
				}

			void
			reset( T* ptr = 0 )
				{
					if ( ptr != ptr_ )
						{
							if ( ptr )
								ptr->AddRef();
							if ( ptr_ )
								ptr_->Release();
							ptr_ = ptr;
						}
				}

		private:
			T* ptr_;
	};




template <class T>
class func_AddRefs_t
		/*
			...

			This class is designed to be used for anonymous temporary objects in the
			argument list of calls that return COM interface pointers, e.g.,

				COM_auto_ptr<IFoo> foop;
				...->QueryInterface(iid, func_AddRefs_t<IFoo>(foop))
				...->QueryInterface(iid, func_AddRefs(foop))

			When initialized with a |COM_auto_ptr|, as in the example above, it returns
			a |void**| (or |T**| if needed) that the outer call (|QueryInterface| in this
			case) can fill in.  When this temporary object goes out of scope, just after
			the call returns, its destructor assigned the resulting interface pointer, i.e.,
			|QueryInterface|s result, into the |COM_auto_ptr| it was initialized with.

			See also |func_doesnt_AddRef_t|.
		*/
	{
		public:
			explicit
			func_AddRefs_t( COM_auto_ptr<T>& P )
					: ptr_(0),
						new_owner_(P)
				{
					// nothing else to do
				}

		 ~func_AddRefs_t()
				{
					new_owner_ = dont_AddRef(ptr_);
				}

			operator void**()
				{
					return reinterpret_cast<void**>(&ptr_);
				}

			operator T**()
				{
					return &ptr_;
				}

		private:
			T* ptr_;
			COM_auto_ptr<T>& new_owner_;
	};

template <class T>
inline
func_AddRefs_t<T>
func_AddRefs( COM_auto_ptr<T>& P )
	{
		return func_AddRefs_t<T>(P);
	}





template <class T>
class func_doesnt_AddRef_t
		/*
			...
		*/
	{
		public:
			explicit
			func_doesnt_AddRef_t( COM_auto_ptr<T>& P )
					: ptr_(0),
						new_owner_(P)
				{
					// nothing else to do
				}

		 ~func_doesnt_AddRef_t()
				{
					new_owner_ = ptr_;
				}

			operator void**()
				{
					return reinterpret_cast<void**>(&ptr_);
				}

			operator T**()
				{
					return &ptr_;
				}

		private:
			T* ptr_;
			COM_auto_ptr<T>& new_owner_;
	};

template <class T>
inline
func_doesnt_AddRef_t<T>
func_doesnt_AddRef( COM_auto_ptr<T>& P )
	{
		return func_doesnt_AddRef_t<T>(P);
	}

#endif // defined(NOT_PRODUCTION_CODE) && defined(USE_EXPERIMENTAL_SMART_POINTERS)

#endif // !defined(COM_auto_ptr_h___)
