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

/* Design and original implementation by Gagan Saksena '98 
 * Ideally we should be using the C++ api directly, but since the core of
 * Netlib is still in C, this file uses the stub functions that call the C++
 * API internally, allowing C programs to use the new cache architecture.
 * If you are accessing the cache, see if you can directly use the C++ api. 
 * -Gagan Saksena 09/15/98
 */

#ifndef CacheStubs_h__
#define CacheStubs_h__

#include "prtypes.h"
#include "prinrval.h"

#define MEM_MODULE_ID 0
#define DISK_MODULE_ID 1

PR_BEGIN_EXTERN_C
 
    /* General mapped functions which resolve to a bunch of internal calls. */
    extern void             Cache_Init(void);
    extern void             Cache_Shutdown(void);

    /* Cache Manager stub functions
       Does not include some functions not required in the current 
       design like AddModule etc... If you need to these functions, try
       accessing them directly thru the C++ api or let me know. 
       Check nsCacheManager.h for details on these functions.*/
    extern PRBool           CacheManager_Contains(const char* i_url);
    extern PRInt16          CacheManager_Entries(void);
    extern void*            CacheManager_GetObject(const char* i_url);
    extern void             CacheManager_InfoAsHTML(char* o_Buffer);
    extern PRBool           CacheManager_IsOffline(void);
    extern void             CacheManager_Offline(PRBool bSet);
    extern PRBool           CacheManager_Remove(const char* i_url);
    extern PRUint32         CacheManager_WorstCaseTime(void);

    /* Cache Object- check nsCacheObject.h for details on these functions */
    extern void*            CacheObject_Create(const char* i_url);
    extern void             CacheObject_Destroy(void* pThis);
    extern const char*      CacheObject_GetAddress(const void* pThis);
    extern const char*      CacheObject_GetCharset(const void* pThis);
    extern const char*      CacheObject_GetContentEncoding(const void* pThis);
    extern PRUint32         CacheObject_GetContentLength(const void* pThis);
    extern const char*      CacheObject_GetContentType(const void* pThis);
    extern const char*      CacheObject_GetEtag(const void* pThis);
    extern PRIntervalTime   CacheObject_GetExpires(const void* pThis);
    extern const char*      CacheObject_GetFilename(const void* pThis);
    extern PRBool           CacheObject_GetIsCompleted(const void* pThis);
    extern PRIntervalTime   CacheObject_GetLastAccessed(const void* pThis);
    extern PRIntervalTime   CacheObject_GetLastModified(const void* pThis);
    extern PRInt16          CacheObject_GetModule(const void* pThis);
    extern const char*      CacheObject_GetPageServicesURL(const void* pThis);
    extern const char*      CacheObject_GetPostData(const void* pThis);
    extern PRUint32         CacheObject_GetPostDataLen(const void* pThis);
    extern PRUint32         CacheObject_GetSize(const void* pThis);
    extern PRUint32         CacheObject_GetState(const void* pThis);
    extern PRUint32         CacheObject_Hits(const void* pThis);
    extern PRBool           CacheObject_IsExpired(const void* pThis);
    extern PRBool           CacheObject_IsPartial(const void* pThis);
    extern PRUint32         CacheObject_Read(const void* pThis, char* o_Destination, PRUint32 i_Len);
    extern void             CacheObject_Reset(void* pThis);
    extern void             CacheObject_SetAddress(void* pThis, const char* i_Address);
    extern void             CacheObject_SetCharset(void* pThis, const char* i_CharSet);
    extern void             CacheObject_SetContentEncoding(void* pThis, const char* i_Encoding);
    extern void             CacheObject_SetContentLength(void* pThis, PRUint32 i_Len);
    extern void             CacheObject_SetContentType(void* pThis, const char* i_Type);
    extern void             CacheObject_SetEtag(void* pThis, const char* i_Etag);
    extern void             CacheObject_SetExpires(void *pThis, const PRIntervalTime i_Time);
    extern void             CacheObject_SetFilename(void *pThis, const char* i_Filename);
    extern void             CacheObject_SetIsCompleted(void *pThis, PRBool bComplete);
    extern void             CacheObject_SetLastModified(void* pThis, const PRIntervalTime i_Time);
    extern void             CacheObject_SetModule(void* pThis, const PRInt16 i_Module);
    extern void             CacheObject_SetPageServicesURL(void* pThis, const char* i_Url);
    extern void             CacheObject_SetPostData(void* pThis, const char* i_PostData, const PRUint32 i_Len);
    extern void             CacheObject_SetSize(void* pThis, const PRUint32 i_Size);
    extern void             CacheObject_SetState(void* pThis, const PRUint32 i_State);
    extern PRBool           CacheObject_Synch(void* pThis);
    extern PRUint32         CacheObject_Write(void* pThis, const char* i_buffer, const PRUint32 i_length);

    /* Cache Prefs- check nsCachePref.h for details on these functions */
    extern PRUint32         CachePref_GetDiskCacheSize(void);
    extern PRBool           CachePref_GetDiskCacheSSL(void);
    extern PRUint32         CachePref_GetMemCacheSize(void);
    extern void             CachePref_SetDiskCacheSize(const PRUint32 i_Size);
    extern void             CachePref_SetDiskCacheSSL(PRBool bSet);
    extern void             CachePref_SetMemCacheSize(const PRUint32 i_Size);
    
    /* Cache Trace- Check nsCacheTrace.h for details on these functions */
    extern void             CacheTrace_Enable(PRBool bEnable);
    extern PRBool           CacheTrace_IsEnabled(void);
    
    /* Disk Module- Check nsDiskModule.h for details on these functions */
    extern PRBool           DiskModule_AddObject(void* pCacheObject);
    extern PRBool           DiskModule_Contains(const char* i_url);
    extern PRUint32         DiskModule_Entries(void);
    extern PRUint32         DiskModule_GetSize(void); /* Should be the same as CachePref_DiskCacheSize */
    extern PRUint32         DiskModule_GetSizeInUse(void);
    extern PRBool           DiskModule_IsEnabled(void);
    extern PRBool           DiskModule_Remove(const char* i_url);
    extern PRBool           DiskModule_RemoveAll(void);
    extern void             DiskModule_SetSize(PRUint32 i_Size);

    /* Mem Module- Check nsMemModule.h for details on these functions */
    extern PRBool           MemModule_AddObject(void* pCacheObject); 
    extern PRBool           MemModule_Contains(const char* i_url);
    extern PRUint32         MemModule_Entries(void);
    extern PRUint32         MemModule_GetSize(void); /* Should be the same as CachePref_MemCacheSize */
    extern PRUint32         MemModule_GetSizeInUse(void);
    extern PRBool           MemModule_IsEnabled(void);
    extern PRBool           MemModule_Remove(const char* i_url);
    extern PRBool           MemModule_RemoveAll(void);
    extern void             MemModule_SetSize(PRUint32 i_Size);

PR_END_EXTERN_C     

#endif // CacheStubs_h__

