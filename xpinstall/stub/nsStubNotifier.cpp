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
 *     Daniel Veditz <dveditz@netscape.com>
 */

#include "nsString.h"
#include "nsStubNotifier.h"

extern PRInt32 gInstallStatus;

nsStubListener::nsStubListener( pfnXPIProgress aProgress )
    : m_progress(aProgress)
{
    NS_INIT_ISUPPORTS();
}

nsStubListener::~nsStubListener()
{}

NS_IMPL_ISUPPORTS1(nsStubListener, nsIXPIListener)


NS_IMETHODIMP
nsStubListener::BeforeJavascriptEvaluation(const PRUnichar *URL)
{
    // we're not interested in this one
    return NS_OK;
}

NS_IMETHODIMP
nsStubListener::AfterJavascriptEvaluation(const PRUnichar *URL)
{
    // we're not interested in this one
    return NS_OK;
}

NS_IMETHODIMP
nsStubListener::InstallStarted(const PRUnichar *URL, const PRUnichar* UIPackageName)
{
    // we're not interested in this one
    return NS_OK;
}

NS_IMETHODIMP
nsStubListener::ItemScheduled(const PRUnichar* message )
{
    if (m_progress)
      {
        m_progress( NS_LossyConvertUCS2toASCII(message).get(), 0, 0 );
      }
    return NS_OK;
}

NS_IMETHODIMP
nsStubListener::FinalizeProgress(const PRUnichar* message, PRInt32 itemNum, PRInt32 totNum )
{
    if (m_progress)
      {
        m_progress( NS_LossyConvertUCS2toASCII(message).get(), itemNum, totNum );
      }
    return NS_OK;
}

NS_IMETHODIMP
nsStubListener::FinalStatus(const PRUnichar *URL, PRInt32 status)
{
//    if (m_final)
//        m_final( nsCAutoString(URL), status );
    gInstallStatus = status;
    return NS_OK;
}

NS_IMETHODIMP
nsStubListener::LogComment(const PRUnichar* comment)
{
    // we're not interested in this one
    return NS_OK;
}
