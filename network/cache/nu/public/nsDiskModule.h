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

#ifndef _nsDiskModule_H_
#define _nsDiskModule_H_
/* 
* nsDiskModule
*
* Gagan Saksena 02/03/98
* 
*/
#include "nsCacheModule.h"
#include "nsCachePref.h"
#include "mcom_db.h"

class nsDiskModule : public nsCacheModule
{

public:
    nsDiskModule(const PRUint32 size = nsCachePref::DiskCacheSize());
    ~nsDiskModule();

    // Cache module interface
    PRBool          AddObject(nsCacheObject* io_pObject);
    
    PRBool          Contains(nsCacheObject* io_pObject) const;
    PRBool          Contains(const char* i_url) const;
    
    void            GarbageCollect(void);

    nsCacheObject*  GetObject(const PRUint32 i_index) const;
    nsCacheObject*  GetObject(const char* i_url) const;

    PRBool          Remove(const char* i_url);
    PRBool          Remove(const PRUint32 i_index);
    
    PRBool          Revalidate(void);

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

#endif
