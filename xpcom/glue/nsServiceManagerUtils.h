/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is XPCOM
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsIServiceManagerUtils_h__
#define nsIServiceManagerUtils_h__

#include "nsIServiceManager.h"
#include "nsIServiceManagerObsolete.h"
#include "nsCOMPtr.h"

////////////////////////////////////////////////////////////////////////////
// Using servicemanager with COMPtrs
class NS_COM nsGetServiceByCID : public nsCOMPtr_helper
{
 public:
    nsGetServiceByCID( const nsCID& aCID, nsISupports* aServiceManager, nsresult* aErrorPtr )
        : mCID(aCID),
        mServiceManager( do_QueryInterface(aServiceManager) ),
        mErrorPtr(aErrorPtr)
        {
            // nothing else to do
        }
    
    virtual nsresult operator()( const nsIID&, void** ) const;
    virtual ~nsGetServiceByCID() {};
    
 private:
    const nsCID&                mCID;
    nsCOMPtr<nsIServiceManager> mServiceManager;
    nsresult*                   mErrorPtr;
};

inline
const nsGetServiceByCID
do_GetService( const nsCID& aCID, nsresult* error = 0 )
{
    return nsGetServiceByCID(aCID, 0, error);
}

inline
const nsGetServiceByCID
do_GetService( const nsCID& aCID, nsISupports* aServiceManager, nsresult* error = 0 )
{
    return nsGetServiceByCID(aCID, aServiceManager, error);
}

class NS_COM nsGetServiceByContractID : public nsCOMPtr_helper
{
 public:
    nsGetServiceByContractID( const char* aContractID, nsISupports* aServiceManager, nsresult* aErrorPtr )
        : mContractID(aContractID),
        mServiceManager( do_QueryInterface(aServiceManager) ),
        mErrorPtr(aErrorPtr)
        {
            // nothing else to do
        }
    
    virtual nsresult operator()( const nsIID&, void** ) const;
    
    virtual ~nsGetServiceByContractID() {};
    
 private:
    const char*                 mContractID;
    nsCOMPtr<nsIServiceManager> mServiceManager;
    nsresult*                   mErrorPtr;
};

inline
const nsGetServiceByContractID
do_GetService( const char* aContractID, nsresult* error = 0 )
{
    return nsGetServiceByContractID(aContractID, 0, error);
}

inline
const nsGetServiceByContractID
do_GetService( const char* aContractID, nsISupports* aServiceManager, nsresult* error = 0 )
{
    return nsGetServiceByContractID(aContractID, aServiceManager, error);
}

class NS_COM nsGetServiceFromCategory : public nsCOMPtr_helper
{
 public:
    nsGetServiceFromCategory(const char* aCategory, const char* aEntry,
                             nsISupports* aServiceManager, 
                             nsresult* aErrorPtr)
        : mCategory(aCategory),
        mEntry(aEntry),
        mServiceManager( do_QueryInterface(aServiceManager) ),
        mErrorPtr(aErrorPtr)
        {
            // nothing else to do
        }
    
    virtual nsresult operator()( const nsIID&, void** ) const;
    virtual ~nsGetServiceFromCategory() {};
 protected:
    const char*                 mCategory;
    const char*                 mEntry;
    nsCOMPtr<nsIServiceManager> mServiceManager;
    nsresult*                   mErrorPtr;
};

inline
const nsGetServiceFromCategory
do_GetServiceFromCategory( const char* category, const char* entry,
                           nsresult* error = 0)
{
    return nsGetServiceFromCategory(category, entry, 0, error);
}

// type-safe shortcuts for calling |GetService|
template <class DestinationType>
inline
nsresult
CallGetService( const nsCID &aClass,
                DestinationType** aDestination)
{
    NS_PRECONDITION(aDestination, "null parameter");
    
    nsCOMPtr<nsIServiceManager> mgr;
    nsresult rv = NS_GetServiceManager(getter_AddRefs(mgr));
    
    if (NS_FAILED(rv))
        return rv;

    return mgr->GetService(aClass,
                           NS_GET_IID(DestinationType), 
                           NS_REINTERPRET_CAST(void**, aDestination));
}

template <class DestinationType>
inline
nsresult
CallGetService( const char *aContractID,
                DestinationType** aDestination)
{
    NS_PRECONDITION(aContractID, "null parameter");
    NS_PRECONDITION(aDestination, "null parameter");
    
    nsCOMPtr<nsIServiceManager> mgr;
    nsresult rv = NS_GetServiceManager(getter_AddRefs(mgr));
    
    if (NS_FAILED(rv))
        return rv;

    return mgr->GetServiceByContractID(aContractID,
                                       NS_GET_IID(DestinationType), 
                                       NS_REINTERPRET_CAST(void**, aDestination));
}

#endif
