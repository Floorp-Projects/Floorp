/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsHTTPHandler.h"
#include "nsHTTPInstance.h"
#include "nsITimer.h" 
#include "nsIProxy.h"
#include "plstr.h" // For PL_strcasecmp maybe DEBUG only... TODO check

NS_METHOD CreateOrGetHTTPHandler(nsIHTTPHandler* *o_HTTPHandler)
{
    *o_HTTPHandler = nsHTTPHandler::GetInstance();
    return NS_OK;
}

nsHTTPHandler::nsHTTPHandler():
    m_pTimer(nsnull),
    mRefCnt(0)
{
}

nsHTTPHandler::~nsHTTPHandler()
{
    NS_IF_RELEASE(m_pTimer);
}

NS_IMPL_ADDREF(nsHTTPHandler);

nsresult
nsHTTPHandler::GetProtocolInstance(
    nsICoolURL* i_URL, 
    nsIProtocolInstance* *o_Instance)
{
    //Assert that iURL's scheme is HTTP
    //This should only happen in debug checks... TODO
    const char* scheme = 0;
    if (i_URL)
    {
        i_URL->GetScheme(&scheme);
        if (0 == PL_strcasecmp(scheme, "http"))
        {
            //Check to see if an instance already exists in the active list
            // if...
            // else
            // Create one
            nsHTTPInstance* pNewInstance = new nsHTTPInstance(i_URL);
            if (pNewInstance)
            {
                *o_Instance = pNewInstance;
                return NS_OK;
            }
            else
                return NS_ERROR_OUT_OF_MEMORY;
        }
        //return NS_ERROR_MISMATCHED_URL;
    }
    return NS_ERROR_NULL_POINTER;
}

nsresult
nsHTTPHandler::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = NULL;
    
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

    if (aIID.Equals(nsIProtocolHandler::GetIID())) {
        *aInstancePtr = (void*) ((nsIProtocolHandler*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIHTTPHandler::GetIID())) {
        *aInstancePtr = (void*) ((nsIHTTPHandler*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIProxy::GetIID())) {
        *aInstancePtr = (void*) ((nsIProxy*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports*)(nsIProtocolHandler*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}
 
NS_IMPL_RELEASE(nsHTTPHandler);
