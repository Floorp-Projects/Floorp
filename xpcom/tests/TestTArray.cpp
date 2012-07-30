/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include <stdlib.h>
#include <stdio.h>
#include "nsTArray.h"
#include "nsMemory.h"
#include "nsAutoPtr.h"
#include "nsStringAPI.h"
#include "nsDirectoryServiceDefs.h"
#include "nsDirectoryServiceUtils.h"
#include "nsComponentManagerUtils.h"
#include "nsXPCOM.h"
#include "nsIFile.h"

using namespace mozilla;

namespace TestTArray {

// Define this so we can use test_basic_array in test_comptr_array
template <class T>
inline bool operator<(const nsCOMPtr<T>& lhs, const nsCOMPtr<T>& rhs) {
  return lhs.get() < rhs.get();
}

//----

template <class ElementType>
static bool test_basic_array(ElementType *data,
                               PRUint32 dataLen,
                               const ElementType& extra) {
  nsTArray<ElementType> ary;
  ary.AppendElements(data, dataLen);
  if (ary.Length() != dataLen) {
    return false;
  }
  if (!(ary == ary)) {
    return false;
  }
  PRUint32 i;
  for (i = 0; i < ary.Length(); ++i) {
    if (ary[i] != data[i])
      return false;
  }
  for (i = 0; i < ary.Length(); ++i) {
    if (ary.SafeElementAt(i, extra) != data[i])
      return false;
  }
  if (ary.SafeElementAt(ary.Length(), extra) != extra ||
      ary.SafeElementAt(ary.Length() * 10, extra) != extra)
    return false;
  // ensure sort results in ascending order
  ary.Sort();
  PRUint32 j = 0, k;
  if (ary.GreatestIndexLtEq(extra, k))
    return false;
  for (i = 0; i < ary.Length(); ++i) {
    if (!ary.GreatestIndexLtEq(ary[i], k))
      return false;
    if (k < j)
      return false;
    j = k;
  }
  for (i = ary.Length(); --i; ) {
    if (ary[i] < ary[i - 1])
      return false;
    if (ary[i] == ary[i - 1])
      ary.RemoveElementAt(i);
  }
  if (!(ary == ary)) {
    return false;
  }
  for (i = 0; i < ary.Length(); ++i) {
    if (ary.BinaryIndexOf(ary[i]) != i)
      return false;
  }
  if (ary.BinaryIndexOf(extra) != ary.NoIndex)
    return false;
  PRUint32 oldLen = ary.Length();
  ary.RemoveElement(data[dataLen / 2]);
  if (ary.Length() != (oldLen - 1))
    return false;
  if (!(ary == ary))
    return false;

  PRUint32 index = ary.Length() / 2;
  if (!ary.InsertElementAt(index, extra))
    return false;
  if (!(ary == ary))
    return false;
  if (ary[index] != extra)
    return false;
  if (ary.IndexOf(extra) == PR_UINT32_MAX)
    return false;
  if (ary.LastIndexOf(extra) == PR_UINT32_MAX)
    return false;
  // ensure proper searching
  if (ary.IndexOf(extra) > ary.LastIndexOf(extra))
    return false;
  if (ary.IndexOf(extra, index) != ary.LastIndexOf(extra, index))
    return false;

  nsTArray<ElementType> copy(ary);
  if (!(ary == copy))
    return false;
  for (i = 0; i < copy.Length(); ++i) {
    if (ary[i] != copy[i])
      return false;
  }
  if (!ary.AppendElements(copy))
    return false;
  PRUint32 cap = ary.Capacity();
  ary.RemoveElementsAt(copy.Length(), copy.Length());
  ary.Compact();
  if (ary.Capacity() == cap)
    return false;

  ary.Clear();
  if (!ary.IsEmpty() || ary.Elements() == nullptr)
    return false;
  if (!(ary == nsTArray<ElementType>()))
    return false;
  if (ary == copy)
    return false;
  if (ary.SafeElementAt(0, extra) != extra ||
      ary.SafeElementAt(10, extra) != extra)
    return false;

  ary = copy;
  if (!(ary == copy))
    return false;
  for (i = 0; i < copy.Length(); ++i) {
    if (ary[i] != copy[i])
      return false;
  }

  if (!ary.InsertElementsAt(0, copy))
    return false;
  if (ary == copy)
    return false;
  ary.RemoveElementsAt(0, copy.Length());
  for (i = 0; i < copy.Length(); ++i) {
    if (ary[i] != copy[i])
      return false;
  }

  // These shouldn't crash!
  nsTArray<ElementType> empty;
  ary.AppendElements(reinterpret_cast<ElementType *>(0), 0);
  ary.AppendElements(empty);

  // See bug 324981
  ary.RemoveElement(extra);
  ary.RemoveElement(extra);

  return true;
}

static bool test_int_array() {
  int data[] = {4,6,8,2,4,1,5,7,3};
  return test_basic_array(data, ArrayLength(data), int(14));
}

static bool test_int64_array() {
  PRInt64 data[] = {4,6,8,2,4,1,5,7,3};
  return test_basic_array(data, ArrayLength(data), PRInt64(14));
}

static bool test_char_array() {
  char data[] = {4,6,8,2,4,1,5,7,3};
  return test_basic_array(data, ArrayLength(data), char(14));
}

static bool test_uint32_array() {
  PRUint32 data[] = {4,6,8,2,4,1,5,7,3};
  return test_basic_array(data, ArrayLength(data), PRUint32(14));
}

//----

class Object {
  public:
    Object() : mNum(0) {
    }
    Object(const char *str, PRUint32 num) : mStr(str), mNum(num) {
    }
    Object(const Object& other) : mStr(other.mStr), mNum(other.mNum) {
    }
    ~Object() {}

    Object& operator=(const Object& other) {
      mStr = other.mStr;
      mNum = other.mNum;
      return *this;
    }

    bool operator==(const Object& other) const {
      return mStr == other.mStr && mNum == other.mNum;
    }

    bool operator<(const Object& other) const {
      // sort based on mStr only
      return mStr.Compare(other.mStr) < 0;
    }

    const char *Str() const { return mStr.get(); }
    PRUint32 Num() const { return mNum; }

  private:
    nsCString mStr;
    PRUint32  mNum;
};

static bool test_object_array() {
  nsTArray<Object> objArray;
  const char kdata[] = "hello world";
  PRUint32 i;
  for (i = 0; i < ArrayLength(kdata); ++i) {
    char x[] = {kdata[i],'\0'};
    if (!objArray.AppendElement(Object(x, i)))
      return false;
  }
  for (i = 0; i < ArrayLength(kdata); ++i) {
    if (objArray[i].Str()[0] != kdata[i])
      return false;
    if (objArray[i].Num() != i)
      return false;
  }
  objArray.Sort();
  const char ksorted[] = "\0 dehllloorw";
  for (i = 0; i < ArrayLength(kdata)-1; ++i) {
    if (objArray[i].Str()[0] != ksorted[i])
      return false;
  }
  return true;
}

// nsTArray<nsAutoPtr<T>> is not supported
#if 0
static bool test_autoptr_array() {
  nsTArray< nsAutoPtr<Object> > objArray;
  const char kdata[] = "hello world";
  for (PRUint32 i = 0; i < ArrayLength(kdata); ++i) {
    char x[] = {kdata[i],'\0'};
    nsAutoPtr<Object> obj(new Object(x,i));
    if (!objArray.AppendElement(obj))  // XXX does not call copy-constructor for nsAutoPtr!!!
      return false;
    if (obj.get() == nullptr)
      return false;
    obj.forget();  // the array now owns the reference
  }
  for (PRUint32 i = 0; i < ArrayLength(kdata); ++i) {
    if (objArray[i]->Str()[0] != kdata[i])
      return false;
    if (objArray[i]->Num() != i)
      return false;
  }
  return true;
}
#endif

//----

static bool test_string_array() {
  nsTArray<nsCString> strArray;
  const char kdata[] = "hello world";
  PRUint32 i;
  for (i = 0; i < ArrayLength(kdata); ++i) {
    nsCString str;
    str.Assign(kdata[i]);
    if (!strArray.AppendElement(str))
      return false;
  }
  for (i = 0; i < ArrayLength(kdata); ++i) {
    if (strArray[i].CharAt(0) != kdata[i])
      return false;
  }

  const char kextra[] = "foo bar";
  PRUint32 oldLen = strArray.Length();
  if (!strArray.AppendElement(kextra))
    return false;
  strArray.RemoveElement(kextra);
  if (oldLen != strArray.Length())
    return false;

  if (strArray.IndexOf("e") != 1)
    return false;

  strArray.Sort();
  const char ksorted[] = "\0 dehllloorw";
  for (i = ArrayLength(kdata); i--; ) {
    if (strArray[i].CharAt(0) != ksorted[i])
      return false;
    if (i > 0 && strArray[i] == strArray[i - 1])
      strArray.RemoveElementAt(i);
  }
  for (i = 0; i < strArray.Length(); ++i) {
    if (strArray.BinaryIndexOf(strArray[i]) != i)
      return false;
  }
  if (strArray.BinaryIndexOf(EmptyCString()) != strArray.NoIndex)
    return false;

  nsCString rawArray[NS_ARRAY_LENGTH(kdata) - 1];
  for (i = 0; i < ArrayLength(rawArray); ++i)
    rawArray[i].Assign(kdata + i);  // substrings of kdata
  return test_basic_array(rawArray, ArrayLength(rawArray),
                          nsCString("foopy"));
}

//----

typedef nsCOMPtr<nsIFile> FilePointer;

class nsFileNameComparator {
  public:
    bool Equals(const FilePointer &a, const char *b) const {
      nsCAutoString name;
      a->GetNativeLeafName(name);
      return name.Equals(b);
    }
};

static bool test_comptr_array() {
  FilePointer tmpDir;
  NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(tmpDir));
  if (!tmpDir)
    return false;
  const char *kNames[] = {
    "foo.txt", "bar.html", "baz.gif"
  };
  nsTArray<FilePointer> fileArray;
  PRUint32 i;
  for (i = 0; i < ArrayLength(kNames); ++i) {
    FilePointer f;
    tmpDir->Clone(getter_AddRefs(f));
    if (!f)
      return false;
    if (NS_FAILED(f->AppendNative(nsDependentCString(kNames[i]))))
      return false;
    fileArray.AppendElement(f);
  }

  if (fileArray.IndexOf(kNames[1], 0, nsFileNameComparator()) != 1)
    return false;

  // It's unclear what 'operator<' means for nsCOMPtr, but whatever...
  return test_basic_array(fileArray.Elements(), fileArray.Length(), 
                          tmpDir);
}

//----

class RefcountedObject {
  public:
    RefcountedObject() : rc(0) {}
    void AddRef() {
      ++rc;
    }
    void Release() {
      if (--rc == 0)
        delete this;
    }
    ~RefcountedObject() {}
  private:
    PRInt32 rc;
};

static bool test_refptr_array() {
  bool rv = true;

  nsTArray< nsRefPtr<RefcountedObject> > objArray;

  RefcountedObject *a = new RefcountedObject(); a->AddRef();
  RefcountedObject *b = new RefcountedObject(); b->AddRef();
  RefcountedObject *c = new RefcountedObject(); c->AddRef();

  objArray.AppendElement(a);
  objArray.AppendElement(b);
  objArray.AppendElement(c);

  if (objArray.IndexOf(b) != 1)
    rv = false;

  a->Release();
  b->Release();
  c->Release();
  return rv;
}

//----

static bool test_ptrarray() {
  nsTArray<PRUint32*> ary;
  if (ary.SafeElementAt(0) != nullptr)
    return false;
  if (ary.SafeElementAt(1000) != nullptr)
    return false;
  PRUint32 a = 10;
  ary.AppendElement(&a);
  if (*ary[0] != a)
    return false;
  if (*ary.SafeElementAt(0) != a)
    return false;

  nsTArray<const PRUint32*> cary;
  if (cary.SafeElementAt(0) != nullptr)
    return false;
  if (cary.SafeElementAt(1000) != nullptr)
    return false;
  const PRUint32 b = 14;
  cary.AppendElement(&a);
  cary.AppendElement(&b);
  if (*cary[0] != a || *cary[1] != b)
    return false;
  if (*cary.SafeElementAt(0) != a || *cary.SafeElementAt(1) != b)
    return false;

  return true;
}

//----

// This test relies too heavily on the existence of DebugGetHeader to be
// useful in non-debug builds.
#ifdef DEBUG
static bool test_autoarray() {
  PRUint32 data[] = {4,6,8,2,4,1,5,7,3};
  nsAutoTArray<PRUint32, NS_ARRAY_LENGTH(data)> array;

  void* hdr = array.DebugGetHeader();
  if (hdr == nsTArray<PRUint32>().DebugGetHeader())
    return false;
  if (hdr == nsAutoTArray<PRUint32, NS_ARRAY_LENGTH(data)>().DebugGetHeader())
    return false;

  array.AppendElement(1u);
  if (hdr != array.DebugGetHeader())
    return false;

  array.RemoveElement(1u);
  array.AppendElements(data, ArrayLength(data));
  if (hdr != array.DebugGetHeader())
    return false;

  array.AppendElement(2u);
  if (hdr == array.DebugGetHeader())
    return false;

  array.Clear();
  array.Compact();
  if (hdr != array.DebugGetHeader())
    return false;
  array.AppendElements(data, ArrayLength(data));
  if (hdr != array.DebugGetHeader())
    return false;

  nsTArray<PRUint32> array2;
  void* emptyHdr = array2.DebugGetHeader();
  array.SwapElements(array2);
  if (emptyHdr == array.DebugGetHeader())
    return false;
  if (hdr == array2.DebugGetHeader())
    return false;
  PRUint32 i;
  for (i = 0; i < ArrayLength(data); ++i) {
    if (array2[i] != data[i])
      return false;
  }
  if (!array.IsEmpty())
    return false;

  array.Compact();
  array.AppendElements(data, ArrayLength(data));
  PRUint32 data3[] = {5, 7, 11};
  nsAutoTArray<PRUint32, NS_ARRAY_LENGTH(data3)> array3;
  array3.AppendElements(data3, ArrayLength(data3));  
  array.SwapElements(array3);
  for (i = 0; i < ArrayLength(data); ++i) {
    if (array3[i] != data[i])
      return false;
  }
  for (i = 0; i < ArrayLength(data3); ++i) {
    if (array[i] != data3[i])
      return false;
  }

  return true;
}
#endif

//----

// IndexOf used to potentially scan beyond the end of the array.  Test for
// this incorrect behavior by adding a value (5), removing it, then seeing
// if IndexOf finds it.
static bool test_indexof() {
  nsTArray<int> array;
  array.AppendElement(0);
  // add and remove the 5
  array.AppendElement(5);
  array.RemoveElementAt(1);
  // we should not find the 5!
  return array.IndexOf(5, 1) == array.NoIndex;
}

//----

template <class Array>
static bool is_heap(const Array& ary, PRUint32 len) {
  PRUint32 index = 1;
  while (index < len) {
    if (ary[index] > ary[(index - 1) >> 1])
      return false;
    index++;
  }
  return true;
} 

static bool test_heap() {
  const int data[] = {4,6,8,2,4,1,5,7,3};
  nsTArray<int> ary;
  ary.AppendElements(data, ArrayLength(data));
  // make a heap and make sure it's a heap
  ary.MakeHeap();
  if (!is_heap(ary, ArrayLength(data)))
    return false;
  // pop the root and make sure it's still a heap
  int root = ary[0];
  ary.PopHeap();
  if (!is_heap(ary, ArrayLength(data) - 1))
    return false;
  // push the previously poped value back on and make sure it's still a heap
  ary.PushHeap(root);
  if (!is_heap(ary, ArrayLength(data)))
    return false;
  // make sure the heap looks like what we expect
  const int expected_data[] = {8,7,5,6,4,1,4,2,3};
  PRUint32 index;
  for (index = 0; index < ArrayLength(data); index++)
    if (ary[index] != expected_data[index])
      return false;
  return true;
}

//----

// An array |arr| is using its auto buffer if |&arr < arr.Elements()| and
// |arr.Elements() - &arr| is small.

#define IS_USING_AUTO(arr) \
  ((uintptr_t) &(arr) < (uintptr_t) arr.Elements() && \
   ((PRPtrdiff)arr.Elements() - (PRPtrdiff)&arr) <= 16)

#define CHECK_IS_USING_AUTO(arr) \
  do {                                                    \
    if (!(IS_USING_AUTO(arr))) {                          \
      printf("%s:%d CHECK_IS_USING_AUTO(%s) failed.\n",   \
             __FILE__, __LINE__, #arr);                   \
      return false;                                    \
    }                                                     \
  } while(0)

#define CHECK_NOT_USING_AUTO(arr) \
  do {                                                    \
    if (IS_USING_AUTO(arr)) {                             \
      printf("%s:%d CHECK_NOT_USING_AUTO(%s) failed.\n",  \
             __FILE__, __LINE__, #arr);                   \
      return false;                                    \
    }                                                     \
  } while(0)

#define CHECK_USES_SHARED_EMPTY_HDR(arr) \
  do {                                                    \
    nsTArray<int> _empty;                                 \
    if (_empty.Elements() != arr.Elements()) {            \
      printf("%s:%d CHECK_USES_EMPTY_HDR(%s) failed.\n",  \
             __FILE__, __LINE__, #arr);                   \
      return false;                                    \
    }                                                     \
  } while(0)

#define CHECK_EQ_INT(actual, expected) \
  do {                                                                       \
    if ((actual) != (expected)) {                                            \
      printf("%s:%d CHECK_EQ_INT(%s=%u, %s=%u) failed.\n",                   \
             __FILE__, __LINE__, #actual, (actual), #expected, (expected));  \
      return false;                                                       \
    }                                                                        \
  } while(0)

#define CHECK_ARRAY(arr, data) \
  do {                                                          \
    CHECK_EQ_INT((arr).Length(), (PRUint32)ArrayLength(data));  \
    for (PRUint32 _i = 0; _i < ArrayLength(data); _i++) {       \
      CHECK_EQ_INT((arr)[_i], (data)[_i]);                      \
    }                                                           \
  } while(0)

static bool test_swap() {
  // Test nsTArray::SwapElements.  Unfortunately there are many cases.
  int data1[] = {8, 6, 7, 5};
  int data2[] = {3, 0, 9};

  // Swap two auto arrays.
  {
    nsAutoTArray<int, 8> a;
    nsAutoTArray<int, 6> b;

    a.AppendElements(data1, ArrayLength(data1));
    b.AppendElements(data2, ArrayLength(data2));
    CHECK_IS_USING_AUTO(a);
    CHECK_IS_USING_AUTO(b);

    a.SwapElements(b);

    CHECK_IS_USING_AUTO(a);
    CHECK_IS_USING_AUTO(b);
    CHECK_ARRAY(a, data2);
    CHECK_ARRAY(b, data1);
  }

  // Swap two auto arrays -- one whose data lives on the heap, the other whose
  // data lives on the stack -- which each fits into the other's auto storage.
  {
    nsAutoTArray<int, 3> a;
    nsAutoTArray<int, 3> b;

    a.AppendElements(data1, ArrayLength(data1));
    a.RemoveElementAt(3);
    b.AppendElements(data2, ArrayLength(data2));

    // Here and elsewhere, we assert that if we start with an auto array
    // capable of storing N elements, we store N+1 elements into the array, and
    // then we remove one element, that array is still not using its auto
    // buffer.
    //
    // This isn't at all required by the TArray API. It would be fine if, when
    // we shrink back to N elements, the TArray frees its heap storage and goes
    // back to using its stack storage.  But we assert here as a check that the
    // test does what we expect.  If the TArray implementation changes, just
    // change the failing assertions.
    CHECK_NOT_USING_AUTO(a);

    // This check had better not change, though.
    CHECK_IS_USING_AUTO(b);

    a.SwapElements(b);

    CHECK_IS_USING_AUTO(b);
    CHECK_ARRAY(a, data2);
    int expectedB[] = {8, 6, 7};
    CHECK_ARRAY(b, expectedB);
  }

  // Swap two auto arrays which are using heap storage such that one fits into
  // the other's auto storage, but the other needs to stay on the heap.
  {
    nsAutoTArray<int, 3> a;
    nsAutoTArray<int, 2> b;
    a.AppendElements(data1, ArrayLength(data1));
    a.RemoveElementAt(3);

    b.AppendElements(data2, ArrayLength(data2));
    b.RemoveElementAt(2);

    CHECK_NOT_USING_AUTO(a);
    CHECK_NOT_USING_AUTO(b);

    a.SwapElements(b);

    CHECK_NOT_USING_AUTO(b);

    int expected1[] = {3, 0};
    int expected2[] = {8, 6, 7};

    CHECK_ARRAY(a, expected1);
    CHECK_ARRAY(b, expected2);
  }

  // Swap two arrays, neither of which fits into the other's auto-storage.
  {
    nsAutoTArray<int, 1> a;
    nsAutoTArray<int, 3> b;

    a.AppendElements(data1, ArrayLength(data1));
    b.AppendElements(data2, ArrayLength(data2));

    a.SwapElements(b);

    CHECK_ARRAY(a, data2);
    CHECK_ARRAY(b, data1);
  }

  // Swap an empty nsTArray with a non-empty nsAutoTArray.
  {
    nsTArray<int> a;
    nsAutoTArray<int, 3> b;

    b.AppendElements(data2, ArrayLength(data2));
    CHECK_IS_USING_AUTO(b);

    a.SwapElements(b);

    CHECK_ARRAY(a, data2);
    CHECK_EQ_INT(b.Length(), 0);
    CHECK_IS_USING_AUTO(b);
  }

  // Swap two big auto arrays.
  {
    const int size = 8192;
    nsAutoTArray<int, size> a;
    nsAutoTArray<int, size> b;

    for (int i = 0; i < size; i++) {
      a.AppendElement(i);
      b.AppendElement(i + 1);
    }

    CHECK_IS_USING_AUTO(a);
    CHECK_IS_USING_AUTO(b);

    a.SwapElements(b);

    CHECK_IS_USING_AUTO(a);
    CHECK_IS_USING_AUTO(b);

    CHECK_EQ_INT(a.Length(), size);
    CHECK_EQ_INT(b.Length(), size);

    for (int i = 0; i < size; i++) {
      CHECK_EQ_INT(a[i], i + 1);
      CHECK_EQ_INT(b[i], i);
    }
  }

  // Swap two arrays and make sure that their capacities don't increase
  // unnecessarily.
  {
    nsTArray<int> a;
    nsTArray<int> b;
    b.AppendElements(data2, ArrayLength(data2));

    CHECK_EQ_INT(a.Capacity(), 0);
    PRUint32 bCapacity = b.Capacity();

    a.SwapElements(b);

    // Make sure that we didn't increase the capacity of either array.
    CHECK_ARRAY(a, data2);
    CHECK_EQ_INT(b.Length(), 0);
    CHECK_EQ_INT(b.Capacity(), 0);
    CHECK_EQ_INT(a.Capacity(), bCapacity);
  }

  // Swap an auto array with a TArray, then clear the auto array and make sure
  // it doesn't forget the fact that it has an auto buffer.
  {
    nsTArray<int> a;
    nsAutoTArray<int, 3> b;

    a.AppendElements(data1, ArrayLength(data1));

    a.SwapElements(b);

    CHECK_EQ_INT(a.Length(), 0);
    CHECK_ARRAY(b, data1);

    b.Clear();

    CHECK_USES_SHARED_EMPTY_HDR(a);
    CHECK_IS_USING_AUTO(b);
  }

  // Same thing as the previous test, but with more auto arrays.
  {
    nsAutoTArray<int, 16> a;
    nsAutoTArray<int, 3> b;

    a.AppendElements(data1, ArrayLength(data1));

    a.SwapElements(b);

    CHECK_EQ_INT(a.Length(), 0);
    CHECK_ARRAY(b, data1);

    b.Clear();

    CHECK_IS_USING_AUTO(a);
    CHECK_IS_USING_AUTO(b);
  }

  // Swap an empty nsTArray and an empty nsAutoTArray.
  {
    nsAutoTArray<int, 8> a;
    nsTArray<int> b;

    a.SwapElements(b);

    CHECK_IS_USING_AUTO(a);
    CHECK_NOT_USING_AUTO(b);
    CHECK_EQ_INT(a.Length(), 0);
    CHECK_EQ_INT(b.Length(), 0);
  }

  // Swap empty auto array with non-empty nsAutoTArray using malloc'ed storage.
  // I promise, all these tests have a point.
  {
    nsAutoTArray<int, 2> a;
    nsAutoTArray<int, 1> b;

    a.AppendElements(data1, ArrayLength(data1));

    a.SwapElements(b);

    CHECK_IS_USING_AUTO(a);
    CHECK_NOT_USING_AUTO(b);
    CHECK_ARRAY(b, data1);
    CHECK_EQ_INT(a.Length(), 0);
  }

  return true;
}

static bool test_fallible()
{
  // Test that FallibleTArray works properly; that is, it never OOMs, but
  // instead eventually returns false.
  //
  // This test is only meaningful on 32-bit systems.  On a 64-bit system, we
  // might never OOM.
  if (sizeof(void*) > 4) {
    return true;
  }

  // Allocate a bunch of 512MB arrays.  We could go bigger, but nsTArray will
  // bail before even attempting to malloc() for gigantic arrays.  512MB should
  // be under that threshold.
  //
  // 9 * 512MB > 4GB, so we should definitely OOM by the 9th array.
  const unsigned numArrays = 9;
  FallibleTArray<char> arrays[numArrays];
  for (PRUint32 i = 0; i < numArrays; i++) {
    bool success = arrays[i].SetCapacity(512 * 1024 * 1024);
    if (!success) {
      // We got our OOM.  Check that it didn't come too early.
      if (i < 2) {
        printf("test_fallible: Got OOM on iteration %d.  Too early!\n", i);
        return false;
      }
      return true;
    }
  }

  // No OOM?  That's...weird.
  printf("test_fallible: Didn't OOM or crash?  nsTArray::SetCapacity "
         "must be lying.\n");
  return false;
}

//----

typedef bool (*TestFunc)();
#define DECL_TEST(name) { #name, name }

static const struct Test {
  const char* name;
  TestFunc    func;
} tests[] = {
  DECL_TEST(test_int_array),
  DECL_TEST(test_int64_array),
  DECL_TEST(test_char_array),
  DECL_TEST(test_uint32_array),
  DECL_TEST(test_object_array),
  DECL_TEST(test_string_array),
  DECL_TEST(test_comptr_array),
  DECL_TEST(test_refptr_array),
  DECL_TEST(test_ptrarray),
#ifdef DEBUG
  DECL_TEST(test_autoarray),
#endif
  DECL_TEST(test_indexof),
  DECL_TEST(test_heap),
  DECL_TEST(test_swap),
  DECL_TEST(test_fallible),
  { nullptr, nullptr }
};

}

using namespace TestTArray;

int main(int argc, char **argv) {
  int count = 1;
  if (argc > 1)
    count = atoi(argv[1]);

  if (NS_FAILED(NS_InitXPCOM2(nullptr, nullptr, nullptr)))
    return -1;

  bool success = true;
  while (count--) {
    for (const Test* t = tests; t->name != nullptr; ++t) {
      bool test_result = t->func();
      printf("%25s : %s\n", t->name, test_result ? "SUCCESS" : "FAILURE");
      if (!test_result)
        success = false;
    }
  }
  
  NS_ShutdownXPCOM(nullptr);
  return success ? 0 : -1;
}
