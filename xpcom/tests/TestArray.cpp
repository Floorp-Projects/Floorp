/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include <stdio.h>
#include <stdlib.h>
#include "nsISupportsArray.h"

// {9e70a320-be02-11d1-8031-006008159b5a}
#define NS_IFOO_IID \
  {0x9e70a320, 0xbe02, 0x11d1,    \
    {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

static const PRBool kExitOnError = PR_TRUE;

static NS_DEFINE_IID(kIFooIID, NS_IFOO_IID);

class IFoo : public nsISupports {
public:
  IFoo(PRInt32 aID);
  ~IFoo();

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);                           
  NS_IMETHOD_(nsrefcnt) AddRef(void);                                       
  NS_IMETHOD_(nsrefcnt) Release(void);                                      

  PRInt32 mRefCnt;
  PRInt32 mID;
  static PRInt32 gCount;
};

PRInt32 IFoo::gCount;

IFoo::IFoo(PRInt32 aID)
{
  NS_INIT_REFCNT();
  mID = aID;
  gCount++;
  fprintf(stdout, "init: %d (%x), %d total)\n", mID, this, gCount);
}

IFoo::~IFoo()
{
  gCount--;
  fprintf(stdout, "destruct: %d (%x), %d remain)\n", mID, this, gCount);
}

NS_IMPL_ISUPPORTS(IFoo, NS_IFOO_IID);

const char* AssertEqual(PRInt32 aValue1, PRInt32 aValue2)
{
  if (aValue1 == aValue2) {
    return "OK";
  }
  if (kExitOnError) {
    exit(1);
  }
  return "ERROR";
}

void DumpArray(nsISupportsArray* aArray, PRInt32 aExpectedCount, PRInt32 aElementIDs[], PRInt32 aExpectedTotal)
{
  PRInt32 count = aArray->Count();
  PRInt32 index;

  fprintf(stdout, "object count %d = %d %s\n", IFoo::gCount, aExpectedTotal, 
          AssertEqual(IFoo::gCount, aExpectedTotal));
  fprintf(stdout, "array count %d = %d %s\n", count, aExpectedCount,
          AssertEqual(count, aExpectedCount));
  
  for (index = 0; (index < count) && (index < aExpectedCount); index++) {
    IFoo* foo = (IFoo*)(aArray->ElementAt(index));
    fprintf(stdout, "%2d: %d=%d (%x) c: %d %s\n", 
            index, aElementIDs[index], foo->mID, foo, foo->mRefCnt - 1,
            AssertEqual(foo->mID, aElementIDs[index]));
    foo->Release();
  }
}

void FillArray(nsISupportsArray* aArray, PRInt32 aCount)
{
  PRInt32 index;
  for (index = 0; index < aCount; index++) {
    IFoo* foo = new IFoo(index);
    foo->AddRef();
    aArray->AppendElement(foo);
    foo->Release();
  }
}

int main(int argc, char *argv[])
{
  nsISupportsArray* array;
  nsresult  rv;
  
  if (NS_OK == (rv = NS_NewISupportsArray(&array))) {
    FillArray(array, 10);
    fprintf(stdout, "Array created:\n");
    PRInt32   fillResult[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    DumpArray(array, 10, fillResult, 10);

    // test insert
    IFoo* foo = (IFoo*)array->ElementAt(3);
    foo->Release();  // pre-release to fix ref count for dumps
    array->InsertElementAt(foo, 5);
    fprintf(stdout, "insert 3 at 5:\n");
    PRInt32   insertResult[11] = {0, 1, 2, 3, 4, 3, 5, 6, 7, 8, 9};
    DumpArray(array, 11, insertResult, 10);
    fprintf(stdout, "insert 3 at 0:\n");
    array->InsertElementAt(foo, 0);
    PRInt32   insertResult2[12] = {3, 0, 1, 2, 3, 4, 3, 5, 6, 7, 8, 9};
    DumpArray(array, 12, insertResult2, 10);
    fprintf(stdout, "append 3:\n");
    array->AppendElement(foo);
    PRInt32   appendResult[13] = {3, 0, 1, 2, 3, 4, 3, 5, 6, 7, 8, 9, 3};
    DumpArray(array, 13, appendResult, 10);


    // test IndexOf && LastIndexOf
    PRInt32 expectedIndex[5] = {0, 4, 6, 12, -1};
    PRInt32 count = 0;
    PRInt32 index = array->IndexOf(foo);
    fprintf(stdout, "IndexOf(foo): %d=%d %s\n", index, expectedIndex[count], 
            AssertEqual(index, expectedIndex[count]));
    while (-1 != index) {
      count++;
      index = array->IndexOf(foo, index + 1);
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
    PRInt32   replaceResult[13] = {3, 0, 1, 2, 3, 4, 3, 5, 3, 7, 8, 9, 3};
    DumpArray(array, 13, replaceResult, 9);

    // test RemoveElementAt, RemoveElement RemoveLastElement
    fprintf(stdout, "RemoveElementAt(0):\n");
    array->RemoveElementAt(0);
    PRInt32   removeResult[12] = {0, 1, 2, 3, 4, 3, 5, 3, 7, 8, 9, 3};
    DumpArray(array, 12, removeResult, 9);
    fprintf(stdout, "RemoveElementAt(7):\n");
    array->RemoveElementAt(7);
    PRInt32   removeResult2[11] = {0, 1, 2, 3, 4, 3, 5, 7, 8, 9, 3};
    DumpArray(array, 11, removeResult2, 9);
    fprintf(stdout, "RemoveElement(foo):\n");
    array->RemoveElement(foo);
    PRInt32   removeResult3[10] = {0, 1, 2, 4, 3, 5, 7, 8, 9, 3};
    DumpArray(array, 10, removeResult3, 9);
    fprintf(stdout, "RemoveLastElement(foo):\n");
    array->RemoveLastElement(foo);
    PRInt32   removeResult4[9] = {0, 1, 2, 4, 3, 5, 7, 8, 9};
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
