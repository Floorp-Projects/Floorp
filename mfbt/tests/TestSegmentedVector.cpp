/* -*- Mode: C++; tab-width: 9; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

// This is included first to ensure it doesn't implicitly depend on anything
// else.
#include "mozilla/SegmentedVector.h"

#include "mozilla/Alignment.h"
#include "mozilla/Assertions.h"

using mozilla::SegmentedVector;

// It would be nice if we could use the InfallibleAllocPolicy from mozalloc,
// but MFBT cannot use mozalloc.
class InfallibleAllocPolicy
{
public:
  template <typename T>
  T* pod_malloc(size_t aNumElems)
  {
    if (aNumElems & mozilla::tl::MulOverflowMask<sizeof(T)>::value) {
      MOZ_CRASH("TestSegmentedVector.cpp: overflow");
    }
    T* rv = static_cast<T*>(malloc(aNumElems * sizeof(T)));
    if (!rv) {
      MOZ_CRASH("TestSegmentedVector.cpp: out of memory");
    }
    return rv;
  }

  template <typename T>
  void free_(T* aPtr, size_t aNumElems = 0) { free(aPtr); }
};

// We want to test Append(), which is fallible and marked with
// MOZ_MUST_USE. But we're using an infallible alloc policy, and so
// don't really need to check the result. Casting to |void| works with clang
// but not GCC, so we instead use this dummy variable which works with both
// compilers.
static int gDummy;

// This tests basic segmented vector construction and iteration.
void TestBasics()
{
  // A SegmentedVector with a POD element type.
  typedef SegmentedVector<int, 1024, InfallibleAllocPolicy> MyVector;
  MyVector v;
  int i, n;

  MOZ_RELEASE_ASSERT(v.IsEmpty());

  // Add 100 elements, then check various things.
  i = 0;
  for ( ; i < 100; i++) {
    gDummy = v.Append(std::move(i));
  }
  MOZ_RELEASE_ASSERT(!v.IsEmpty());
  MOZ_RELEASE_ASSERT(v.Length() == 100);

  n = 0;
  for (auto iter = v.Iter(); !iter.Done(); iter.Next()) {
    MOZ_RELEASE_ASSERT(iter.Get() == n);
    n++;
  }
  MOZ_RELEASE_ASSERT(n == 100);

  // Add another 900 elements, then re-check.
  for ( ; i < 1000; i++) {
    v.InfallibleAppend(std::move(i));
  }
  MOZ_RELEASE_ASSERT(!v.IsEmpty());
  MOZ_RELEASE_ASSERT(v.Length() == 1000);

  n = 0;
  for (auto iter = v.Iter(); !iter.Done(); iter.Next()) {
    MOZ_RELEASE_ASSERT(iter.Get() == n);
    n++;
  }
  MOZ_RELEASE_ASSERT(n == 1000);

  // Pop off all of the elements.
  MOZ_RELEASE_ASSERT(v.Length() == 1000);
  for (int len = (int)v.Length(); len > 0; len--) {
    MOZ_RELEASE_ASSERT(v.GetLast() == len - 1);
    v.PopLast();
  }
  MOZ_RELEASE_ASSERT(v.IsEmpty());
  MOZ_RELEASE_ASSERT(v.Length() == 0);

  // Fill the vector up again to prepare for the clear.
  for (i = 0; i < 1000; i++) {
    v.InfallibleAppend(std::move(i));
  }
  MOZ_RELEASE_ASSERT(!v.IsEmpty());
  MOZ_RELEASE_ASSERT(v.Length() == 1000);

  v.Clear();
  MOZ_RELEASE_ASSERT(v.IsEmpty());
  MOZ_RELEASE_ASSERT(v.Length() == 0);

  // Fill the vector up to verify PopLastN works.
  for (i = 0; i < 1000; ++i) {
    v.InfallibleAppend(std::move(i));
  }
  MOZ_RELEASE_ASSERT(!v.IsEmpty());
  MOZ_RELEASE_ASSERT(v.Length() == 1000);

  // Verify we pop the right amount of elements.
  v.PopLastN(300);
  MOZ_RELEASE_ASSERT(v.Length() == 700);

  // Verify the contents are what we expect.
  n = 0;
  for (auto iter = v.Iter(); !iter.Done(); iter.Next()) {
    MOZ_RELEASE_ASSERT(iter.Get() == n);
    n++;
  }
  MOZ_RELEASE_ASSERT(n == 700);
}

static size_t gNumDefaultCtors;
static size_t gNumExplicitCtors;
static size_t gNumCopyCtors;
static size_t gNumMoveCtors;
static size_t gNumDtors;

struct NonPOD
{
  NonPOD()                { gNumDefaultCtors++; }
  explicit NonPOD(int x)  { gNumExplicitCtors++; }
  NonPOD(NonPOD&)         { gNumCopyCtors++; }
  NonPOD(NonPOD&&)        { gNumMoveCtors++; }
  ~NonPOD()               { gNumDtors++; }
};

// This tests how segmented vectors with non-POD elements construct and
// destruct those elements.
void TestConstructorsAndDestructors()
{
  size_t defaultCtorCalls = 0;
  size_t explicitCtorCalls = 0;
  size_t copyCtorCalls = 0;
  size_t moveCtorCalls = 0;
  size_t dtorCalls = 0;

  {
    static const size_t segmentSize = 64;

    // A SegmentedVector with a non-POD element type.
    NonPOD x(1);                          // explicit constructor called
    explicitCtorCalls++;
    SegmentedVector<NonPOD, segmentSize, InfallibleAllocPolicy> v;
                                          // default constructor called 0 times
    MOZ_RELEASE_ASSERT(v.IsEmpty());
    gDummy = v.Append(x);                 // copy constructor called
    copyCtorCalls++;
    NonPOD y(1);                          // explicit constructor called
    explicitCtorCalls++;
    gDummy = v.Append(std::move(y));  // move constructor called
    moveCtorCalls++;
    NonPOD z(1);                          // explicit constructor called
    explicitCtorCalls++;
    v.InfallibleAppend(std::move(z)); // move constructor called
    moveCtorCalls++;
    v.PopLast();                          // destructor called 1 time
    dtorCalls++;
    MOZ_RELEASE_ASSERT(gNumDtors == dtorCalls);
    v.Clear();                            // destructor called 2 times
    dtorCalls += 2;

    // Test that PopLastN() correctly calls the destructors of all the
    // elements in the segments it destroys.
    //
    // We depend on the size of NonPOD when determining how many things
    // to push onto the vector.  It would be nicer to get this information
    // from SegmentedVector itself...
    static_assert(sizeof(NonPOD) == 1, "Fix length calculations!");

    size_t nonFullLastSegmentSize = segmentSize - 1;
    for (size_t i = 0; i < nonFullLastSegmentSize; ++i) {
      gDummy = v.Append(x);     // copy constructor called
      copyCtorCalls++;
    }
    MOZ_RELEASE_ASSERT(gNumCopyCtors == copyCtorCalls);

    // Pop some of the elements.
    {
      size_t partialPopAmount = 5;
      MOZ_RELEASE_ASSERT(nonFullLastSegmentSize > partialPopAmount);
      v.PopLastN(partialPopAmount); // destructor called partialPopAmount times
      dtorCalls += partialPopAmount;
      MOZ_RELEASE_ASSERT(v.Length() == nonFullLastSegmentSize - partialPopAmount);
      MOZ_RELEASE_ASSERT(!v.IsEmpty());
      MOZ_RELEASE_ASSERT(gNumDtors == dtorCalls);
    }

    // Pop a full segment.
    {
      size_t length = v.Length();
      v.PopLastN(length);
      dtorCalls += length;
      // These two tests *are* semantically different given the underlying
      // implementation; Length sums up the sizes of the internal segments,
      // while IsEmpty looks at the sequence of internal segments.
      MOZ_RELEASE_ASSERT(v.Length() == 0);
      MOZ_RELEASE_ASSERT(v.IsEmpty());
      MOZ_RELEASE_ASSERT(gNumDtors == dtorCalls);
    }

    size_t multipleSegmentsSize = (segmentSize * 3) / 2;
    for (size_t i = 0; i < multipleSegmentsSize; ++i) {
      gDummy = v.Append(x);     // copy constructor called
      copyCtorCalls++;
    }
    MOZ_RELEASE_ASSERT(gNumCopyCtors == copyCtorCalls);

    // Pop across segment boundaries.
    {
      v.PopLastN(segmentSize);
      dtorCalls += segmentSize;
      MOZ_RELEASE_ASSERT(v.Length() == (multipleSegmentsSize - segmentSize));
      MOZ_RELEASE_ASSERT(!v.IsEmpty());
      MOZ_RELEASE_ASSERT(gNumDtors == dtorCalls);
    }

    // Clear everything here to make calculations easier.
    {
      size_t length = v.Length();
      v.Clear();
      dtorCalls += length;
      MOZ_RELEASE_ASSERT(v.IsEmpty());
      MOZ_RELEASE_ASSERT(gNumDtors == dtorCalls);
    }

    MOZ_RELEASE_ASSERT(gNumDefaultCtors  == defaultCtorCalls);
    MOZ_RELEASE_ASSERT(gNumExplicitCtors == explicitCtorCalls);
    MOZ_RELEASE_ASSERT(gNumCopyCtors     == copyCtorCalls);
    MOZ_RELEASE_ASSERT(gNumMoveCtors     == moveCtorCalls);
    MOZ_RELEASE_ASSERT(gNumDtors         == dtorCalls);
  }                                       // destructor called for x, y, z
  dtorCalls += 3;
  MOZ_RELEASE_ASSERT(gNumDtors == dtorCalls);
}

struct A { int mX; int mY; };
struct B { int mX; char mY; double mZ; };
struct C { A mA; B mB; };
struct D { char mBuf[101]; };
struct E { };

// This tests that we get the right segment capacities for specified segment
// sizes, and that the elements are aligned appropriately.
void TestSegmentCapacitiesAndAlignments()
{
  // When SegmentedVector's constructor is passed a size, it asserts that the
  // vector's segment capacity results in a segment size equal to (or very
  // close to) the passed size.
  //
  // Also, SegmentedVector has a static assertion that elements are
  // appropriately aligned.
  SegmentedVector<double, 512> v1(512);
  SegmentedVector<A, 1024> v2(1024);
  SegmentedVector<B, 999> v3(999);
  SegmentedVector<C, 10> v4(10);
  SegmentedVector<D, 1234> v5(1234);
  SegmentedVector<E> v6(4096);  // 4096 is the default segment size
  SegmentedVector<mozilla::AlignedElem<16>, 100> v7(100);
}

void
TestIterator()
{
  SegmentedVector<int, 4> v;

  auto iter = v.Iter();
  auto iterFromLast = v.IterFromLast();
  MOZ_RELEASE_ASSERT(iter.Done());
  MOZ_RELEASE_ASSERT(iterFromLast.Done());

  gDummy = v.Append(1);
  iter = v.Iter();
  iterFromLast = v.IterFromLast();
  MOZ_RELEASE_ASSERT(!iter.Done());
  MOZ_RELEASE_ASSERT(!iterFromLast.Done());

  iter.Next();
  MOZ_RELEASE_ASSERT(iter.Done());
  iterFromLast.Next();
  MOZ_RELEASE_ASSERT(iterFromLast.Done());

  iter = v.Iter();
  iterFromLast = v.IterFromLast();
  MOZ_RELEASE_ASSERT(!iter.Done());
  MOZ_RELEASE_ASSERT(!iterFromLast.Done());

  iter.Prev();
  MOZ_RELEASE_ASSERT(iter.Done());
  iterFromLast.Prev();
  MOZ_RELEASE_ASSERT(iterFromLast.Done());

  // Append enough entries to ensure we have at least two segments.
  gDummy = v.Append(1);
  gDummy = v.Append(1);
  gDummy = v.Append(1);
  gDummy = v.Append(1);

  iter = v.Iter();
  iterFromLast = v.IterFromLast();
  MOZ_RELEASE_ASSERT(!iter.Done());
  MOZ_RELEASE_ASSERT(!iterFromLast.Done());

  iter.Prev();
  MOZ_RELEASE_ASSERT(iter.Done());
  iterFromLast.Next();
  MOZ_RELEASE_ASSERT(iterFromLast.Done());

  iter = v.Iter();
  iterFromLast = v.IterFromLast();
  MOZ_RELEASE_ASSERT(!iter.Done());
  MOZ_RELEASE_ASSERT(!iterFromLast.Done());

  iter.Next();
  MOZ_RELEASE_ASSERT(!iter.Done());
  iterFromLast.Prev();
  MOZ_RELEASE_ASSERT(!iterFromLast.Done());

  iter = v.Iter();
  iterFromLast = v.IterFromLast();
  int count = 0;
  for (; !iter.Done() && !iterFromLast.Done(); iter.Next(), iterFromLast.Prev()) {
    ++count;
  }
  MOZ_RELEASE_ASSERT(count == 5);

  // Modify the vector while using the iterator.
  iterFromLast = v.IterFromLast();
  gDummy = v.Append(2);
  gDummy = v.Append(3);
  gDummy = v.Append(4);
  iterFromLast.Next();
  MOZ_RELEASE_ASSERT(!iterFromLast.Done());
  MOZ_RELEASE_ASSERT(iterFromLast.Get() == 2);
  iterFromLast.Next();
  MOZ_RELEASE_ASSERT(iterFromLast.Get() == 3);
  iterFromLast.Next();
  MOZ_RELEASE_ASSERT(iterFromLast.Get() == 4);
  iterFromLast.Next();
  MOZ_RELEASE_ASSERT(iterFromLast.Done());
}

int main(void)
{
  TestBasics();
  TestConstructorsAndDestructors();
  TestSegmentCapacitiesAndAlignments();
  TestIterator();

  return 0;
}
