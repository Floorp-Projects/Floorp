/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-*/
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation..
 * Portions created by Netscape Communications Corporation. are
 * Copyright (C) 1999 Netscape Communications Corporation.. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   John Bandhauer <jband@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */


/*
 * This is a simple multithreaded test for the jar cache. Inorder to run it
 * you must create the zip files listed in "filenames" below. 
 */


#include "nsISupports.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"
#include "nsMemory.h"
#include "nsAutoLock.h"
#include "nsIRunnable.h"
#include "nsIThread.h"
#include "nsIZipReader.h"
#include "nsILocalFile.h"

#include <stdio.h>
#include <stdlib.h>

static char** filenames; 

#define ZIP_COUNT    8
#define CACHE_SIZE   4
#define THREAD_COUNT 6
#define THREAD_LOOP_COUNT 1000

static nsCOMPtr<nsILocalFile> files[ZIP_COUNT];

static const char gCacheContractID[] = "@mozilla.org/libjar/zip-reader-cache;1";
static const PRUint32 gCacheSize = 4;

nsCOMPtr<nsIZipReaderCache> gCache = nsnull;

static nsIZipReader* GetZipReader(nsILocalFile* file)
{
    if(!gCache)
    {
        gCache = do_CreateInstance(gCacheContractID);
        if(!gCache || NS_FAILED(gCache->Init(CACHE_SIZE)))
            return nsnull;
    }

    nsIZipReader* reader = nsnull;

    if(!file || NS_FAILED(gCache->GetZip(file, &reader)))
        return nsnull;

    return reader;
}

/***************************************************************************/

class TestThread : public nsIRunnable
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIRUNNABLE

    TestThread();
    virtual ~TestThread();

private:
    PRUint32 mID;
    static PRUint32 gCounter;
};

NS_IMPL_THREADSAFE_ISUPPORTS1(TestThread, nsIRunnable)

PRUint32 TestThread::gCounter = 0;

TestThread::TestThread()
    : mID(++gCounter)
{
    NS_INIT_REFCNT();
}

TestThread::~TestThread()
{
}

NS_IMETHODIMP
TestThread::Run()
{
    printf("thread %d started\n", mID);
    
    nsCOMPtr<nsIZipReader> reader;
    int failure = 0;
    
    for(int i = 0; i < THREAD_LOOP_COUNT; i++)
    {
        int k = rand()%ZIP_COUNT;
        reader = dont_AddRef(GetZipReader(files[k]));
        if(!reader)
        {
            printf("thread %d failed to get reader for %s\n", mID, filenames[k]);
            failure = 1;
            break;         
        }

        //printf("thread %d got reader for %s\n", mID, filenames[k]);

        PR_Sleep(rand()%10);    
    }
    
    reader = nsnull;

    printf("thread %d finished\n", mID);

    if ( failure ) return NS_ERROR_FAILURE;
    return NS_OK;
}

/***************************************************************************/

int main(int argc, char **argv)
{
    nsresult rv;
    int i;

    if (ZIP_COUNT != (argc - 1)) 
    {
        printf("usage: TestJarCache ");
        for ( i = 0; i < ZIP_COUNT; i++)
            printf("file%1d ",i + 1);
        printf("\n");
        return 1;
    }
    filenames = argv + 1;

    rv = NS_InitXPCOM(nsnull, nsnull);
    if(NS_FAILED(rv)) 
    {
        printf("NS_InitXPCOM failed!\n");
        return 1;
    }

    // construct the cache
    nsIZipReader* bogus = GetZipReader(nsnull);


    for(i = 0; i < ZIP_COUNT; i++)
    {
        PRBool exists;
        rv = NS_NewLocalFile(filenames[i], PR_FALSE, getter_AddRefs(files[i]));
        if(NS_FAILED(rv) || NS_FAILED(files[i]->Exists(&exists)) || !exists) 
        {   
            printf("Couldn't find %s\n", filenames[i]);
            return 1;
        }
    }

    nsCOMPtr<nsIThread> threads[THREAD_COUNT];

    for(i = 0; i < THREAD_COUNT; i++)
    {
        rv = NS_NewThread(getter_AddRefs(threads[i]),
                          new TestThread(), 
                          0, PR_JOINABLE_THREAD);
        if(NS_FAILED(rv)) 
        {
            printf("NS_NewThread failed!\n");
            return 1;
        }
        PR_Sleep(10);
    }

    printf("all threads created\n");

    for(i = 0; i < THREAD_COUNT; i++)
    {
        if(threads[i])
        {
            threads[i]->Join();
            threads[i] = nsnull;
        }
    }

    for(i = 0; i < ZIP_COUNT; i++)
        files[i] = nsnull;

    // kill the cache
    gCache = nsnull;

    rv = NS_ShutdownXPCOM(nsnull);
    if(NS_FAILED(rv)) 
    {
        printf("NS_ShutdownXPCOM failed!\n");
        return 1;
    }

    printf("done\n");

    return 0;
}    


