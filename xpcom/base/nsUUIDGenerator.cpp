/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * mozilla.org
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#if defined(XP_WIN)
#include <windows.h>
#include <objbase.h>
#elif defined(XP_MACOSX)
#include <CoreFoundation/CoreFoundation.h>
#else
#include <stdlib.h>
#include "prrng.h"
#endif

#include "nsMemory.h"

#include "nsAutoLock.h"

#include "nsUUIDGenerator.h"

NS_IMPL_THREADSAFE_ISUPPORTS1(nsUUIDGenerator, nsIUUIDGenerator)

nsUUIDGenerator::nsUUIDGenerator()
    : mLock(nsnull)
{
}

nsUUIDGenerator::~nsUUIDGenerator()
{
    if (mLock) {
        PR_DestroyLock(mLock);
    }
}

nsresult
nsUUIDGenerator::Init()
{
    mLock = PR_NewLock();

    NS_ENSURE_TRUE(mLock, NS_ERROR_OUT_OF_MEMORY);

    // We're a service, so we're guaranteed that Init() is not going
    // to be reentered while we're inside Init().
    
#if !defined(XP_WIN) && !defined(XP_MACOSX)
    /* initialize random number generator using NSPR random noise */
    unsigned int seed;

    PRSize bytes = 0;
    while (bytes < sizeof(seed)) {
        PRSize nbytes = PR_GetRandomNoise(((unsigned char *)&seed)+bytes,
                                          sizeof(seed)-bytes);
        if (nbytes == 0) {
            return NS_ERROR_FAILURE;
        }
        bytes += nbytes;
    }

    initstate(seed, mState, sizeof(mState));

    mRBytes = 4;
#ifdef RAND_MAX
    if ((unsigned long) RAND_MAX < (unsigned long)0xffffffff)
        mRBytes = 3;
    if ((unsigned long) RAND_MAX < (unsigned long)0x00ffffff)
        mRBytes = 2;
    if ((unsigned long) RAND_MAX < (unsigned long)0x0000ffff)
        mRBytes = 1;
    if ((unsigned long) RAND_MAX < (unsigned long)0x000000ff)
        return NS_ERROR_FAILURE;
#endif

#endif /* non XP_WIN and non XP_MACOSX */

    return NS_OK;
}

NS_IMETHODIMP
nsUUIDGenerator::GenerateUUID(nsID** ret)
{
    nsID *id = NS_STATIC_CAST(nsID*, NS_Alloc(sizeof(nsID)));
    if (id == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = GenerateUUIDInPlace(id);
    if (NS_FAILED(rv)) {
        NS_Free(id);
        return rv;
    }

    *ret = id;
    return rv;
}

NS_IMETHODIMP
nsUUIDGenerator::GenerateUUIDInPlace(nsID* id)
{
    // The various code in this method is probably not threadsafe, so lock
    // across the whole method.
    nsAutoLock lock(mLock);
    
#if defined(XP_WIN)
    HRESULT hr = CoCreateGuid((GUID*)id);
    if (NS_FAILED(hr))
        return NS_ERROR_FAILURE;
#elif defined(XP_MACOSX)
    CFUUIDRef uuid = CFUUIDCreate(kCFAllocatorDefault);
    if (!uuid)
        return NS_ERROR_FAILURE;

    CFUUIDBytes bytes = CFUUIDGetUUIDBytes(uuid);
    memcpy(id, &bytes, sizeof(nsID));

    CFRelease(uuid);
#else /* not windows or OS X; generate randomness using random(). */
    PRSize bytesLeft = sizeof(nsID);
    while (bytesLeft > 0) {
        long rval = random();

        PRUint8 *src = (PRUint8*)&rval;
        // We want to grab the mRBytes least significant bytes of rval, since
        // mRBytes less than sizeof(rval) means the high bytes are 0.
#ifdef IS_BIG_ENDIAN
        src += sizeof(rval) - mRBytes;
#endif
        PRUint8 *dst = ((PRUint8*) id) + (sizeof(nsID) - bytesLeft);
        PRSize toWrite = (bytesLeft < mRBytes ? bytesLeft : mRBytes);
        for (PRSize i = 0; i < toWrite; i++)
            dst[i] = src[i];

        bytesLeft -= toWrite;
    }

    /* Put in the version */
    id->m2 &= 0x0fff;
    id->m2 |= 0x4000;

    /* Put in the variant */
    id->m3[0] &= 0x3f;
    id->m3[0] |= 0x80;
#endif

    return NS_OK;
}
