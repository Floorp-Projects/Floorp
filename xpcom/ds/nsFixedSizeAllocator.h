/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Waterson <waterson@netscape.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*

  A simple fixed-size allocator that allocates its memory from an
  arena.

  Although the allocator can handle blocks of any size, its
  preformance will degrade rapidly if used to allocate blocks of
  arbitrary size. Ideally, it should be used to allocate and recycle a
  large number of fixed-size blocks.

  Here is a typical usage pattern:

    #include "nsFixedSizeAllocator.h"

    // Say this is the object you want to allocate a ton of
    class Foo {
    public:
      // Implement placement new & delete operators that will
      // use the fixed size allocator.
      static operator new(size_t aSize, nsFixedSizeAllocator& aAllocator) {
        return aAllocator.Alloc(aSize); }

      static operator delete(void* aPtr, size_t aSize) {
        nsFixedSizeAllocator::Free(aPtr, aSize); }

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
      static const PRInt32 kInitialPoolSize =
        NS_SIZE_IN_HEAP(sizeof(Foo)) * 1024;

      // Initialize (or re-initialize) the pool
      pool.Init("TheFooPool", kBucketSizes, kNumBuckets, kInitialPoolSize);

      // Now we can use the pool.

      // Create a new Foo object using the pool:
      Foo* foo = new (pool) Foo();
      if (! foo) {
        // uh oh, out of memory!
      }

      // Delete the object. The memory used by `foo' is recycled in
      // the pool, and placed in a freelist
      delete foo;

      // Create another foo: this one will be allocated from the
      // free-list
      foo = new (pool) foo();

      // When pool is destroyed, all of its memory is automatically
      // freed. N.B. it will *not* call your objects' destructors! In
      // this case, foo's ~Foo() method would never be called.
    }

*/

#ifndef nsFixedSizeAllocator_h__
#define nsFixedSizeAllocator_h__

#include "nscore.h"
#include "nsCom.h"
#include "nsError.h"
#include "plarena.h"

#define NS_SIZE_IN_HEAP(_size) (_size)

class NS_COM nsFixedSizeAllocator
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
