/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

/* nsCachePref. A class to separate the preference related code to 
 * one place. This is an incomplete implementation, I need to access
 * the libprefs directly, but that would add another dependency. And 
 * libpref comes with a JS dependency... so holding this off for the 
 * moment. 
 * 
 * -Gagan Saksena 09/15/98
 */
#ifndef nsCachePref_h__
#define nsCachePref_h__

#include "nsICachePref.h"
#include "nsIFactory.h"
#include "prtypes.h"
#include "prlog.h"

class nsCachePref : public nsICachePref
{

public:
    
    nsCachePref(void);
    virtual ~nsCachePref();

 /*
    NS_IMETHOD              QueryInterface(const nsIID& aIID, 
                                           void** aInstancePtr);
    NS_IMETHOD_(nsrefcnt)   AddRef(void);
    NS_IMETHOD_(nsrefcnt)   Release(void);
*/
    NS_DECL_ISUPPORTS
	/*
    enum Refresh 
    {
        ONCE,
        ALWAYS,
        NEVER
    };
	*/
    NS_IMETHOD	GetBkgSleepTime (PRUint32* o_time) ;
    
    NS_IMETHOD	GetDiskCacheDBFilename (char** o_name) ; /* like Fat.db */
    NS_IMETHOD	GetDiskCacheFolder (char** o_folder) ;   /* Cache dir */

    NS_IMETHOD	GetDiskCacheSSL (PRBool* o_bSet) ;
    NS_IMETHOD	SetDiskCacheSSL (PRBool i_bSet) ;

    NS_IMETHOD	GetDiskCacheSize (PRUint32* o_size) ;
    NS_IMETHOD	SetDiskCacheSize (const PRUint32 i_size) ;

    NS_IMETHOD	GetMemCacheSize (PRUint32 * o_size) ;
    NS_IMETHOD	SetMemCacheSize (const PRUint32 i_size) ;

    NS_IMETHOD	GetFrequency (nsICachePref::Refresh* o_frequency) ;

    /* Revalidating in background, makes IMS calls in the bkg thread to 
       update cache entries. TODO, this should be at a bigger time period
       than the cache cleanup routine */
    NS_IMETHOD	RevalidateInBkg (PRBool* i_bRevalidateInBkg) ;

    /* Setup all prefs */
    NS_IMETHOD	SetupPrefs(const char* i_Pref) ;

    static nsICachePref* GetInstance(void);

private:
    nsCachePref(const nsCachePref& o);
    nsCachePref& operator=(const nsCachePref& o);

    PRBool                  m_bRevalidateInBkg;
    PRBool                  m_bDiskCacheSSL;
    nsICachePref::Refresh    m_RefreshFreq;
    PRUint32                m_MemCacheSize;
    PRUint32                m_DiskCacheSize;
    char*                   m_DiskCacheDBFilename;
    char*                   m_DiskCacheFolder;
    PRUint32                m_BkgSleepTime;
};

inline NS_IMETHODIMP
nsCachePref::GetDiskCacheSize(PRUint32* o_size)
{
	if (!o_size)
	  return NS_ERROR_NULL_POINTER ;

    *o_size = m_DiskCacheSize;
	return NS_OK ;
}

inline NS_IMETHODIMP
nsCachePref::SetDiskCacheSize(const PRUint32 i_Size)
{
    m_DiskCacheSize = i_Size;
	return NS_OK ;
}

inline NS_IMETHODIMP
nsCachePref::GetDiskCacheSSL(PRBool* o_bSet)
{
	if (!o_bSet)
	  return NS_ERROR_NULL_POINTER ;

    *o_bSet = m_bDiskCacheSSL;
	return NS_OK ;
}

inline NS_IMETHODIMP
nsCachePref::SetDiskCacheSSL(PRBool bSet)
{
    m_bDiskCacheSSL = bSet;
	return NS_OK ;
}

inline NS_IMETHODIMP
nsCachePref::GetDiskCacheFolder(char** o_folder)
{
    if (!o_folder)
	  return NS_ERROR_NULL_POINTER ;

    PR_ASSERT(m_DiskCacheFolder);
	*o_folder = m_DiskCacheFolder ;
    return NS_OK ;
}

inline NS_IMETHODIMP
nsCachePref::GetMemCacheSize(PRUint32 * o_size)
{
	if (!o_size)
	  return NS_ERROR_NULL_POINTER ;

    *o_size = m_MemCacheSize;
	return NS_OK ;
}

inline NS_IMETHODIMP
nsCachePref::SetMemCacheSize(const PRUint32 i_Size)
{
    m_MemCacheSize = i_Size;
	return NS_OK ;
}

inline NS_IMETHODIMP
nsCachePref::RevalidateInBkg(PRBool* o_RevalidateInBkg)
{
	if (!o_RevalidateInBkg) 
	  return NS_ERROR_NULL_POINTER ;

    *o_RevalidateInBkg = m_bRevalidateInBkg;
	return NS_OK ;
}

inline NS_IMETHODIMP
nsCachePref::GetFrequency (nsICachePref::Refresh* o_frequency)
{
  if (!o_frequency)
	return NS_ERROR_NULL_POINTER ;

  *o_frequency = m_RefreshFreq ;
  return NS_OK ;
}

/////////////////////////////////////////////////////////////////////
// nsCacheObjectFactory

class nsCachePrefFactory : public nsIFactory {
    public:
	  nsCachePrefFactory();
	  virtual ~nsCachePrefFactory();

	  NS_DECL_ISUPPORTS
	  NS_IMETHOD CreateInstance(nsISupports *aOuter,
	                            const nsIID &aIID,
                                void **aResult);
      NS_IMETHOD LockFactory(PRBool aLock);
} ;

#endif // nsCachePref_h__
