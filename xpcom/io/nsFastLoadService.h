/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla FastLoad code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Brendan Eich <brendan@mozilla.org> (original author)
 */
#include "prtypes.h"
#include "pldhash.h"
#include "nsCOMPtr.h"
#include "nsHashtable.h"
#include "nsIFastLoadService.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"

class nsFastLoadFileReader;
class nsFastLoadFileWriter;

class NS_COM nsFastLoadService : public nsIFastLoadService
{
  public:
    nsFastLoadService();
    virtual ~nsFastLoadService();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIFASTLOADSERVICE

    static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  private:
    PRLock*                         mLock;
    PLDHashTable*                   mFastLoadPtrMap;
    nsCOMPtr<nsIObjectInputStream>  mInputStream;
    nsCOMPtr<nsIObjectOutputStream> mOutputStream;
    nsCOMPtr<nsIFastLoadFileIO>     mFileIO;
    PRInt32                         mDirection;
    nsHashtable                     mChecksumTable;
};
