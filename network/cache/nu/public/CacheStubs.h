/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

/* Designed and original implementation by Gagan Saksena '98 */

/* Ideally we should be using the C++ api directly, but since the core of
   Netlib is still in C, this file uses the stub functions that call the C++
   API internally, allowing C programs to use the new cache architecture */

#ifndef CacheStubs_h__
#define CacheStubs_h__

#include <prtypes.h>

#ifdef __cplusplus
extern "C" {
#endif
 
    /* Cache Manager stub functions
       Does not include other functions not required in the current 
       design like AddModule etc... */
    extern PRBool       CacheManager_Contains(const char* i_url);
    extern PRUint32     CacheManager_Entries(void);
    extern PRBool       CacheManager_IsOffline(void);
    extern void         CacheManager_Offline(PRBool bSet);
    extern PRUint32     CacheManager_WorstCaseTime(void);

        /* Disk Module functions */
    /*
    extern PRBool       DiskModule_AddObject() */
    extern PRBool       DiskModule_Contains(const char* i_url);
    extern PRUint32     DiskModule_Entries(void);
    extern PRUint32     DiskModule_GetSize(void);
    extern PRBool       DiskModule_IsEnabled(void);
    extern PRBool       DiskModule_Remove(const char* i_url);
    extern void         DiskModule_SetSize(PRUint32 i_Size);

    /* Mem Module functions */
    /*
    extern PRBool       MemModule_AddObject() */
    extern PRBool       MemModule_Contains(const char* i_url);
    extern PRUint32     MemModule_Entries(void);
    extern PRUint32     MemModule_GetSize(void);
    extern PRBool       MemModule_IsEnabled(void);
    extern PRBool       MemModule_Remove(const char* i_url);
    extern void         MemModule_SetSize(PRUint32 i_Size);

#ifdef __cplusplus
}
#endif

     
#endif // CacheStubs_h__

