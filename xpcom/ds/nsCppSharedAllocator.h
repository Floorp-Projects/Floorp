/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsCppSharedAllocator_h__
#define nsCppSharedAllocator_h__

#include "nsMemory.h"     // for |nsMemory|
#include <new>					// to allow placement |new|


  // under MSVC shut off copious warnings about unused in-lines
#ifdef _MSC_VER
  #pragma warning( disable: 4514 )
#endif

#include <limits.h>


template <class T>
class nsCppSharedAllocator
    /*
      ...allows Standard Library containers, et al, to use our global shared
      (XP)COM-aware allocator.
    */
  {
    public:
      typedef T          value_type;
      typedef size_t     size_type;
      typedef ptrdiff_t  difference_type;

      typedef T*         pointer;
      typedef const T*   const_pointer;

      typedef T&         reference;
      typedef const T&   const_reference;



      nsCppSharedAllocator() { }

     ~nsCppSharedAllocator() { }


      pointer
      address( reference r ) const
        {
          return &r;
        }

      const_pointer
      address( const_reference r ) const
        {
          return &r;
        }

      pointer
      allocate( size_type n, const void* /*hint*/=0 )
        {
          return reinterpret_cast<pointer>(nsMemory::Alloc(static_cast<uint32_t>(n*sizeof(T))));
        }

      void
      deallocate( pointer p, size_type /*n*/ )
        {
          nsMemory::Free(p);
        }

      void
      construct( pointer p, const T& val )
        {
          new (p) T(val);
        }
      
      void
      destroy( pointer p )
        {
          p->~T();
        }

      size_type
      max_size() const
        {
          return ULONG_MAX / sizeof(T);
        }

  };


template <class T>
bool
operator==( const nsCppSharedAllocator<T>&, const nsCppSharedAllocator<T>& )
  {
    return true;
  }

template <class T>
bool
operator!=( const nsCppSharedAllocator<T>&, const nsCppSharedAllocator<T>& )
  {
    return false;
  }

#endif /* !defined(nsCppSharedAllocator_h__) */
