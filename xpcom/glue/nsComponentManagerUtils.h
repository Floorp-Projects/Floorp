/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
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

#ifndef nsComponentManagerUtils_h__
#define nsComponentManagerUtils_h__

/*
 * Do not include this file directly.  Instead,
 * |#include "nsIComponentManager.h"|.
 */

#ifndef nsCOMPtr_h__
#include "nsCOMPtr.h"
#endif

#ifndef nsComponentManagerObsolete_h___
#include "nsComponentManagerObsolete.h"
#endif

#define NS_COMPONENTMANAGER_CID                      \
{ /* 91775d60-d5dc-11d2-92fb-00e09805570f */         \
    0x91775d60,                                      \
    0xd5dc,                                          \
    0x11d2,                                          \
    {0x92, 0xfb, 0x00, 0xe0, 0x98, 0x05, 0x57, 0x0f} \
}

class NS_COM nsCreateInstanceByCID : public nsCOMPtr_helper
{
public:
    nsCreateInstanceByCID( const nsCID& aCID, nsISupports* aOuter, nsresult* aErrorPtr )
        : mCID(aCID),
          mOuter(aOuter),
          mErrorPtr(aErrorPtr)
    {
        // nothing else to do here
    }
    
    virtual nsresult operator()( const nsIID&, void** ) const;
    
private:
    const nsCID&	mCID;
    nsISupports*	mOuter;
    nsresult*		mErrorPtr;
};

class NS_COM nsCreateInstanceByContractID : public nsCOMPtr_helper
{
public:
    nsCreateInstanceByContractID( const char* aContractID, nsISupports* aOuter, nsresult* aErrorPtr )
        : mContractID(aContractID),
          mOuter(aOuter),
          mErrorPtr(aErrorPtr)
				{
					// nothing else to do here
				}
    
    virtual nsresult operator()( const nsIID&, void** ) const;
    
private:
    const char*	  mContractID;
    nsISupports*  mOuter;
    nsresult*	  mErrorPtr;
};

inline
const nsCreateInstanceByCID
do_CreateInstance( const nsCID& aCID, nsresult* error = 0 )
{
    return nsCreateInstanceByCID(aCID, 0, error);
}

inline
const nsCreateInstanceByCID
do_CreateInstance( const nsCID& aCID, nsISupports* aOuter, nsresult* error = 0 )
{
    return nsCreateInstanceByCID(aCID, aOuter, error);
}

inline
const nsCreateInstanceByContractID
do_CreateInstance( const char* aContractID, nsresult* error = 0 )
{
    return nsCreateInstanceByContractID(aContractID, 0, error);
}

inline
const nsCreateInstanceByContractID
do_CreateInstance( const char* aContractID, nsISupports* aOuter, nsresult* error = 0 )
{
    return nsCreateInstanceByContractID(aContractID, aOuter, error);
}

// type-safe shortcuts for calling |CreateInstance|
template <class DestinationType>
inline
nsresult
CallCreateInstance( const nsCID &aClass,
                    nsISupports *aDelegate,
                    DestinationType** aDestination )
{
    NS_PRECONDITION(aDestination, "null parameter");
    
    return nsComponentManager::CreateInstance(aClass, aDelegate,
                                              NS_GET_IID(DestinationType),
                                              NS_REINTERPRET_CAST(void**, aDestination));
}

template <class DestinationType>
inline
nsresult
CallCreateInstance( const nsCID &aClass,
                    DestinationType** aDestination )
{
    NS_PRECONDITION(aDestination, "null parameter");
    
    return nsComponentManager::CreateInstance(aClass, nsnull,
                                              NS_GET_IID(DestinationType),
                                              NS_REINTERPRET_CAST(void**, aDestination));
}

template <class DestinationType>
inline
nsresult
CallCreateInstance( const char *aContractID,
                    nsISupports *aDelegate,
                    DestinationType** aDestination )
{
    NS_PRECONDITION(aContractID, "null parameter");
    NS_PRECONDITION(aDestination, "null parameter");
    
    return nsComponentManager::CreateInstance(aContractID, 
                                              aDelegate,
                                              NS_GET_IID(DestinationType),
                                              NS_REINTERPRET_CAST(void**, aDestination));
}

template <class DestinationType>
inline
nsresult
CallCreateInstance( const char *aContractID,
                    DestinationType** aDestination )
{
    NS_PRECONDITION(aContractID, "null parameter");
    NS_PRECONDITION(aDestination, "null parameter");
    
    return nsComponentManager::CreateInstance(aContractID, nsnull,
                                              NS_GET_IID(DestinationType),
                                              NS_REINTERPRET_CAST(void**, aDestination));
}

/* keys for registry use */
extern const char xpcomKeyName[];
extern const char xpcomComponentsKeyName[];
extern const char lastModValueName[];
extern const char fileSizeValueName[];
extern const char nativeComponentType[];
extern const char staticComponentType[];

#endif /* nsComponentManagerUtils_h__ */


