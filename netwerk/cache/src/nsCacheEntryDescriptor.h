/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is nsCacheEntryDescriptor.h, released February 22, 2001.
 * 
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 *    Gordon Sheridan, 22-February-2001
 */


#ifndef _nsCacheEntryDescriptor_h_
#define _nsCacheEntryDescriptor_h_

#include "nsICacheEntryDescriptor.h"
#include "nsCacheEntry.h"
#include "nsIOutputStream.h"
#include "nsITransport.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

/******************************************************************************
* nsCacheEntryDescriptor
*******************************************************************************/
class nsCacheEntryDescriptor :
    public PRCList,
    public nsICacheEntryDescriptor
{
public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICACHEENTRYDESCRIPTOR
    NS_DECL_NSICACHEENTRYINFO
    
    nsCacheEntryDescriptor(nsCacheEntry * entry, nsCacheAccessMode  mode);
    virtual ~nsCacheEntryDescriptor();
    
    static nsresult Create(nsCacheEntry * entry, nsCacheAccessMode  accessGranted,
                           nsICacheEntryDescriptor ** result);


    /**
     * utility method to attempt changing data size of associated entry
     */
    nsresult  RequestDataSizeChange(PRInt32 deltaSize);
    
    /**
     * methods callbacks for nsCacheService
     */
    nsCacheEntry * CacheEntry(void)      { return mCacheEntry; }
    void           ClearCacheEntry(void) { mCacheEntry = nsnull; }

private:

     /*************************************************************************
      * transport wrapper class - 
      *
      * we want the transport wrapper to have the same lifetime as the
      * descriptor, but since they each need to reference the other, we have the
      * descriptor include the transport wrapper as a member, rather than just
      * pointing to it, which avoids circular AddRefs.
      *************************************************************************/
     class nsTransportWrapper : public nsITransport
     {
     public:
         NS_DECL_ISUPPORTS_INHERITED
         NS_DECL_NSITRANSPORT

         nsTransportWrapper() : mCallbackFlags(0) {}
         virtual ~nsTransportWrapper() {}

         nsresult EnsureTransportWithAccess(nsCacheAccessMode  mode);

         PRUint32                         mCallbackFlags;

         nsCOMPtr<nsITransport>           mTransport;
         nsCOMPtr<nsIInterfaceRequestor>  mCallbacks;
     }; // end of class nsTransportWrapper
     friend class nsTransportWrapper;


     /*************************************************************************
      * output stream wrapper class -
      *
      * The output stream wrapper references the descriptor, but the descriptor
      * doesn't need any references to the stream wrapper, so we don't need the
      * same kind of tricks that we're using for the transport wrapper.
      *************************************************************************/
     class nsOutputStreamWrapper : public nsIOutputStream {
     private:
         nsCacheEntryDescriptor *  mDescriptor;
         nsCOMPtr<nsIOutputStream> mOutput;
     public:
         NS_DECL_ISUPPORTS
         // NS_DECL_NSIOUTPUTSTREAM
         NS_IMETHOD Close(void) { return mOutput->Close(); }
         NS_IMETHOD Flush(void) { return mOutput->Flush(); }

         NS_IMETHOD Write(const char * buf,
                          PRUint32     count,
                          PRUint32 *   result);

         NS_IMETHOD WriteFrom(nsIInputStream * inStr,
                              PRUint32         count,
                              PRUint32 *       result);

         NS_IMETHOD WriteSegments(nsReadSegmentFun reader,
                                  void *           closure,
                                  PRUint32         count,
                                  PRUint32 *       result);

         NS_IMETHOD IsNonBlocking(PRBool * nonBlocking)
         { return mOutput->IsNonBlocking(nonBlocking); }

         nsOutputStreamWrapper(nsCacheEntryDescriptor * descriptor,
                               nsIOutputStream *        output)
             : mDescriptor(nsnull), mOutput(output)
         {
             NS_ADDREF(mDescriptor = descriptor);
         }
    
         virtual ~nsOutputStreamWrapper()
         {
             NS_RELEASE(mDescriptor);
         }
    
         nsresult Init();


     private:
         nsresult OnWrite(PRUint32 count);
     }; // end of class nsOutputStreamWrapper
     friend class nsOutputStreamWrapper;



     static nsresult NewOutputStreamWrapper(nsIOutputStream **       result,
                                            nsCacheEntryDescriptor * descriptor,
                                            nsIOutputStream *        output);
 private:
     /**
      * nsCacheEntryDescriptor data members
      */
     nsCacheEntry          * mCacheEntry; // we are a child of the entry
     nsCacheAccessMode       mAccessGranted;
     nsTransportWrapper      mTransportWrapper;
};


#endif // _nsCacheEntryDescriptor_h_
