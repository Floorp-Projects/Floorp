/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Douglas Turner <dougt@netscape.com>
 *     Daniel Veditz <dveditz@netscape.com>
 *     Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsTopProgressNotifier.h"

nsTopProgressListener::nsTopProgressListener()
{
    NS_INIT_ISUPPORTS();
    mListeners = new nsVoidArray();
    mActive = 0;
    mLock = PR_NewLock();
}

nsTopProgressListener::~nsTopProgressListener()
{
    if (mLock) PR_Lock(mLock);

    if (mListeners)
    {
        PRInt32 i=0;
        for (; i < mListeners->Count(); i++) 
        {
            nsIXPIListener* element = (nsIXPIListener*)mListeners->ElementAt(i);
            NS_IF_RELEASE(element);
        }

        mListeners->Clear();
        delete (mListeners);
    }

    if (mLock)
    {
        PR_Unlock(mLock);
        PR_DestroyLock(mLock);
    }
}


NS_IMPL_THREADSAFE_ISUPPORTS1(nsTopProgressListener, nsIXPIListener);


long
nsTopProgressListener::RegisterListener(nsIXPIListener * newListener)
{
    if (mLock) PR_Lock(mLock);
    NS_IF_ADDREF( newListener );
    long retval = mListeners->AppendElement( newListener );
    if (mLock) PR_Unlock(mLock);
    return retval;
}


void
nsTopProgressListener::UnregisterListener(long id)
{
    if (mLock) PR_Lock(mLock);
    nsIXPIListener *item = (nsIXPIListener*)mListeners->ElementAt(id);
    mListeners->ReplaceElementAt(nsnull, id);
    NS_IF_RELEASE(item);
    if (mLock) PR_Unlock(mLock);
}



NS_IMETHODIMP
nsTopProgressListener::BeforeJavascriptEvaluation(const PRUnichar *URL)
{
    if (mActive)
        mActive->BeforeJavascriptEvaluation(URL);

    if (mListeners)
    {
        PRInt32 i=0;
        for (; i < mListeners->Count(); i++) 
        {
            nsIXPIListener* element = (nsIXPIListener*)mListeners->ElementAt(i);
            if (element != NULL)
                element->BeforeJavascriptEvaluation(URL);
        }
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsTopProgressListener::AfterJavascriptEvaluation(const PRUnichar *URL)
{
    if (mActive)
        mActive->AfterJavascriptEvaluation(URL);

    if (mListeners)
    {
        PRInt32 i=0;
        for (; i < mListeners->Count(); i++) 
        {
            nsIXPIListener* element = (nsIXPIListener*)mListeners->ElementAt(i);
            if (element != NULL)
                element->AfterJavascriptEvaluation(URL);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsTopProgressListener::InstallStarted(const PRUnichar *URL, const PRUnichar* UIPackageName)
{
    if (mActive)
        mActive->InstallStarted(URL, UIPackageName);

    if (mListeners)
    {
        PRInt32 i=0;
        for (; i < mListeners->Count(); i++) 
        {
            nsIXPIListener* element = (nsIXPIListener*)mListeners->ElementAt(i);
            if (element != NULL)
                element->InstallStarted(URL, UIPackageName);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsTopProgressListener::ItemScheduled( const PRUnichar* message )
{
    long rv = 0;

    if (mActive)
        mActive->ItemScheduled( message );

    if (mListeners)
    {
        PRInt32 i=0;
        for (; i < mListeners->Count(); i++) 
        {
            nsIXPIListener* element = (nsIXPIListener*)mListeners->ElementAt(i);
            if (element != NULL)
                element->ItemScheduled( message );
        }
    }

    return rv;
}

NS_IMETHODIMP
nsTopProgressListener::FinalizeProgress( const PRUnichar* message, PRInt32 itemNum, PRInt32 totNum )
{
    if (mActive)
        mActive->FinalizeProgress( message, itemNum, totNum );

    if (mListeners)
    {
        PRInt32 i=0;
        for (; i < mListeners->Count(); i++) 
        {
            nsIXPIListener* element = (nsIXPIListener*)mListeners->ElementAt(i);
            if (element != NULL)
                element->FinalizeProgress( message, itemNum, totNum );
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsTopProgressListener::FinalStatus(const PRUnichar *URL, PRInt32 status)
{
    if (mActive)
        mActive->FinalStatus(URL, status);

    if (mListeners)
    {
        PRInt32 i=0;
        for (; i < mListeners->Count(); i++) 
        {
            nsIXPIListener* element = (nsIXPIListener*)mListeners->ElementAt(i);
            if (element != NULL)
                element->FinalStatus(URL,status);
        }
    }
   return NS_OK;
}

NS_IMETHODIMP
nsTopProgressListener::LogComment(const PRUnichar* comment)
{
    if (mActive)
        mActive->LogComment(comment);

   if (mListeners)
    {
        PRInt32 i=0;
        for (; i < mListeners->Count(); i++) 
        {
            nsIXPIListener* element = (nsIXPIListener*)mListeners->ElementAt(i);
            if (element != NULL)
                element->LogComment(comment);
        }
    }
   return NS_OK;
}

