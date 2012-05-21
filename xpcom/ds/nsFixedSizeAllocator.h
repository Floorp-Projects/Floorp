/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*

  A simple fixed-size allocator that allocates its memory from an
  arena.

  Although the allocator can handle blocks of any size, its
  preformance will degrade rapidly if used to allocate blocks of
  arbitrary size. Ideally, it should be used to allocate and recycle a
  large number of fixed-size blocks.

  Here is a typical usage pattern:

    #include NEW_H // You'll need this!
    #include "nsFixedSizeAllocator.h"

    // Say this is the object you want to allocate a ton of
    class Foo {
    public:
      // Implement placement new & delete operators that will
      // use the fixed size allocator.
      static Foo *
      Create(nsFixedSizeAllocator &aAllocator)
      {
        void *place = aAllocator.Alloc(sizeof(Foo));
        return place ? ::new (place) Foo() : nsnull;
      }

      static void
      Destroy(nsFixedSizeAllocator &aAllocator, Foo *aFoo)
      {
        aFoo->~Foo();
        aAllocator.Free(aFoo, sizeof(Foo));
      }

      // ctor & dtor
      Foo() {}
      ~Foo() {}
    };


    int main(int argc, char* argv[])
    {
      // Somewhere in your code, you'll need to create an
      // nsFixedSizeAllocator object and initialize it:
      nsFixedSizeAllocator pool;

      // The fixed size allocator will support multiple fixed sizes.
      // This array lists an initial set of sizes that the allocator
      // should be prepared to support. In our case, there's just one,
      // which is Foo.
      static const size_t kBucketSizes[]
        = { sizeof(Foo) }

      // This is the number of different "buckets" you'll need for
      // fixed size objects. In our example, this will be "1".
      static const PRInt32 kNumBuckets
        = sizeof(kBucketSizes) / sizeof(size_t);

      // This is the intial size of the allocator, in bytes. We'll
      // assume that we want to start with space for 1024 Foo objects.
      static const PRInt32 kInitialPoolSize = sizeof(Foo) * 1024;

      // Initialize (or re-initialize) the pool
      pool.Init("TheFooPool", kBucketSizes, kNumBuckets, kInitialPoolSize);

      // Now we can use the pool.

      // Create a new Foo object using the pool:
      Foo* foo = Foo::Create(pool);
      if (! foo) {
        // uh oh, out of memory!
      }

      // Delete the object. The memory used by `foo' is recycled in
      // the pool, and placed in a freelist
      Foo::Destroy(foo);

      // Create another foo: this one will be allocated from the
      // free-list
      foo = Foo::Create(pool);

      // When pool is destroyed, all of its memory is automatically
      // freed. N.B. it will *not* call your objects' destructors! In
      // this case, foo's ~Foo() method would never be called.
    }

*/

#ifndef nsFixedSizeAllocator_h__
#define nsFixedSizeAllocator_h__

#include "nscore.h"
#include "nsError.h"
#include "plarena.h"

class nsFixedSizeAllocator
{
protected:
    PLArenaPool mPool;

    struct Bucket;
    struct FreeEntry;
  
    friend struct Bucket;
    friend struct FreeEntry;

    struct FreeEntry {
        FreeEntry* mNext;
    };

    struct Bucket {
        size_t     mSize;
        FreeEntry* mFirst;
        Bucket*    mNext;
    };

    Bucket* mBuckets;

    Bucket *
    AddBucket(size_t aSize);

    Bucket *
    FindBucket(size_t aSize);

public:
    nsFixedSizeAllocator() : mBuckets(nsnull) {}

    ~nsFixedSizeAllocator() {
        if (mBuckets)
            PL_FinishArenaPool(&mPool);
    }

    /**
     * Initialize the fixed size allocator. 'aName' is used to tag
     * the underlying PLArena object for debugging and measurement
     * purposes. 'aNumBuckets' specifies the number of elements in
     * 'aBucketSizes', which is an array of integral block sizes
     * that this allocator should be prepared to handle.
     */
    nsresult
    Init(const char* aName,
         const size_t* aBucketSizes,
         PRInt32 aNumBuckets,
         PRInt32 aInitialSize,
         PRInt32 aAlign = 0);

    /**
     * Allocate a block of memory 'aSize' bytes big.
     */
    void* Alloc(size_t aSize);

    /**
     * Free a pointer allocated using a fixed-size allocator
     */
    void Free(void* aPtr, size_t aSize);
};



#endif // nsFixedSizeAllocator_h__
