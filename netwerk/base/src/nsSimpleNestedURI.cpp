/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * the Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Boris Zbarsky <bzbarsky@mit.edu> (Original author)
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

#include "IPCMessageUtils.h"
#include "mozilla/net/NeckoMessageUtils.h"

#include "nsSimpleNestedURI.h"
#include "nsIObjectInputStream.h"
#include "nsIObjectOutputStream.h"
#include "nsNetUtil.h"

NS_IMPL_ISUPPORTS_INHERITED1(nsSimpleNestedURI, nsSimpleURI, nsINestedURI)

nsSimpleNestedURI::nsSimpleNestedURI(nsIURI* innerURI)
    : mInnerURI(innerURI)
{
    NS_ASSERTION(innerURI, "Must have inner URI");
    NS_TryToSetImmutable(innerURI);
}
    
// nsISerializable

NS_IMETHODIMP
nsSimpleNestedURI::Read(nsIObjectInputStream* aStream)
{
    nsresult rv = nsSimpleURI::Read(aStream);
    if (NS_FAILED(rv)) return rv;

    NS_ASSERTION(!mMutable, "How did that happen?");

    rv = aStream->ReadObject(PR_TRUE, getter_AddRefs(mInnerURI));
    if (NS_FAILED(rv)) return rv;

    NS_TryToSetImmutable(mInnerURI);

    return rv;
}

NS_IMETHODIMP
nsSimpleNestedURI::Write(nsIObjectOutputStream* aStream)
{
    nsCOMPtr<nsISerializable> serializable = do_QueryInterface(mInnerURI);
    if (!serializable) {
        // We can't serialize ourselves
        return NS_ERROR_NOT_AVAILABLE;
    }

    nsresult rv = nsSimpleURI::Write(aStream);
    if (NS_FAILED(rv)) return rv;

    rv = aStream->WriteCompoundObject(mInnerURI, NS_GET_IID(nsIURI),
                                      PR_TRUE);
    return rv;
}

// nsIIPCSerializable

PRBool
nsSimpleNestedURI::Read(const IPC::Message *aMsg, void **aIter)
{
    if (!nsSimpleURI::Read(aMsg, aIter))
        return PR_FALSE;

    IPC::URI uri;
    if (!ReadParam(aMsg, aIter, &uri))
        return PR_FALSE;

    mInnerURI = uri;

    return PR_TRUE;
}

void
nsSimpleNestedURI::Write(IPC::Message *aMsg)
{
    nsSimpleURI::Write(aMsg);

    IPC::URI uri(mInnerURI);
    WriteParam(aMsg, uri);
}

// nsINestedURI

NS_IMETHODIMP
nsSimpleNestedURI::GetInnerURI(nsIURI** uri)
{
    NS_ENSURE_TRUE(mInnerURI, NS_ERROR_NOT_INITIALIZED);
    
    return NS_EnsureSafeToReturn(mInnerURI, uri);
}

NS_IMETHODIMP
nsSimpleNestedURI::GetInnermostURI(nsIURI** uri)
{
    return NS_ImplGetInnermostURI(this, uri);
}

// nsIURI overrides

NS_IMETHODIMP
nsSimpleNestedURI::Equals(nsIURI* other, PRBool *result)
{
    *result = PR_FALSE;
    NS_ENSURE_TRUE(mInnerURI, NS_ERROR_NOT_INITIALIZED);
    
    if (other) {
        PRBool correctScheme;
        nsresult rv = other->SchemeIs(mScheme.get(), &correctScheme);
        NS_ENSURE_SUCCESS(rv, rv);

        if (correctScheme) {
            nsCOMPtr<nsINestedURI> nest = do_QueryInterface(other);
            if (nest) {
                nsCOMPtr<nsIURI> otherInner;
                rv = nest->GetInnerURI(getter_AddRefs(otherInner));
                NS_ENSURE_SUCCESS(rv, rv);

                return otherInner->Equals(mInnerURI, result);
            }
        }
    }

    return NS_OK;
}

/* virtual */ nsSimpleURI*
nsSimpleNestedURI::StartClone()
{
    NS_ENSURE_TRUE(mInnerURI, nsnull);
    
    nsCOMPtr<nsIURI> innerClone;
    nsresult rv = mInnerURI->Clone(getter_AddRefs(innerClone));
    if (NS_FAILED(rv)) {
        return nsnull;
    }

    nsSimpleNestedURI* url = new nsSimpleNestedURI(innerClone);
    if (url) {
        url->SetMutable(PR_FALSE);
    }

    return url;
}

// nsIClassInfo overrides

NS_IMETHODIMP 
nsSimpleNestedURI::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
    static NS_DEFINE_CID(kSimpleNestedURICID, NS_SIMPLENESTEDURI_CID);

    *aClassIDNoAlloc = kSimpleNestedURICID;
    return NS_OK;
}
