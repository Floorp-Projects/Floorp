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
#include "nsICacheObject.h"
#include "nsCacheObject.h"
#include "nsCacheManager.h"
#include "nsFileStream.h"
#include "nsCachePref.h"
#include "nsRepository.h"

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
    nsICacheObject* cacheObject;
};

/* Find pointer to recentlyUsedObject struct 
 * from the list linkaged embedded in it 
 */
#define OBJECT_PTR(_link) \
    ((recentlyUsedObject*) ((char*) (_link) - offsetof(recentlyUsedObject,link)))

//File static list of recently used cache objects
static PRCList g_RecentlyUsedList;
static nsICacheObject* g_pTempObj=0;
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
static nsICacheObject* LRUObject(DB* pDB);


static NS_DEFINE_IID (kICacheObjectIID, NS_ICACHEOBJECT_IID);
static NS_DEFINE_IID (kCacheObjectCID,  NS_CACHEOBJECT_CID);

static NS_DEFINE_IID (kICacheManagerIID, NS_ICACHEMANAGER_IID);
static NS_DEFINE_IID (kCacheManagerCID,  NS_CACHEMANAGER_CID);


static nsICachePref* 
GetPrefsInstance ()
{
    nsICacheManager* pICacheMgr = nsnull;
    nsServiceManager::GetService (kCacheManagerCID, kICacheManagerIID,
                                  (nsISupports **) &pICacheMgr);

    if (!pICacheMgr)
        return nsnull;

    nsICachePref* pICachePref = nsnull;
    pICacheMgr->GetPrefs (&pICachePref);
    return pICachePref;
}


//
// Constructors: nsDiskModule
//
nsDiskModule::nsDiskModule():
    m_SizeInUse(0),
    m_pEnumeration(0),
    m_pNext(0),
    m_pDB(0)
{
    nsICachePref* pICachePref = GetPrefsInstance();
    PR_ASSERT (pICachePref);
    pICachePref->GetDiskCacheSize (&m_Size);

    PR_INIT_CLIST(&g_RecentlyUsedList);
}

nsDiskModule::nsDiskModule(const PRUint32 size):
    m_Size (size),
    m_SizeInUse(0),
    m_pEnumeration(0),
    m_pNext(0),
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

NS_IMETHODIMP
nsDiskModule::AddObject(nsICacheObject* io_pObject)
{
    ENSURE_INIT;
    
    if (!m_pDB || !io_pObject)
    {   
        // Set some error state TODO
        return NS_ERROR_FAILURE;
    }   

	char *url = nsnull;
	nsresult r = io_pObject->GetAddress (&url);
    if (r == NS_OK && url)
    {
        MonitorLocker ml(this);
        
        //Remove the earliar copy- silently handles if not found
        RemoveByURL(url);

        // TODO optimize these further- make static - Gagan
        DBT* key = PR_NEW(DBT);
        DBT* data = PR_NEW(DBT);
        
        //Set the module
        io_pObject->SetModuleIndex (nsCacheManager::DISK);
        
        //Close the corresponding file 
        nsStream* pStream = (nsStream *) nsnull;
		r = io_pObject->GetStream (&pStream);

		if (pStream)
			PR_Close(((nsFileStream*)pStream)->FileDesc());

        key->data = (void*)url;
        /* Later on change this to include post data- io_pObject->KeyData() */
        key->size = PL_strlen(url);

		io_pObject->GetInfo ((void **) &data->data);
		io_pObject->GetInfoSize (&data->size);

        int status = (*m_pDB->put)(m_pDB, key, data, 0);
        if (status == 0)
        {
//            if (m_Sync == EVERYTIME)
                status = (*m_pDB->sync)(m_pDB, 0);
                m_Entries++;
                PRUint32 size;
				io_pObject->GetSize(&size);
                m_SizeInUse += size;
        }
        PR_Free(key);
        PR_Free(data);
        return ((status == 0) ? NS_OK : NS_ERROR_FAILURE);
    }
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsDiskModule::ContainsCacheObj(nsICacheObject* io_pObject, PRBool* o_bContain)
{
    ENSURE_INIT;

	*o_bContain = PR_FALSE;
    if (!m_pDB || !io_pObject)
        return NS_OK;

	char *url;
	if (io_pObject->GetAddress (&url) != NS_OK)
		return NS_OK;

    nsICacheObject* pTemp = 0;
    if (GetObjectByURL(url, &pTemp) != NS_OK)
		return NS_OK;

    if (pTemp)
    {
        //PR_ASSERT(io_pObject == pTemp); 
        // until I do a copyFrom function
        *o_bContain = PR_TRUE;
    }
    return NS_OK;
}


NS_IMETHODIMP
nsDiskModule::ContainsURL(const char* i_url, PRBool* o_bContain)
{

    ENSURE_INIT;

	*o_bContain = PR_FALSE;
    if (!m_pDB || !i_url || !*i_url)
        return NS_OK;

    DBT key, data;

    key.data = (void*) i_url;
    key.size = PL_strlen(i_url);

    int status = (*m_pDB->get)(m_pDB, &key, &data, 0);

	*o_bContain = (status == 0)? PR_TRUE : PR_FALSE;
	return NS_OK;
}


NS_IMETHODIMP
nsDiskModule::GarbageCollect(void)
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
	return NS_OK;
}


NS_IMETHODIMP
nsDiskModule::GetObjectByIndex(const PRUint32 i_index, 
								nsICacheObject** ppICacheObject)
{
    ENSURE_INIT;

	if (!ppICacheObject)
		return NS_ERROR_NULL_POINTER;

	*ppICacheObject = (nsICacheObject *) nsnull;
    if (!m_pDB)
        return NS_OK;

//todo
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsDiskModule::GetObjectByURL(const char* i_url, nsICacheObject** ppICacheObject)
{
    ENSURE_INIT;
    MonitorLocker ml((nsDiskModule*)this);

	if (!ppICacheObject)
		return NS_ERROR_NULL_POINTER;

	*ppICacheObject = (nsICacheObject *) nsnull;

    if (!m_pDB || !i_url || !*i_url)
        return NS_OK;

    /* Check amongst recently used objects */
    recentlyUsedObject* obj;
    PRCList* list = &g_RecentlyUsedList;
    if (!PR_CLIST_IS_EMPTY(&g_RecentlyUsedList)) 
    {
        list = g_RecentlyUsedList.next;
        nsICacheObject* pObj;
        while (list != &g_RecentlyUsedList)
        {
            obj = OBJECT_PTR(list);
            pObj = obj->cacheObject;
			char *addr;
			nsresult r = pObj->GetAddress (&addr);
			if (r != NS_OK)
				return r;
            if (0 == PL_strcasecmp(i_url, addr)) //todo also validate
			{
				*ppICacheObject = pObj;
                return NS_OK;
			}
            list = list->next;
        }
    }

    DBT key, data;

    key.data = (void*) i_url;
    key.size = PL_strlen(i_url);

    if (0 == (*m_pDB->get)(m_pDB, &key, &data, 0))
    {
        nsICacheObject* pTemp;
	nsresult rv = nsComponentManager::CreateInstance(kCacheObjectCID,
						nsnull,
						kICacheObjectIID,
						(void**) &pTemp);
        PR_ASSERT(rv == NS_OK);
        pTemp->SetInfo(data.data);
        recentlyUsedObject* pNode = PR_NEWZAP(recentlyUsedObject);
        PR_APPEND_LINK(&pNode->link, &g_RecentlyUsedList);
        pNode->cacheObject = pTemp;
		*ppICacheObject = pTemp;
    }

    return NS_OK;
}


NS_IMETHODIMP
nsDiskModule::GetStreamFor(const nsICacheObject* i_pObject, nsStream** o_stream)
{
    ENSURE_INIT;
    MonitorLocker ml(this);

	*o_stream = 0;
    if (i_pObject)
    {
        nsresult r = i_pObject->GetStream(o_stream);
        if (r != NS_OK)
            return r;

		char *filename = nsnull;
		i_pObject->GetFilename (&filename);
        PR_ASSERT(filename);

        char* fullname = FullFilename(filename);

        if (fullname)
        {
            *o_stream = new nsFileStream(fullname);
        }
    }
    return NS_OK;
}


//  Private Function
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

    char *filename;
    nsICachePref* pICachePref = GetPrefsInstance();
    PR_ASSERT (pICachePref);
    pICachePref->GetDiskCacheDBFilename (&filename);

    m_pDB = dbopen(
        FullFilename(filename), 
        O_RDWR | O_CREAT, 
        0600, // this is octal, yixiong 
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
    {
	nsresult rv = nsComponentManager::CreateInstance(kCacheObjectCID,
						nsnull,
						kICacheObjectIID,
						(void**) &g_pTempObj);
        PR_ASSERT (rv == NS_OK);
    }
    if(!(status = (*m_pDB->seq)(m_pDB, &key, &data, R_FIRST)))
    {
        do
        {
            /* Also validate the corresponding file here *///TODO

            g_pTempObj->SetInfo(data.data);
			PRUint32 size;
			g_pTempObj->GetSize(&size);
            m_SizeInUse += size;
            m_Entries++;
        }
        while(!(status = (*m_pDB->seq) (m_pDB, &key, &data, R_NEXT)));
    }

    if (status < 0)
        return PR_FALSE;

    return PR_TRUE;
}


NS_IMETHODIMP
nsDiskModule::ReduceSizeTo(const PRUint32 i_NewSize)
{
    MonitorLocker ml(this);

    PRInt32 needToFree = m_SizeInUse - i_NewSize;
    if ((m_Entries>0) && (needToFree > 0))
    {
        PRUint32 avg = m_SizeInUse/m_Entries;
        if (avg==0)
            return NS_ERROR_FAILURE; 
        
        PRUint32 nObjectsToFree = needToFree/avg;
        if (nObjectsToFree < 1)
            nObjectsToFree = 1;

        while (nObjectsToFree > 0)
        {
            RemoveByObject (LRUObject(m_pDB));
            --nObjectsToFree;
        }
        
        return NS_OK;
    }
    
    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP
nsDiskModule::RemoveByURL(const char* i_url)
{
    ENSURE_INIT;
	nsICacheObject* pICacheObject;
	nsresult r = GetObjectByURL(i_url, &pICacheObject);
	if (r != NS_OK)
		return r;
    return RemoveByObject(pICacheObject);
}


NS_IMETHODIMP
nsDiskModule::RemoveByObject(nsICacheObject* pObject)
{
    MonitorLocker ml(this);
    nsresult bStatus = NS_ERROR_FAILURE;
    if (!pObject)
        return bStatus;
    
    //PR_ASSERT(Contains(pObject);

    // TODO Mark the objects state for deletion so that we dont 
    // read it in the meanwhile.
    pObject->SetState(nsCacheObject::EXPIRED);

    //Remove it from the index
    DBT key;
    pObject->GetAddress((char **) &key.data);
    key.size = PL_strlen((char *)key.data);
    if (0 == (*m_pDB->del)(m_pDB, &key, 0))
    {
        --m_Entries;
		PRUint32 size;
		pObject->GetSize (&size);
        m_SizeInUse -= size;
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
        nsICacheObject* pObj;
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
	char *filename;
	pObject->GetFilename(&filename);
    if (PR_SUCCESS == PR_Delete(FullFilename(filename)))
    {
        bStatus = NS_OK;
    }
    else
    {
        //Failed to delete the file off the disk!
        bStatus = NS_ERROR_FAILURE;
    }

    //Finally delete it 
    delete pObject; 
    pObject = 0;

    return bStatus;
}


NS_IMETHODIMP
nsDiskModule::RemoveByIndex(const PRUint32 i_index)
{
    //This will probably go away. 
    ENSURE_INIT;
    //TODO
    // Also remove the file corresponding to this item. 
    //return PR_FALSE;
	return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsDiskModule::RemoveAll(void)
{
    MonitorLocker ml (this);
    while (m_Entries > 0)
    {
        RemoveByIndex (--m_Entries);
    }
    return NS_OK;
}


NS_IMETHODIMP
nsDiskModule::Revalidate(void)
{
    ENSURE_INIT;
    //TODO - This will add a dependency on HTTP lib
    //return PR_FALSE;
	return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsDiskModule::SetSize(const PRUint32 i_Size)
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

	return NS_OK;
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
	char *folder;
    nsICachePref* pICachePref = GetPrefsInstance();
    PR_ASSERT (pICachePref);
    pICachePref->GetDiskCacheFolder (&folder);

    PR_ASSERT (folder);
    PL_strcpy(g_FullFilename, folder);
    if (0==cacheFolderLength)
        cacheFolderLength = PL_strlen(folder);
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

nsICacheObject* LRUObject(DB* pDB)
{
    int status;
    DBT key, data;
    
    nsICacheObject* pTempObj;
    nsresult rv = nsComponentManager::CreateInstance(kCacheObjectCID,
						nsnull,
						kICacheObjectIID,
						(void**) &pTempObj);
    
    if (rv != NS_OK)
        return nsnull;

    nsICacheObject* pOldest=0;
    if(!(status = (*pDB->seq)(pDB, &key, &data, R_FIRST)))
    {
        do
        {
            pTempObj->SetInfo(data.data);

			PRIntervalTime old, current;
			if (pOldest)
			{
				pOldest->GetLastAccessed (&old);
			}
			pTempObj->GetLastAccessed (&current);
            if (!pOldest || (old > current))
                pOldest = pTempObj;
			   
        }
        while(!(status = (*pDB->seq) (pDB, &key, &data, R_NEXT)));
    }
    return ((nsICacheObject *) pOldest);
}


NS_IMETHODIMP
nsDiskModule::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
	NS_ASSERTION(aInstancePtr, "no instance pointer");

	*aInstancePtr = 0;

	if (aIID.Equals(nsCOMTypeInfo<nsISupports>::GetIID()) ||
		aIID.Equals(nsCOMTypeInfo<nsICacheModule>::GetIID()))
    {
        *aInstancePtr = NS_STATIC_CAST (nsICacheModule*, this);
		NS_ADDREF_THIS();
		return NS_OK;
	}
	return NS_NOINTERFACE;
}


NS_IMPL_ADDREF(nsDiskModule);
NS_IMPL_RELEASE(nsDiskModule);


NS_METHOD
nsDiskModule::Create(nsISupports* aOuter, const nsIID& aIID, void** aResult)
{
	nsDiskModule* cm = new nsDiskModule();
	if (cm == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

	NS_ADDREF (cm);
	nsresult rv = cm->QueryInterface (aIID, aResult);
	NS_RELEASE (cm);
	return rv;
}


NS_IMETHODIMP 
nsDiskModule::Enable(PRBool i_bEnable)
{
    m_Enabled = i_bEnable;
	return NS_OK;
}

NS_IMETHODIMP 
nsDiskModule::GetNumOfEntries(PRUint32* o_nEntries)
{
    *o_nEntries = m_Entries;
    return NS_OK;
}


NS_IMETHODIMP
nsDiskModule::GetEnumerator(nsEnumeration** o_enum)
{
    MonitorLocker ml((nsMonitorable*)this);
    if (!m_pEnumeration)
    {
        PR_ASSERT(m_pIterator);
        ((nsDiskModule*)this)->m_pEnumeration = 
									new nsEnumeration((nsIterator*)m_pIterator);
    }
    else
        ((nsDiskModule*)this)->m_pEnumeration->Reset();
    *o_enum = m_pEnumeration;
    return NS_OK;
}


NS_IMETHODIMP
nsDiskModule::IsEnabled(PRBool* o_bEnabled)
{
	*o_bEnabled = m_Enabled;
    return NS_OK;
}


NS_IMETHODIMP
nsDiskModule::IsReadOnly (PRBool* o_bReadOnly)
{
	*o_bReadOnly  = PR_FALSE;		// for Now
    return NS_OK;
}


NS_IMETHODIMP
nsDiskModule::GetNextModule(nsICacheModule** o_pICacheModule)
{
	*o_pICacheModule = m_pNext;
    return NS_OK;
}


NS_IMETHODIMP
nsDiskModule::SetNextModule(nsICacheModule* pNext) 
{
    /* No overwriting */
    PR_ASSERT(m_pNext == 0);
    if (m_pNext)
    {
        /* ERROR */
        delete m_pNext; //Worst case. 
        m_pNext = 0;
    }
    m_pNext = pNext;
	return NS_OK;
}


NS_IMETHODIMP
nsDiskModule::GetSize(PRUint32* o_size)
{
	*o_size = m_Size;
    return NS_OK;
}


NS_IMETHODIMP
nsDiskModule::GetSizeInUse(PRUint32* o_size)
{
	*o_size = m_SizeInUse;
    return NS_OK;
}

#undef ENSURE_INIT
