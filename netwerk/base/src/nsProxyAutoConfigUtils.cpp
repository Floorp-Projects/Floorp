/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Andreas Otte.
 *
 * Contributor(s): 
 */
 
#include "nsProxyAutoConfigUtils.h"

nsProxyAutoConfigUtils::nsProxyAutoConfigUtils()
{
    NS_INIT_ISUPPORTS();
}

nsProxyAutoConfigUtils::~nsProxyAutoConfigUtils()
{
}

NS_IMPL_ISUPPORTS1(nsProxyAutoConfigUtils, nsIProxyAutoConfigUtils);

NS_IMETHODIMP
nsProxyAutoConfigUtils::IsResolvable(const char* aHost, PRBool *result)
{
    return NS_OK;
}

NS_IMETHODIMP
nsProxyAutoConfigUtils::IsInNet(const char* aHost, 
        const char* aPattern, 
        const char* aMask,
        PRBool *result)
{
    return NS_OK;
}

NS_IMETHODIMP
nsProxyAutoConfigUtils::DNSResolve(const char* aHost, char** aIPAddress)
{
    return NS_OK;
}

NS_IMETHODIMP
nsProxyAutoConfigUtils::MyIPAddress(char **aIPAddress)
{
    return NS_OK;
}

