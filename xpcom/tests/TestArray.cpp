/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <stdlib.h>
#include "nsISupportsArray.h"

// {9e70a320-be02-11d1-8031-006008159b5a}
#define NS_IFOO_IID \
  {0x9e70a320, 0xbe02, 0x11d1,    \
    {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

namespace TestArray {

static const bool kExitOnError = true;

class IFoo : public nsISupports {
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IFOO_IID)

  NS_IMETHOD_(nsrefcnt) RefCnt() = 0;
  NS_IMETHOD_(int32_t) ID() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(IFoo, NS_IFOO_IID)

class Foo : public IFoo {
public:

  Foo(int32_t aID);

  // nsISupports implementation
  NS_DECL_ISUPPORTS

  // IFoo implementation
  NS_IMETHOD_(nsrefcnt) RefCnt() { return mRefCnt; }
  NS_IMETHOD_(int32_t) ID() { return mID; }

  static int32_t gCount;

  int32_t mID;

private:
  ~Foo();
};

int32_t Foo::gCount;

Foo::Foo(int32_t aID)
{
  mID = aID;
  ++gCount;
  fprintf(stdout, "init: %d (%p), %d total)\n",
          mID, static_cast<void*>(this), gCount);
}

Foo::~Foo()
{
  --gCount;
  fprintf(stdout, "destruct: %d (%p), %d remain)\n",
          mID, static_cast<void*>(this), gCount);
}

NS_IMPL_ISUPPORTS1(Foo, IFoo)

const char* AssertEqual(int32_t aValue1, int32_t aValue2)
{
  if (aValue1 == aValue2) {
    return "OK";
  }
  if (kExitOnError) {
    exit(1);
  }
  return "ERROR";
}

void DumpArray(nsISupportsArray* aArray, int32_t aExpectedCount, int32_t aElementIDs[], int32_t aExpectedTotal)
{
  uint32_t cnt = 0;
#ifdef DEBUG
  nsresult rv =
#endif
    aArray->Count(&cnt);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Count failed");
  int32_t count = cnt;
  int32_t index;

  fprintf(stdout, "object count %d = %d %s\n", Foo::gCount, aExpectedTotal, 
          AssertEqual(Foo::gCount, aExpectedTotal));
  fprintf(stdout, "array count %d = %d %s\n", count, aExpectedCount,
          AssertEqual(count, aExpectedCount));
  
  for (index = 0; (index < count) && (index < aExpectedCount); index++) {
    IFoo* foo = (IFoo*)(aArray->ElementAt(index));
    fprintf(stdout, "%2d: %d=%d (%p) c: %d %s\n", 
            index, aElementIDs[index], foo->ID(),
            static_cast<void*>(foo), foo->RefCnt() - 1,
            AssertEqual(foo->ID(), aElementIDs[index]));
    foo->Release();
  }
}

void FillArray(nsISupportsArray* aArray, int32_t aCount)
{
  int32_t index;
  for (index = 0; index < aCount; index++) {
    nsCOMPtr<IFoo> foo = new Foo(index);
    aArray->AppendElement(foo);
  }
}

}

using namespace TestArray;

int main(int argc, char *argv[])
{
  nsISupportsArray* array;
  nsresult  rv;
  
  if (NS_OK == (rv = NS_NewISupportsArray(&array))) {
    FillArray(array, 10);
    fprintf(stdout, "Array created:\n");
    int32_t   fillResult[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    DumpArray(array, 10, fillResult, 10);

    // test insert
    IFoo* foo = (IFoo*)array->ElementAt(3);
    foo->Release();  // pre-release to fix ref count for dumps
    array->InsertElementAt(foo, 5);
    fprintf(stdout, "insert 3 at 5:\n");
    int32_t   insertResult[11] = {0, 1, 2, 3, 4, 3, 5, 6, 7, 8, 9};
    DumpArray(array, 11, insertResult, 10);
    fprintf(stdout, "insert 3 at 0:\n");
    array->InsertElementAt(foo, 0);
    int32_t   insertResult2[12] = {3, 0, 1, 2, 3, 4, 3, 5, 6, 7, 8, 9};
    DumpArray(array, 12, insertResult2, 10);
    fprintf(stdout, "append 3:\n");
    array->AppendElement(foo);
    int32_t   appendResult[13] = {3, 0, 1, 2, 3, 4, 3, 5, 6, 7, 8, 9, 3};
    DumpArray(array, 13, appendResult, 10);


    // test IndexOf && LastIndexOf
    int32_t expectedIndex[5] = {0, 4, 6, 12, -1};
    int32_t count = 0;
    int32_t index = array->IndexOf(foo);
    fprintf(stdout, "IndexOf(foo): %d=%d %s\n", index, expectedIndex[count], 
            AssertEqual(index, expectedIndex[count]));
    while (-1 != index) {
      count++;
      index = array->IndexOfStartingAt(foo, index + 1);
      if (-1 != index)
        fprintf(stdout, "IndexOf(foo): %d=%d %s\n", index, expectedIndex[count], 
                AssertEqual(index, expectedIndex[count]));
    }
    index = array->LastIndexOf(foo);
    count--;
    fprintf(stdout, "LastIndexOf(foo): %d=%d %s\n", index, expectedIndex[count], 
            AssertEqual(index, expectedIndex[count]));

    // test ReplaceElementAt
    fprintf(stdout, "ReplaceElementAt(8):\n");
    array->ReplaceElementAt(foo, 8);
    int32_t   replaceResult[13] = {3, 0, 1, 2, 3, 4, 3, 5, 3, 7, 8, 9, 3};
    DumpArray(array, 13, replaceResult, 9);

    // test RemoveElementAt, RemoveElement RemoveLastElement
    fprintf(stdout, "RemoveElementAt(0):\n");
    array->RemoveElementAt(0);
    int32_t   removeResult[12] = {0, 1, 2, 3, 4, 3, 5, 3, 7, 8, 9, 3};
    DumpArray(array, 12, removeResult, 9);
    fprintf(stdout, "RemoveElementAt(7):\n");
    array->RemoveElementAt(7);
    int32_t   removeResult2[11] = {0, 1, 2, 3, 4, 3, 5, 7, 8, 9, 3};
    DumpArray(array, 11, removeResult2, 9);
    fprintf(stdout, "RemoveElement(foo):\n");
    array->RemoveElement(foo);
    int32_t   removeResult3[10] = {0, 1, 2, 4, 3, 5, 7, 8, 9, 3};
    DumpArray(array, 10, removeResult3, 9);
    fprintf(stdout, "RemoveLastElement(foo):\n");
    array->RemoveLastElement(foo);
    int32_t   removeResult4[9] = {0, 1, 2, 4, 3, 5, 7, 8, 9};
    DumpArray(array, 9, removeResult4, 9);

    // test clear
    fprintf(stdout, "clear array:\n");
    array->Clear();
    DumpArray(array, 0, 0, 0);
    fprintf(stdout, "add 4 new:\n");
    FillArray(array, 4);
    DumpArray(array, 4, fillResult, 4);

    // test compact
    fprintf(stdout, "compact array:\n");
    array->Compact();
    DumpArray(array, 4, fillResult, 4);

    // test delete
    fprintf(stdout, "release array:\n");
    NS_RELEASE(array);
  }
  else {
    fprintf(stdout, "error can't create array: %x\n", rv);
  }

  return 0;
}
