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

//Note eventually this file moves to plughttp.dll. post DP work.
//
#include "nsHTTPInstance.h"

nsHTTPInstance::nsHTTPInstance(nsICoolURL* i_URL):
    m_pURL(i_URL),
    m_bConnected(PR_FALSE),
    mRefCnt(0)
{
    //TODO think if we need to make a copy of the URL and keep it here
    //since it might get deleted off the creators thread. And the
    //stream listener could be elsewhere...
}

nsHTTPInstance::~nsHTTPInstance()
{
    //TODO if we keep our copy of m_pURL, then delete it too.
}

NS_IMPL_ADDREF(nsHTTPInstance);

NS_METHOD
nsHTTPInstance::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    *aInstancePtr = NULL;
    
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

    if (aIID.Equals(nsIProtocolInstance::GetIID())) {
        *aInstancePtr = (void*) ((nsIProtocolInstance*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(nsIHTTPInstance::GetIID())) {
        *aInstancePtr = (void*) ((nsIHTTPInstance*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports*)this);
        NS_ADDREF_THIS();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}
 
NS_IMPL_RELEASE(nsHTTPInstance);

NS_METHOD
nsHTTPInstance::GetInputStream(nsIInputStream* *o_Stream)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsHTTPInstance::Interrupt(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsHTTPInstance::Load(void)
{
    if (m_bConnected) 
        return NS_ERROR_ALREADY_CONNECTED;



    m_bConnected = PR_TRUE;

    return NS_ERROR_NOT_IMPLEMENTED;
}

//nsIHTTPInstance functions now...

NS_METHOD
nsHTTPInstance::SetAccept(const char* i_AcceptHeader)
{
    if (m_bConnected) 
        return NS_ERROR_ALREADY_CONNECTED;
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsHTTPInstance::SetCookie(const char* i_Cookie)
{
    if (m_bConnected) 
        return NS_ERROR_ALREADY_CONNECTED;
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsHTTPInstance::SetUserAgent(const char* i_UserAgent)
{
    if (m_bConnected) 
        return NS_ERROR_ALREADY_CONNECTED;
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsHTTPInstance::SetHTTPVersion(HTTPVersion i_Version)
{
    if (m_bConnected) 
        return NS_ERROR_ALREADY_CONNECTED;
    return NS_ERROR_NOT_IMPLEMENTED;
}

PRInt32
nsHTTPInstance::GetContentLength(void) const
{
    if (!m_bConnected) 
        ((nsHTTPInstance*)this)->Load();

    return -1;
}

NS_METHOD
nsHTTPInstance::GetContentType(const char* *o_Type) const
{
    if (!m_bConnected) 
        ((nsHTTPInstance*)this)->Load();
    return NS_ERROR_NOT_IMPLEMENTED;
}

PRInt32
nsHTTPInstance::GetResponseStatus(void) const
{
    if (!m_bConnected) 
        ((nsHTTPInstance*)this)->Load();
    return -1;
}

NS_METHOD
nsHTTPInstance::GetResponseString(const char* *o_String) const
{
    if (!m_bConnected) 
        ((nsHTTPInstance*)this)->Load();
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_METHOD
nsHTTPInstance::GetServer(const char* *o_String) const
{
    if (!m_bConnected) 
        ((nsHTTPInstance*)this)->Load();
    return NS_ERROR_NOT_IMPLEMENTED;
}

