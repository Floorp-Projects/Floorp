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

/* 
 * nsDiskModule. The disk cache module that stores the cache objects
 * on the disk. 
 *
 * Gagan Saksena 02/03/98
 * 
 */
#ifndef _nsDiskModule_H_
#define _nsDiskModule_H_

#include "nsCacheModule.h"
#include "nsCachePref.h"
#include "mcom_db.h"

class nsDiskModule : public nsCacheModule
{

public:
    nsDiskModule(const PRUint32 size = nsCachePref::GetInstance()->DiskCacheSize());
    ~nsDiskModule();

    // Cache module interface
    PRBool          AddObject(nsCacheObject* io_pObject);
 
    PRUint32        AverageSize(void) const;

    PRBool          Contains(nsCacheObject* io_pObject) const;
    PRBool          Contains(const char* i_url) const;
    
    void            GarbageCollect(void);

    nsCacheObject*  GetObject(const PRUint32 i_index) const;
    nsCacheObject*  GetObject(const char* i_url) const;

    nsStream*       GetStreamFor(const nsCacheObject* i_pObject);

    PRBool          Remove(const char* i_url);
    PRBool          Remove(nsCacheObject* pObject);
    PRBool          Remove(const PRUint32 i_index);

    // To do cleanup set size to zero. Else initialize disk cache
    void            SetSize(const PRUint32 i_size);

    PRBool          Revalidate(void);

protected:
    
    PRBool          ReduceSizeTo(const PRUint32 i_NewSize);

private:
    enum sync_frequency
    {
        EVERYTIME,
        IDLE,
        NEVER
    } m_Sync;

    PRBool          InitDB(void);

    nsDiskModule(const nsDiskModule& dm);
    nsDiskModule& operator=(const nsDiskModule& dm);

    DB* m_pDB;
};

inline
PRUint32 nsDiskModule::AverageSize(void) const
{
    MonitorLocker ml((nsDiskModule*)this);
    if (Entries()>0)
    {
        return (PRUint32)(m_SizeInUse/m_Entries);
    }
    return 0;
}
#endif
