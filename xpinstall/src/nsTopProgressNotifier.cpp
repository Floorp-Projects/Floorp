/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Douglas Turner <dougt@netscape.com>
 *     Daniel Veditz <dveditz@netscape.com>
 */

#include "nsIXPINotifier.h"
#include "nsTopProgressNotifier.h"

nsTopProgressNotifier::nsTopProgressNotifier()
{
    NS_INIT_ISUPPORTS();
    mNotifiers = new nsVoidArray();
    mActive = 0;
}

nsTopProgressNotifier::~nsTopProgressNotifier()
{
    if (mNotifiers)
    {
        PRInt32 i=0;
        for (; i < mNotifiers->Count(); i++) 
        {
            nsIXPINotifier* element = (nsIXPINotifier*)mNotifiers->ElementAt(i);
            delete element;
        }

        mNotifiers->Clear();
        delete (mNotifiers);
    }
}


NS_IMPL_ISUPPORTS(nsTopProgressNotifier, nsIXPINotifier::GetIID());


long
nsTopProgressNotifier::RegisterNotifier(nsIXPINotifier * newNotifier)
{
    NS_IF_ADDREF( newNotifier );
    return mNotifiers->AppendElement( newNotifier );
}


void
nsTopProgressNotifier::UnregisterNotifier(long id)
{
    nsIXPINotifier *item = (nsIXPINotifier*)mNotifiers->ElementAt(id);
    NS_IF_RELEASE(item);
    mNotifiers->ReplaceElementAt(nsnull, id);
}



NS_IMETHODIMP
nsTopProgressNotifier::BeforeJavascriptEvaluation(const PRUnichar *URL)
{
    if (mActive)
        mActive->BeforeJavascriptEvaluation(URL);

    if (mNotifiers)
    {
        PRInt32 i=0;
        for (; i < mNotifiers->Count(); i++) 
        {
            nsIXPINotifier* element = (nsIXPINotifier*)mNotifiers->ElementAt(i);
            if (element != NULL)
                element->BeforeJavascriptEvaluation(URL);
        }
    }
    
    return NS_OK;
}

NS_IMETHODIMP
nsTopProgressNotifier::AfterJavascriptEvaluation(const PRUnichar *URL)
{
    if (mActive)
        mActive->AfterJavascriptEvaluation(URL);

    if (mNotifiers)
    {
        PRInt32 i=0;
        for (; i < mNotifiers->Count(); i++) 
        {
            nsIXPINotifier* element = (nsIXPINotifier*)mNotifiers->ElementAt(i);
            if (element != NULL)
                element->AfterJavascriptEvaluation(URL);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsTopProgressNotifier::InstallStarted(const PRUnichar *URL, const PRUnichar* UIPackageName)
{
    if (mActive)
        mActive->InstallStarted(URL, UIPackageName);

    if (mNotifiers)
    {
        PRInt32 i=0;
        for (; i < mNotifiers->Count(); i++) 
        {
            nsIXPINotifier* element = (nsIXPINotifier*)mNotifiers->ElementAt(i);
            if (element != NULL)
                element->InstallStarted(URL, UIPackageName);
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsTopProgressNotifier::ItemScheduled( const PRUnichar* message )
{
    long rv = 0;

    if (mActive)
        mActive->ItemScheduled( message );

    if (mNotifiers)
    {
        PRInt32 i=0;
        for (; i < mNotifiers->Count(); i++) 
        {
            nsIXPINotifier* element = (nsIXPINotifier*)mNotifiers->ElementAt(i);
            if (element != NULL)
                element->ItemScheduled( message );
        }
    }

    return rv;
}

NS_IMETHODIMP
nsTopProgressNotifier::FinalizeProgress( const PRUnichar* message, PRInt32 itemNum, PRInt32 totNum )
{
    if (mActive)
        mActive->FinalizeProgress( message, itemNum, totNum );

    if (mNotifiers)
    {
        PRInt32 i=0;
        for (; i < mNotifiers->Count(); i++) 
        {
            nsIXPINotifier* element = (nsIXPINotifier*)mNotifiers->ElementAt(i);
            if (element != NULL)
                element->FinalizeProgress( message, itemNum, totNum );
        }
    }
    return NS_OK;
}

NS_IMETHODIMP
nsTopProgressNotifier::FinalStatus(const PRUnichar *URL, PRInt32 status)
{
    if (mActive)
        mActive->FinalStatus(URL, status);

    if (mNotifiers)
    {
        PRInt32 i=0;
        for (; i < mNotifiers->Count(); i++) 
        {
            nsIXPINotifier* element = (nsIXPINotifier*)mNotifiers->ElementAt(i);
            if (element != NULL)
                element->FinalStatus(URL,status);
        }
    }
   return NS_OK;
}

NS_IMETHODIMP
nsTopProgressNotifier::LogComment(const PRUnichar* comment)
{
    if (mActive)
        mActive->LogComment(comment);

   if (mNotifiers)
    {
        PRInt32 i=0;
        for (; i < mNotifiers->Count(); i++) 
        {
            nsIXPINotifier* element = (nsIXPINotifier*)mNotifiers->ElementAt(i);
            if (element != NULL)
                element->LogComment(comment);
        }
    }
   return NS_OK;
}

