/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIFileTransportService.h"
#include "nsIChannel.h"
#include "nsITransport.h"
#include "nsIServiceManager.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsIMemory.h"
#include "nsString.h"
#include "nsIFileStream.h"
#include "nsIStreamListener.h"
#include "nsIEventQueueService.h"
#include "nsIEventQueue.h"
#include "nsILocalFile.h"
#include "plhash.h"


static NS_DEFINE_CID(kFileTransportServiceCID, NS_FILETRANSPORTSERVICE_CID);

////////////////////////////////////////////////////////////////////////////////

// A supply of stream data to either store or compare with
class nsITestDataStream {
public:
    virtual ~nsITestDataStream() {};
    virtual PRUint32 Next() = 0;
    virtual void Read(char* aBuf, PRUint32 aCount) = 0;

    virtual PRBool Match(char* aBuf, PRUint32 aCount) = 0;
    virtual void Skip(PRUint32 aCount) = 0;
};

// A reproducible stream of random data.
class RandomStream : public nsITestDataStream {
public:
    RandomStream(PRUint32 aSeed) {
        mStartSeed = mState = aSeed;
    }
    
    PRUint32 GetStartSeed() {
        return mStartSeed;
    }
    
    PRUint32 Next() {
        mState = 1103515245 * mState + 12345;
        return mState;
    }

    void Read(char* aBuf, PRUint32 aCount) {
        PRUint32 i;
        for (i = 0; i < aCount; i++) {
            *aBuf++ = Next();
        }
    }

    PRBool
    Match(char* aBuf, PRUint32 aCount) {
        PRUint32 i;
        for (i = 0; i < aCount; i++) {
            if (*aBuf++ != (char)(Next() & 0xff))
                return PR_FALSE;
        }
        return PR_TRUE;
    }

    void
    Skip(PRUint32 aCount) {
        while (aCount--)
            Next();
    }

protected:
    
    PRUint32 mState;
    PRUint32 mStartSeed;
};

PRIntervalTime gDuration;
PRUint32 gTotalBytesWritten = 0;

////////////////////////////////////////////////////////////////////////////////

nsresult
TestSyncWrite(char* filename, PRUint32 startPosition, PRInt32 length)
{
    nsresult rv;
    nsCOMPtr<nsIOutputStream> outStream ;
    RandomStream *randomStream;
    char buf[500];
    
    nsCOMPtr<nsIFileTransportService> fts = 
             do_GetService(kFileTransportServiceCID, &rv) ;
    if (NS_FAILED(rv)) return rv ;
  
    nsCOMPtr<nsILocalFile> fs;
    rv = NS_NewLocalFile(filename, PR_FALSE, getter_AddRefs(fs));
    if (NS_FAILED(rv)) return rv ;

    nsCOMPtr<nsITransport> transport;
    rv = fts->CreateTransport(fs, PR_RDWR | PR_CREATE_FILE, 0664,
                              getter_AddRefs(transport)) ;
    if (NS_FAILED(rv)) return rv ;
 
    rv = transport->OpenOutputStream(startPosition, -1, 0, getter_AddRefs(outStream)) ;
    if (NS_FAILED(rv)) return rv;
    
    PRIntervalTime startTime = PR_IntervalNow();

    randomStream = new RandomStream(PL_HashString(filename));

    int remaining = length;
    while (remaining) {
        PRUint32 numWritten;
        int amount = PR_MIN(sizeof buf, remaining);
        randomStream->Read(buf, amount);

        rv = outStream->Write(buf, amount, &numWritten);
        NS_ASSERTION(NS_SUCCEEDED(rv), " ");
        NS_ASSERTION(numWritten == (PRUint32)amount, "Write() bug?");
        
        remaining -= amount;
    }
    outStream->Close();
    gTotalBytesWritten += length;

    PRIntervalTime endTime = PR_IntervalNow();
    gDuration += (endTime - startTime);

    delete randomStream;

    return NS_OK;
}

#define MAX_FILES_TO_WRITE 15

nsresult
TestSyncWrites(char* filenamePrefix, PRUint32 startPosition, PRInt32 length)
{
    char filename[100];
    int test;
    nsresult rv;

    for (test = 0; test < MAX_FILES_TO_WRITE; test++) {
        sprintf(filename, "%s_%d", filenamePrefix, test);

        rv = TestSyncWrite(filename, startPosition, length);
        if (NS_FAILED(rv)) return rv;
    }

    double rate = gTotalBytesWritten / PR_IntervalToMilliseconds(gDuration);
    rate *= 1000;
    rate /= (1024 * 1024);
    printf("Wrote %7d bytes at a rate of %6.2f MB per second \n",
           gTotalBytesWritten, rate);

    return NS_OK;
}
        
////////////////////////////////////////////////////////////////////////////////

nsresult
NS_AutoregisterComponents()
{
  nsresult rv = nsComponentManager::AutoRegister(nsIComponentManager::NS_Startup, NULL /* default */);
  return rv;
}

int
main(int argc, char* argv[])
{
    nsresult rv;

    if (argc < 3) {
        printf("usage: %s <file-prefix-to-write> <num-bytes>\n", argv[0]);
        return -1;
    }
    char* fileName = argv[1];
    int length = atoi(argv[2]);

    rv = NS_AutoregisterComponents();
    if (NS_FAILED(rv)) return rv;

    rv = TestSyncWrites(fileName, 0, length);
    NS_ASSERTION(NS_SUCCEEDED(rv), "TestAsyncRead failed");

    return NS_OK;
}
