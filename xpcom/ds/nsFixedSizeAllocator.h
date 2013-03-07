/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  A simple fixed-size allocator that allocates its memory from an
  arena.

  WARNING: you probably shouldn't use this class.  If you are thinking of using
  it, you should have measurements that indicate it has a clear advantage over
  vanilla malloc/free or vanilla new/delete.

  This allocator has the following notable properties.

  - Although it can handle blocks of any size, its performance will degrade
    rapidly if used to allocate blocks of many different sizes.  Ideally, it
    should be used to allocate and recycle many fixed-size blocks.

  - None of the chunks allocated are released back to the OS unless Init() is
    re-called or the nsFixedSizeAllocator is destroyed.  So it's generally a
    bad choice if it might live for a long time (e.g. see bug 847210).

  - You have to manually construct and destruct objects allocated with it.
    Furthermore, any objects that haven't been freed when the allocator is
    destroyed won't have their destructors run.  So if you are allocating
    objects that have destructors they should all be manually freed (and
    destructed) before the allocator is destroyed.

  - It does no locking and so is not thread-safe.  If all allocations and
    deallocations are on a single thread, this is fine and these operations
    might be faster than vanilla malloc/free due to the lack of locking.

    Otherwise, you need to add locking yourself.  In unusual circumstances,
    this can be a good thing, because it reduces contention over the main
    malloc/free lock.  See TimerEventAllocator and bug 733277 for an example.

  - Because it's an arena-style allocator, it might reduce fragmentation,
    because objects allocated with the arena won't be co-allocated with
    longer-lived objects.  However, this is hard to demonstrate and you should
    not assume the effects are significant without conclusive measurements.

  Here is a typical usage pattern.  Note that no locking is done in this
  example so it's only safe for use on a single thread.

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
        return place ? ::new (place) Foo() : nullptr;
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
      static const size_t kBucketSizes[] = { sizeof(Foo) }

      // This is the number of different "buckets" you'll need for
      // fixed size objects. In our example, this will be "1".
      static const int32_t kNumBuckets = sizeof(kBucketSizes) / sizeof(size_t);

      // This is the size of the chunks used by the allocator, which should be
      // a power of two to minimize slop.
      static const int32_t kInitialPoolSize = 4096;

      // Initialize (or re-initialize) the pool.
      pool.Init("TheFooPool", kBucketSizes, kNumBuckets, kInitialPoolSize);

      // Create a new Foo object using the pool:
      Foo* foo = Foo::Create(pool);
      if (!foo) {
        // uh oh, out of memory!
      }

      // Delete the object. The memory used by `foo' is recycled in
      // the pool, and placed in a freelist.
      Foo::Destroy(foo);

      // Create another foo: this one will be allocated from the
      // free-list.
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
    nsFixedSizeAllocator() : mBuckets(nullptr) {}

    ~nsFixedSizeAllocator() {
        if (mBuckets)
            PL_FinishArenaPool(&mPool);
    }

    /**
     * Initialize the fixed size allocator.
     * - 'aName' is used to tag the underlying PLArena object for debugging and
     *   measurement purposes.
     * - 'aNumBuckets' specifies the number of elements in 'aBucketSizes'.
     * - 'aBucketSizes' is an array of integral block sizes that this allocator
     *   should be prepared to handle.
     * - 'aChunkSize' is the size of the chunks used.  It should be a power of
     *   two to minimize slop bytes caused by the underlying malloc
     *   implementation rounding up request sizes.  Some of the space in each
     *   chunk will be used by the nsFixedSizeAllocator (or the underlying
     *   PLArena) itself.
     */
    nsresult
    Init(const char* aName,
         const size_t* aBucketSizes,
         int32_t aNumBuckets,
         int32_t aChunkSize,
         int32_t aAlign = 0);

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
