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
 * nsDiskModule
 *
 * Gagan Saksena 02/02/98
 * 
 */

#include "prtypes.h"
#include "prmem.h"
#include "plstr.h"
#include "prlog.h"
#include "prclist.h" 
#include "prio.h" 
#include "prsystem.h" // Directory Separator 

#include "nsDiskModule.h"
#include "nsCacheObject.h"
#include "nsCacheManager.h"
#include "nsFileStream.h"
#include "nsCachePref.h"

#include "mcom_db.h"

#define ENSURE_INIT  \
    if (!m_pDB)     \
    {               \
        nsDiskModule* pThis = (nsDiskModule*) this; \
        PRBool res = pThis->InitDB();   \
        PR_ASSERT(res);                 \
    }

struct recentlyUsedObject {
    PRCList link;
    nsCacheObject* cacheObject;
};

/* Find pointer to recentlyUsedObject struct 
 * from the list linkaged embedded in it 
 */
#define OBJECT_PTR(_link) \
    ((recentlyUsedObject*) ((char*) (_link) - offsetof(recentlyUsedObject,link)))

//File static list of recently used cache objects
static PRCList g_RecentlyUsedList;
static nsCacheObject* g_pTempObj=0;
static char* g_FullFilename=0;
const static int MAX_FILENAME_LEN = 512;
const static int MAX_OBJECTS_IN_RECENTLY_USED_LIST = 20; // change later TODO.

// Every time we cleanup we cleanup to 75% of the max available size. 
// This should ideally change to a more const number than a percentage. 
// Since someone may have a bigger cache size, so we don't really want to 
// be cleaning up 5 megs if some one has a 20 megs cache size.
// I will revisit this issue once the basic architecture is up and running. 
#define CLEANUP_FACTOR 0.75

//Returns the full filename including the cache directory.
static char* FullFilename(const char* i_Filename);

//Returns the Least Recently Used object in the database
static nsCacheObject* LRUObject(DB* pDB);

//
// Constructor: nsDiskModule
//
nsDiskModule::nsDiskModule(const PRUint32 size):
    nsCacheModule(size),
    m_pDB(0)
{
    PR_INIT_CLIST(&g_RecentlyUsedList);
}

nsDiskModule::~nsDiskModule()
{
    //This will free up any operational "over the limit" cache objects.
    GarbageCollect();

    if (m_pDB)
    {
        (*m_pDB->sync)(m_pDB, 0);
        (*m_pDB->close)(m_pDB);
        m_pDB = 0;
    }

    /* Clean up the recently used list */
    recentlyUsedObject* obj;
    while (!PR_CLIST_IS_EMPTY(&g_RecentlyUsedList)) 
    {
        obj = (recentlyUsedObject*) PR_LIST_HEAD(&g_RecentlyUsedList);
        if (obj->cacheObject)
            delete obj->cacheObject;
        PR_REMOVE_LINK(&obj->link);
    }

    PR_ASSERT(PR_CLIST_IS_EMPTY(&g_RecentlyUsedList));

    if (g_FullFilename)
    {
        delete[] g_FullFilename;
        g_FullFilename = 0;
    }

    if (g_pTempObj)
    {
        delete g_pTempObj;
        g_pTempObj = 0;
    }
}

PRBool nsDiskModule::AddObject(nsCacheObject* io_pObject)
{
    ENSURE_INIT;
    
    if (!m_pDB || !io_pObject)
    {   
        // Set some error state TODO
        return PR_FALSE;
    }   

    if (io_pObject->Address())
    {
        MonitorLocker ml(this);
        
        //Remove the earliar copy- silently handles if not found
        Remove(io_pObject->Address());

        // TODO optimize these further- make static - Gagan
        DBT* key = PR_NEW(DBT);
        DBT* data = PR_NEW(DBT);
        
        //Set the module
        io_pObject->Module(nsCacheManager::DISK);
        
        //Close the corresponding file 
        PR_ASSERT(io_pObject->Stream());
		if (io_pObject->Stream())
			PR_Close(((nsFileStream*)io_pObject->Stream())->FileDesc());

        key->data = (void*)io_pObject->Address(); 
        /* Later on change this to include post data- io_pObject->KeyData() */
        key->size = PL_strlen(io_pObject->Address());

        data->data = io_pObject->Info();
        data->size = io_pObject->InfoSize();

        int status = (*m_pDB->put)(m_pDB, key, data, 0);
        if (status == 0)
        {
//            if (m_Sync == EVERYTIME)
                status = (*m_pDB->sync)(m_pDB, 0);
                m_Entries++;
                m_SizeInUse += io_pObject->Size();
        }
        PR_Free(key);
        PR_Free(data);
        return (status == 0);
    }
    return PR_FALSE;
}

PRBool nsDiskModule::Contains(nsCacheObject* io_pObject) const
{
    ENSURE_INIT;

    if (!m_pDB || !io_pObject)
        return PR_FALSE;

    nsCacheObject* pTemp = GetObject(io_pObject->Address());
    if (pTemp)
    {
        //PR_ASSERT(io_pObject == pTemp); 
        // until I do a copyFrom function
        return PR_TRUE;
    }
    return PR_FALSE;
}

PRBool nsDiskModule::Contains(const char* i_url) const
{

    ENSURE_INIT;

    if (!m_pDB || !i_url || !*i_url)
        return PR_FALSE;

    DBT key, data;

    key.data = (void*) i_url;
    key.size = PL_strlen(i_url);

    int status = (*m_pDB->get)(m_pDB, &key, &data, 0);

    return (status == 0);

}

void nsDiskModule::GarbageCollect(void)
{
    MonitorLocker ml(this);
    ReduceSizeTo((PRUint32)(CLEANUP_FACTOR*m_Size));

    // if the recentlyusedlist has grown too big, trim some objects from there as well
    // Count how many are there
    int count=0;
    if (!PR_CLIST_IS_EMPTY(&g_RecentlyUsedList))
    {
        PRCList* list = g_RecentlyUsedList.next;
        while (list != &g_RecentlyUsedList)
        {
            count++;
            list = list->next;
        }
    }
    // Then trim the extra ones.
    int extra = count-MAX_OBJECTS_IN_RECENTLY_USED_LIST;
    while (extra>0)
    {
        recentlyUsedObject* obj = (recentlyUsedObject*) PR_LIST_HEAD(&g_RecentlyUsedList);
        if (obj->cacheObject)
            delete obj->cacheObject;
        PR_REMOVE_LINK(&obj->link);
        extra--;
    }
}

nsCacheObject* nsDiskModule::GetObject(const PRUint32 i_index) const
{
    ENSURE_INIT;
    if (!m_pDB)
        return 0;
//todo
    return 0;
}

nsCacheObject* nsDiskModule::GetObject(const char* i_url) const
{
    ENSURE_INIT;
    MonitorLocker ml((nsDiskModule*)this);

    if (!m_pDB || !i_url || !*i_url)
        return 0;

    /* Check amongst recently used objects */
    recentlyUsedObject* obj;
    PRCList* list = &g_RecentlyUsedList;
    if (!PR_CLIST_IS_EMPTY(&g_RecentlyUsedList)) 
    {
        list = g_RecentlyUsedList.next;
        nsCacheObject* pObj;
        while (list != &g_RecentlyUsedList)
        {
            obj = OBJECT_PTR(list);
            pObj = obj->cacheObject;
            if (0 == PL_strcasecmp(i_url, pObj->Address())) //todo also validate
                return pObj;
            list = list->next;
        }
    }

    DBT key, data;

    key.data = (void*) i_url;
    key.size = PL_strlen(i_url);

    if (0 == (*m_pDB->get)(m_pDB, &key, &data, 0))
    {
        nsCacheObject* pTemp = new nsCacheObject();
        pTemp->Info(data.data);
        recentlyUsedObject* pNode = PR_NEWZAP(recentlyUsedObject);
        PR_APPEND_LINK(&pNode->link, &g_RecentlyUsedList);
        pNode->cacheObject = pTemp;
        return pTemp;
    }

    return 0;
}

nsStream* nsDiskModule::GetStreamFor(const nsCacheObject* i_pObject)
{
    ENSURE_INIT;
    MonitorLocker ml(this);
    if (i_pObject)
    {
        nsStream* pStream = i_pObject->Stream();
        if (pStream)
            return pStream;

        PR_ASSERT(*i_pObject->Filename());

        char* fullname = FullFilename(i_pObject->Filename());

        if (fullname)
        {
            return new nsFileStream(fullname);
        }
    }
    return 0;
}

PRBool nsDiskModule::InitDB(void) 
{
    MonitorLocker ml(this);

    if (m_pDB)
        return PR_TRUE;

    HASHINFO hash_info = {
        16*1024,  /* bucket size */
        0,        /* fill factor */
        0,        /* number of elements */
        0,        /* bytes to cache */
        0,        /* hash function */
        0};       /* byte order */

    m_pDB = dbopen(
        FullFilename(nsCachePref::GetInstance()->DiskCacheDBFilename()), 
        O_RDWR | O_CREAT, 
        0600, 
        DB_HASH, 
        &hash_info);

    if (!m_pDB)
        return PR_FALSE;

    /* Open and read in the number of existing entries */
    m_Entries = 0;
    m_SizeInUse = 0;
    int status;
    DBT key, data;
    if (0 == g_pTempObj)
        g_pTempObj = new nsCacheObject();
    if(!(status = (*m_pDB->seq)(m_pDB, &key, &data, R_FIRST)))
    {
        do
        {
            /* Also validate the corresponding file here *///TODO
            g_pTempObj->Info(data.data);
            m_SizeInUse += g_pTempObj->Size();
            m_Entries++;
        }
        while(!(status = (*m_pDB->seq) (m_pDB, &key, &data, R_NEXT)));
    }

    if (status < 0)
        return PR_FALSE;

    return PR_TRUE;
}

PRBool nsDiskModule::ReduceSizeTo(const PRUint32 i_NewSize)
{
    MonitorLocker ml(this);

    PRInt32 needToFree = m_SizeInUse - i_NewSize;
    if ((m_Entries>0) && (needToFree > 0))
    {
        PRUint32 avg = m_SizeInUse/m_Entries;
        if (avg==0)
            return PR_FALSE; 
        
        PRUint32 nObjectsToFree = needToFree/avg;
        if (nObjectsToFree < 1)
            nObjectsToFree = 1;

        while (nObjectsToFree > 0)
        {
            Remove(LRUObject(m_pDB));
            --nObjectsToFree;
        }
        
        return PR_TRUE;
    }
    
    return PR_FALSE;
}

PRBool nsDiskModule::Remove(const char* i_url)
{
    ENSURE_INIT;
    return Remove(GetObject(i_url));
}

PRBool nsDiskModule::Remove(nsCacheObject* pObject)
{
    MonitorLocker ml(this);
    PRBool bStatus = PR_FALSE;
    if (!pObject)
        return bStatus;
    
    //PR_ASSERT(Contains(pObject);

    // TODO Mark the objects state for deletion so that we dont 
    // read it in the meanwhile.
    pObject->State(nsCacheObject::EXPIRED);

    //Remove it from the index
    DBT key;
    key.data = (void*) pObject->Address();
    key.size = PL_strlen(pObject->Address());
    if (0 == (*m_pDB->del)(m_pDB, &key, 0))
    {
        --m_Entries;
        m_SizeInUse -= pObject->Size();
		if (-1 == (*m_pDB->sync)(m_pDB, 0))
		{
			//Failed to sync database
			PR_ASSERT(0);
		}
    }
    
    //Remove it from the recently used list
    recentlyUsedObject* obj;
    PRCList* list = &g_RecentlyUsedList;
    if (!PR_CLIST_IS_EMPTY(&g_RecentlyUsedList)) 
    {
        list = g_RecentlyUsedList.next;
        nsCacheObject* pObj;
        PRBool bDone = PR_FALSE;
        while ((list != &g_RecentlyUsedList) && !bDone)
        {
            obj = OBJECT_PTR(list);
            pObj = obj->cacheObject;
            if (pObj == pObject)
            {
                bDone = PR_TRUE;
                PR_REMOVE_LINK(&obj->link);
            }
            list = list->next;
        }
    }
    
    //Remove it from the disk
    if (PR_SUCCESS == PR_Delete(FullFilename(pObject->Filename())))
    {
        bStatus = PR_TRUE;
    }
    else
    {
        //Failed to delete the file off the disk!
        bStatus = PR_FALSE;
    }

    //Finally delete it 
    delete pObject; 
    pObject = 0;

    return PR_FALSE;
}
PRBool nsDiskModule::Remove(const PRUint32 i_index)
{
    //This will probably go away. 
    ENSURE_INIT;
    //TODO
    // Also remove the file corresponding to this item. 
    return PR_FALSE;
}

PRBool nsDiskModule::Revalidate(void)
{
    ENSURE_INIT;
    //TODO - This will add a dependency on HTTP lib
    return PR_FALSE;
}

void nsDiskModule::SetSize(const PRUint32 i_Size)
{
    MonitorLocker ml(this);
    m_Size = i_Size;
    if (m_Size >0)
    {
        ReduceSizeTo((PRUint32)(CLEANUP_FACTOR*m_Size));
    }
    else
    {
        RemoveAll();
    }
}

char* FullFilename(const char* i_Filename)
{
    static int cacheFolderLength=0;
    if (0 == g_FullFilename)
    {
        g_FullFilename = new char[MAX_FILENAME_LEN];
        *g_FullFilename = '\0';
        if (0 == g_FullFilename)
            return 0;
    }
    PL_strcpy(g_FullFilename, nsCachePref::GetInstance()->DiskCacheFolder());
    if (0==cacheFolderLength)
        cacheFolderLength = PL_strlen(nsCachePref::GetInstance()->DiskCacheFolder());
#ifndef XP_MAC
	if (g_FullFilename[cacheFolderLength-1]!=PR_GetDirectorySeparator())
	{
		g_FullFilename[cacheFolderLength] = PR_GetDirectorySeparator(); 
		g_FullFilename[cacheFolderLength+1] = '\0';
	}
#endif
    PL_strcat(g_FullFilename, i_Filename);
    return g_FullFilename;
}

nsCacheObject* LRUObject(DB* pDB)
{
    int status;
    DBT key, data;
    
    nsCacheObject* pTempObj = new nsCacheObject();
    
    nsCacheObject* pOldest=0;
    if(!(status = (*pDB->seq)(pDB, &key, &data, R_FIRST)))
    {
        do
        {
            pTempObj->Info(data.data);
            if (!pOldest ||
                (pOldest->LastAccessed() > pTempObj->LastAccessed()))
                pOldest = pTempObj;
        }
        while(!(status = (*pDB->seq) (pDB, &key, &data, R_NEXT)));
    }
    return pOldest;
}

#undef ENSURE_INIT
