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

NS_IMPL_ISUPPORTS(nsStubNotifier, NS_IXPINOTIFIER_IID);


NS_IMETHODIMP
nsStubNotifier::BeforeJavascriptEvaluation(void)
{
    // we're not interested in this one
    return NS_OK;
}

NS_IMETHODIMP
nsStubNotifier::AfterJavascriptEvaluation(void)
{
    // we're not interested in this one
    return NS_OK;
}

NS_IMETHODIMP
nsStubNotifier::InstallStarted(const char* UIPackageName)
{
    if (m_start)
        m_start(UIPackageName);
    return NS_OK;
}

NS_IMETHODIMP
nsStubNotifier::ItemScheduled(const char* message )
{
    if (m_progress)
        return m_progress( message, 0, 0 );
    else
        return NS_OK;
}

NS_IMETHODIMP
nsStubNotifier::InstallFinalization(const char* message, PRInt32 itemNum, PRInt32 totNum )
{
    if (m_progress)
        return m_progress( message, itemNum, totNum );
    else
        return NS_OK;
}

NS_IMETHODIMP
nsStubNotifier::InstallAborted(void)
{
    // XXX need to hook this one to m_final
    return NS_OK;
}

NS_IMETHODIMP
nsStubNotifier::LogComment(const char* comment)
{
    // we're not interested in this one
    return NS_OK;
}
