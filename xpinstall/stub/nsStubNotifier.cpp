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
 *     Daniel Veditz <dveditz@netscape.com>
 */

#include "nsString.h"
#include "nsStubNotifier.h"


nsStubNotifier::nsStubNotifier( pfnXPIStart aStart, 
                                pfnXPIProgress aProgress, 
                                pfnXPIFinal aFinal)
    : m_start(aStart), m_progress(aProgress), m_final(aFinal)
{
    NS_INIT_ISUPPORTS();
}

nsStubNotifier::~nsStubNotifier()
{}

NS_IMPL_ISUPPORTS(nsStubNotifier, NS_GET_IID(nsIXPINotifier));


NS_IMETHODIMP
nsStubNotifier::BeforeJavascriptEvaluation(const PRUnichar *URL)
{
    // we're not interested in this one
    return NS_OK;
}

NS_IMETHODIMP
nsStubNotifier::AfterJavascriptEvaluation(const PRUnichar *URL)
{
    // we're not interested in this one
    return NS_OK;
}

NS_IMETHODIMP
nsStubNotifier::InstallStarted(const PRUnichar *URL, const PRUnichar* UIPackageName)
{
    if (m_start)
        m_start(nsCAutoString(URL), nsCAutoString(UIPackageName));
    return NS_OK;
}

NS_IMETHODIMP
nsStubNotifier::ItemScheduled(const PRUnichar* message )
{
    if (m_progress)
        m_progress( nsCAutoString(message), 0, 0 );
    return NS_OK;
}

NS_IMETHODIMP
nsStubNotifier::FinalizeProgress(const PRUnichar* message, PRInt32 itemNum, PRInt32 totNum )
{
    if (m_progress)
        m_progress( nsCAutoString(message), itemNum, totNum );
    return NS_OK;
}

NS_IMETHODIMP
nsStubNotifier::FinalStatus(const PRUnichar *URL, PRInt32 status)
{
    if (m_final)
        m_final( nsCAutoString(URL), status );

    return NS_OK;
}

NS_IMETHODIMP
nsStubNotifier::LogComment(const PRUnichar* comment)
{
    // we're not interested in this one
    return NS_OK;
}
