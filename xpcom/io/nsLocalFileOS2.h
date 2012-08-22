/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 05/26/2000       IBM Corp.      Make more like Windows.
 */

#ifndef _nsLocalFileOS2_H_
#define _nsLocalFileOS2_H_

#include "nscore.h"
#include "nsError.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIFile.h"
#include "nsIFactory.h"
#include "nsILocalFileOS2.h"
#include "nsIHashable.h"
#include "nsIClassInfoImpl.h"

#define INCL_DOS
#define INCL_DOSERRORS
#define INCL_WINCOUNTRY
#define INCL_WINWORKPLACE

#include <os2.h>

class TypeEaEnumerator;

class nsLocalFile : public nsILocalFileOS2,
                    public nsIHashable
{
public:
    NS_DEFINE_STATIC_CID_ACCESSOR(NS_LOCAL_FILE_CID)

    nsLocalFile();

    static nsresult nsLocalFileConstructor(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    // nsISupports interface
    NS_DECL_ISUPPORTS

    // nsIFile interface
    NS_DECL_NSIFILE

    // nsILocalFile interface
    NS_DECL_NSILOCALFILE

    // nsILocalFileOS2 interface
    NS_DECL_NSILOCALFILEOS2

    // nsIHashable interface
    NS_DECL_NSIHASHABLE

public:
    static void GlobalInit();
    static void GlobalShutdown();

private:
    nsLocalFile(const nsLocalFile& other);
    ~nsLocalFile() {}

    // cached information can only be used when this is false
    bool mDirty;

    // this string will always be in native format!
    nsCString mWorkingPath;

    PRFileInfo64  mFileInfo64;

    void MakeDirty() { mDirty = true; }

    nsresult Stat();

    nsresult CopyMove(nsIFile *newParentDir, const nsACString &newName, bool move);
    nsresult CopySingleFile(nsIFile *source, nsIFile* dest, const nsACString &newName, bool move);

    nsresult SetModDate(int64_t aLastModifiedTime);
    nsresult AppendNativeInternal(const nsAFlatCString &node, bool multipleComponents);

    nsresult GetEA(const char *eaName, PFEA2LIST pfea2list);
    friend class TypeEaEnumerator;
};

#endif
