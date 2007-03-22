#ifndef nsCppSharedAllocator_h__
#define nsCppSharedAllocator_h__

#include "nsMemory.h"     // for |nsMemory|
#include "nscore.h"       // for |NS_XXX_CAST|
#include NEW_H					// to allow placement |new|


  // under Metrowerks (Mac), we don't have autoconf yet
#ifdef __MWERKS__
  #define HAVE_CPP_MEMBER_TEMPLATES
	#define HAVE_CPP_NUMERIC_LIMITS
#endif

  // under MSVC shut off copious warnings about unused in-lines
#ifdef _MSC_VER
  #pragma warning( disable: 4514 )
#endif

#ifdef HAVE_CPP_NUMERIC_LIMITS
#include <limits>
#else
#include <limits.h>
#endif


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

#ifdef HAVE_CPP_MEMBER_TEMPLATES
      template <class U>
      nsCppSharedAllocator( const nsCppSharedAllocator<U>& ) { }
#endif

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
          return NS_REINTERPRET_CAST(pointer, nsMemory::Alloc(NS_STATIC_CAST(PRUint32, n*sizeof(T))));
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
#ifdef HAVE_CPP_NUMERIC_LIMITS
          return numeric_limits<size_type>::max() / sizeof(T);
#else
          return ULONG_MAX / sizeof(T);
#endif
        }

#ifdef HAVE_CPP_MEMBER_TEMPLATES
      template <class U>
      struct rebind
        {
          typedef nsCppSharedAllocator<U> other;
        };
#endif
  };


template <class T>
PRBool
operator==( const nsCppSharedAllocator<T>&, const nsCppSharedAllocator<T>& )
  {
    return PR_TRUE;
  }

template <class T>
PRBool
operator!=( const nsCppSharedAllocator<T>&, const nsCppSharedAllocator<T>& )
  {
    return PR_FALSE;
  }

#endif /* !defined(nsCppSharedAllocator_h__) */
