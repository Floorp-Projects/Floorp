/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsCategoryManagerUtils_h__
#define nsCategoryManagerUtils_h__

#include "nsICategoryManager.h"
#include "nsCOMPtr.h"

NS_COM nsresult
NS_CreateServicesFromCategory(const char *category,
                              nsISupports *origin,
                              const char *observerTopic);

class NS_COM nsCreateInstanceFromCategory : public nsCOMPtr_helper
{
public:
    nsCreateInstanceFromCategory(const char *aCategory, const char *aEntry,
                                 nsISupports *aOuter, nsresult *aErrorPtr)
        : mCategory(aCategory),
          mEntry(aEntry),
          mErrorPtr(aErrorPtr)
    {
        // nothing else to do;
    }
    virtual nsresult NS_FASTCALL operator()( const nsIID& aIID, void** aInstancePtr) const;

private:
    const char *mCategory;  // Do not free.  This char * is not owned.
    const char *mEntry;     // Do not free.  This char * is not owned.

    nsISupports *mOuter;
    nsresult *mErrorPtr;

};

inline
const nsCreateInstanceFromCategory
do_CreateInstanceFromCategory( const char *aCategory, const char *aEntry,
                               nsresult *aErrorPtr = 0)
{
    return nsCreateInstanceFromCategory(aCategory, aEntry, 0, aErrorPtr);
}

inline
const nsCreateInstanceFromCategory
do_CreateInstanceFromCategory( const char *aCategory, const char *aEntry,
                               nsISupports *aOuter, nsresult *aErrorPtr = 0)
{
    return nsCreateInstanceFromCategory(aCategory, aEntry, aOuter, aErrorPtr);
}


#endif
