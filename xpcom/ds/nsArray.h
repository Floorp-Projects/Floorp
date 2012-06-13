/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsArray_h__
#define nsArray_h__

#include "nsIMutableArray.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "mozilla/Attributes.h"

#define NS_ARRAY_CLASSNAME \
  "nsIArray implementation"

// {35C66FD1-95E9-4e0a-80C5-C3BD2B375481}
#define NS_ARRAY_CID \
{ 0x35c66fd1, 0x95e9, 0x4e0a, \
  { 0x80, 0xc5, 0xc3, 0xbd, 0x2b, 0x37, 0x54, 0x81 } }


// adapter class to map nsIArray->nsCOMArray
// do NOT declare this as a stack or member variable, use
// nsCOMArray instead!
// if you need to convert a nsCOMArray->nsIArray, see NS_NewArray above
class nsArray : public nsIMutableArray
{
public:
    nsArray() { }
    nsArray(const nsCOMArray_base& aBaseArray) : mArray(aBaseArray)
    { }
    
    NS_DECL_ISUPPORTS
    NS_DECL_NSIARRAY
    NS_DECL_NSIMUTABLEARRAY

protected:
    virtual ~nsArray(); // nsArrayCC inherits from this

    nsCOMArray_base mArray;
};

class nsArrayCC MOZ_FINAL : public nsArray
{
public:
    nsArrayCC() : nsArray() { }
    nsArrayCC(const nsCOMArray_base& aBaseArray) : nsArray(aBaseArray)
    { }
    
    NS_DECL_CYCLE_COLLECTING_ISUPPORTS
    NS_DECL_CYCLE_COLLECTION_CLASS(nsArrayCC)
};

nsresult nsArrayConstructor(nsISupports *aOuter, const nsIID& aIID, void **aResult);

#endif
