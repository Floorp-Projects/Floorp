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

/* Design and original implementation by Gagan Saksena '98 */

#include "CacheStubs.h"
#include "nsCacheManager.h"
#include "nsDiskModule.h"
#include "nsMemModule.h"
#include "nsCacheTrace.h"
#include "prlog.h"

#define CACHEMGR nsCacheManager::GetInstance()

void
Cache_Init(void)
{
    CACHEMGR->Init();
}

void 
Cache_Shutdown(void)
{
    /* todo- Should the destructor for nsCacheManager object be forced here?*/
}

/* CacheManager functions */
PRBool
CacheManager_Contains(const char* i_url)
{
    return CACHEMGR->Contains(i_url);
}

PRInt16 
CacheManager_Entries()
{
    return CACHEMGR->Entries();
}

void*
CacheManager_GetObject(const char* i_url)
{
    return CACHEMGR->GetObj(i_url);
}

void
CacheManager_InfoAsHTML(char* o_Buffer)
{
    CACHEMGR->InfoAsHTML(o_Buffer);
}

PRBool
CacheManager_IsOffline(void)
{
    return CACHEMGR->IsOffline();
}

void
CacheManager_Offline(PRBool bSet)
{
    CACHEMGR->Offline(bSet);
}

PRBool
CacheManager_Remove(const char* i_url)
{
    return CACHEMGR->Remove(i_url);
}

PRUint32
CacheManager_WorstCaseTime(void)
{
    return CACHEMGR->WorstCaseTime();
}

/* CacheObject functions */
void*
CacheObject_Create(const char* i_url)
{
    return new nsCacheObject(i_url);
}

void
CacheObject_Destroy(void* pThis)
{
    if (pThis)
    {
        ((nsCacheObject*)pThis)->~nsCacheObject();
        pThis = 0;
    }
}

const char*
CacheObject_GetAddress(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->Address() : 0;
}

const char*
CacheObject_GetCharset(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->Charset() : 0;
}

const char*
CacheObject_GetContentEncoding(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->ContentEncoding() : 0;
}

PRUint32
CacheObject_GetContentLength(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->ContentLength() : 0;
}

const char*
CacheObject_GetContentType(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->ContentType() : 0;
}

const char*
CacheObject_GetEtag(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->Etag() : 0;
}

PRIntervalTime
CacheObject_GetExpires(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->Expires() : 0;
}

const char*      
CacheObject_GetFilename(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->Filename() : 0;
}

PRBool
CacheObject_GetIsCompleted(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->IsCompleted() : PR_TRUE;
}

PRIntervalTime
CacheObject_GetLastAccessed(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->LastAccessed() : 0;
}

PRIntervalTime
CacheObject_GetLastModified(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->LastModified() : 0;
}

PRInt16 
CacheObject_GetModule(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->Module() : -1;
}

const char*
CacheObject_GetPageServicesURL(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->PageServicesURL() : 0;
}

const char*
CacheObject_GetPostData(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->PostData() : 0;
}

PRUint32
CacheObject_GetPostDataLen(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->PostDataLen() : 0;
}

PRUint32
CacheObject_GetSize(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->Size() : 0;
}

PRUint32
CacheObject_GetState(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->State() : 0;
}

PRUint32
CacheObject_Hits(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->Hits() : 0;
}

PRBool
CacheObject_IsExpired(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->IsExpired() : PR_FALSE;
}

PRBool
CacheObject_IsPartial(const void* pThis)
{
    return pThis ? ((nsCacheObject*)pThis)->IsPartial() : PR_FALSE;
}

PRUint32
CacheObject_Read(const void* pThis, char* o_Buffer, PRUint32 i_Len)
{
    return pThis ? ((nsCacheObject*)pThis)->Read(o_Buffer, i_Len) : 0;
}

void
CacheObject_Reset(void* pThis)
{
    if (pThis)
        ((nsCacheObject*)pThis)->Reset();
}

void
CacheObject_SetAddress(void* pThis, const char* i_Address)
{
    if (pThis)
        ((nsCacheObject*)pThis)->Address(i_Address);
}

void
CacheObject_SetCharset(void* pThis, const char* i_Charset)
{
    if (pThis)
        ((nsCacheObject*)pThis)->Charset(i_Charset);
}

void
CacheObject_SetContentEncoding(void* pThis, const char* i_Encoding)
{
    if (pThis)
        ((nsCacheObject*)pThis)->ContentEncoding(i_Encoding);
}

void
CacheObject_SetContentLength(void* pThis, PRUint32 i_ContentLen)
{
    if (pThis)
        ((nsCacheObject*)pThis)->ContentLength(i_ContentLen);
}

void
CacheObject_SetContentType(void* pThis, const char* i_ContentType)
{
    if (pThis)
        ((nsCacheObject*)pThis)->ContentType(i_ContentType);
}

void
CacheObject_SetEtag(void* pThis, const char* i_Etag)
{
    if (pThis)
        ((nsCacheObject*)pThis)->Etag(i_Etag);
}

void
CacheObject_SetExpires(void *pThis, const PRIntervalTime i_Time)
{
    if (pThis)
        ((nsCacheObject*)pThis)->Expires(i_Time);
}

void
CacheObject_SetFilename(void* pThis, const char* i_Filename)
{
    if (pThis)
        ((nsCacheObject*)pThis)->Filename(i_Filename);
}

void
CacheObject_SetIsCompleted(void* pThis, PRBool bCompleted)
{
    if (pThis)
        ((nsCacheObject*)pThis)->IsCompleted(bCompleted);
}

void
CacheObject_SetLastModified(void* pThis, const PRIntervalTime i_Time)
{
    if (pThis)
        ((nsCacheObject*)pThis)->LastModified(i_Time);   
}

void
CacheObject_SetModule(void* pThis, const PRInt16 i_Module)
{
    if (pThis)
        ((nsCacheObject*)pThis)->Module(i_Module);
}

void
CacheObject_SetPageServicesURL(void* pThis, const char* i_Url)
{
    if (pThis)
        ((nsCacheObject*)pThis)->PageServicesURL(i_Url);
}

void
CacheObject_SetPostData(void* pThis, const char* i_PostData, const PRUint32 i_Len)
{
    if (pThis)
        ((nsCacheObject*)pThis)->PostData(i_PostData, i_Len);
}

void
CacheObject_SetSize(void* pThis, const PRUint32 i_Size)
{
    if (pThis)
        ((nsCacheObject*)pThis)->Size(i_Size);
}

void
CacheObject_SetState(void* pThis, const PRUint32 i_State)
{
    if (pThis)
        ((nsCacheObject*)pThis)->State(i_State);
}

PRBool
CacheObject_Synch(void* pThis)
{
    if (pThis)
    {
        nsCacheObject* pObj = (nsCacheObject*) pThis;
        return CACHEMGR->GetModule(pObj->Module())->AddObject(pObj);
    }
    return PR_FALSE;
}

PRUint32
CacheObject_Write(void* pThis, const char* i_buffer, const PRUint32 i_length)
{
    if (pThis)
    {
        nsCacheObject* pObj = (nsCacheObject*) pThis;
        return pObj->Write(i_buffer, i_length);
    }
    return 0;
}

/* CachePref functions */
PRUint32
CachePref_GetDiskCacheSize(void)
{
    return nsCachePref::GetInstance()->DiskCacheSize();
}

PRBool
CachePref_GetDiskCacheSSL(void)
{
    return nsCachePref::GetInstance()->DiskCacheSSL();
}

PRUint32
CachePref_GetMemCacheSize(void)
{
    return nsCachePref::GetInstance()->MemCacheSize();
}

void
CachePref_SetDiskCacheSize(const PRUint32 i_Size)
{
    nsCachePref::GetInstance()->DiskCacheSize(i_Size);
}

void
CachePref_SetDiskCacheSSL(PRBool bSet)
{
    nsCachePref::GetInstance()->DiskCacheSSL(bSet);
}

void
CachePref_SetMemCacheSize(const PRUint32 i_Size)
{
    nsCachePref::GetInstance()->MemCacheSize(i_Size);
}

/* CacheTrace functions */
void
CacheTrace_Enable(PRBool bEnable)
{
    nsCacheTrace::Enable(bEnable);
}

PRBool
CacheTrace_IsEnabled(void)
{
    return nsCacheTrace::IsEnabled();
}

/* DiskModule functions */
#define DISKMOD nsCacheManager::GetInstance()->GetDiskModule()

PRBool
DiskModule_AddObject(void* pObject)
{
    return DISKMOD->AddObject((nsCacheObject*)pObject);
}

PRBool
DiskModule_Contains(const char* i_url)
{
    return DISKMOD->Contains(i_url);
}

PRUint32
DiskModule_Entries(void)
{
    return DISKMOD->Entries();
}

PRUint32
DiskModule_GetSize(void)
{
    return DISKMOD->Size();
}

PRUint32
DiskModule_GetSizeInUse(void)
{
    return DISKMOD->SizeInUse(); 
}

PRBool
DiskModule_IsEnabled(void)
{
    return DISKMOD->IsEnabled();
}

PRBool
DiskModule_Remove(const char* i_url)
{
    return DISKMOD->Remove(i_url);
}

PRBool
DiskModule_RemoveAll(void)
{
    return DISKMOD->RemoveAll();
}

void
DiskModule_SetSize(PRUint32 i_Size)
{
    DISKMOD->SetSize(i_Size);
}

/* MemModule functions */
#define MEMMOD nsCacheManager::GetInstance()->GetMemModule()

PRBool
MemModule_AddObject(void* pObject)
{
    return MEMMOD->AddObject((nsCacheObject*)pObject);
}

PRBool
MemModule_Contains(const char* i_url)
{
    return MEMMOD->Contains(i_url);
}

PRUint32
MemModule_Entries(void)
{
    return MEMMOD->Entries();
}

PRUint32
MemModule_GetSize(void)
{
    return MEMMOD->Size();
}

PRUint32
MemModule_GetSizeInUse(void)
{
    return MEMMOD->SizeInUse() ;
}

PRBool
MemModule_IsEnabled(void)
{
    return MEMMOD->IsEnabled();
}

PRBool
MemModule_Remove(const char* i_url)
{
    return MEMMOD->Remove(i_url);
}

PRBool
MemModule_RemoveAll(void)
{
    return MEMMOD->RemoveAll();
}

void
MemModule_SetSize(PRUint32 i_Size)
{
    MEMMOD->SetSize(i_Size);
}

#undef MEMMOD
#undef DISKMOD
#undef CACHEMGR
