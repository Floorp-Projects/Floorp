/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set et ts=4 sw=4 sts=4 cin: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
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
 * Bradley Baetz
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Bradley Baetz <bbaetz@student.usyd.edu.au>
 *   Christian Biesinger <cbiesinger@web.de>
 * 
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsResumableEntityID.h"
#include "nsReadableUtils.h"

NS_IMPL_ISUPPORTS1(nsResumableEntityID, nsIResumableEntityID)
    
nsResumableEntityID::nsResumableEntityID() :
    mSize(LL_MaxUint()) {
}

nsResumableEntityID::~nsResumableEntityID() {}

NS_IMETHODIMP
nsResumableEntityID::GetSize(PRUint64 *aSize) {
    if (LL_EQ(mSize, LL_MaxUint()))
        return NS_ERROR_NOT_AVAILABLE;
    *aSize = mSize;
    return NS_OK;
}

NS_IMETHODIMP
nsResumableEntityID::SetSize(PRUint64 aSize) {
    mSize = aSize;
    return NS_OK;
}

NS_IMETHODIMP
nsResumableEntityID::GetLastModified(nsACString& aLastModified) {
    aLastModified = mLastModified;
    return NS_OK;
}

NS_IMETHODIMP
nsResumableEntityID::SetLastModified(const nsACString& aLastModified) {
    mLastModified = aLastModified;
    return NS_OK;
}

NS_IMETHODIMP
nsResumableEntityID::SetEntityTag(const nsACString& aTag) {
    mEntityTag = aTag;
    return NS_OK;
}

NS_IMETHODIMP
nsResumableEntityID::GetEntityTag(nsACString& aTag) {
    aTag = mEntityTag;
    return NS_OK;
}

NS_IMETHODIMP
nsResumableEntityID::Equals(nsIResumableEntityID *other, PRBool *ret) {
    PRUint64 size;
    nsCAutoString lastMod;
    nsCAutoString entityTag;

    nsresult rv = other->GetSize(&size);
    if (NS_FAILED(rv))
        size = LL_MaxUint();

    rv = other->GetLastModified(lastMod);
    if (NS_FAILED(rv))
        lastMod.Truncate();

    rv = other->GetEntityTag(entityTag);
    if (NS_FAILED(rv))
        entityTag.Truncate();

    // This assumes that the server generated the last modification time in
    // exactly the same way for both of these entity IDs (same timezone, same
    // format, etc).
    *ret = mEntityTag.Equals(entityTag) && lastMod.Equals(mLastModified) &&
           LL_EQ(mSize, size);

    return NS_OK;
}
