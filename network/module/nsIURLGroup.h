/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */

#ifndef nsIURLGroup_h__
#define nsIURLGroup_h__

#include "nsISupports.h"

class nsString;
class nsIURL;
class nsIInputStream;
class nsIStreamListener;
class nsILoadAttribs;

/* {a4071430-5263-11d2-929b-00105a1b0d64} */
#define NS_IURLGROUP_IID      \
{ 0xa4071430, 0x5263, 0x11d2, \
  {0x92, 0x9b, 0x00, 0x10, 0x5a, 0x1b, 0x0d, 0x64} }

class nsIURLGroup : public nsISupports
{
public:
    /**
     * Create a new URL, setting its LoadAttributes to the default
     * LoadAttributes for the group.
     *
     * If aBaseURL is non-null then aSpec is interpreted as relative to
     * aBaseURL.
     *
     * @param aInstancePtrResult    Newly created URL.
     * @param aBaseURL              An existing URL to be treated as a base
     * @param aSpec                 String specification of the URL
     * @param aContainer            An ISupports interface associated with the
     *                              URL.
     * @return  Returns NS_OK if successful, otherwise NS_ERROR_FAILURE...
     */
    NS_IMETHOD CreateURL(nsIURL** aInstancePtrResult, 
                         nsIURL* aBaseURL,
                         const nsString& aSpec,
                         nsISupports* aContainer) = 0;

    /**
     * Initiate an asynchronous URL load...
     *
     * @param aUrl      The URL to be loaded.
     * @param aConsumer The nsIStreamListener where the data and loading
     *                  notifications will be sent.
     *
     * @return  Returns NS_OK if successful, otherwise NS_ERROR_FAILURE.
     */
    NS_IMETHOD OpenStream(nsIURL *aUrl, 
                          nsIStreamListener *aConsumer) = 0;

    /**
     * Get the nsILoadAttributes for this nsIURLGroup and its children...
     *
     * @param aLoadAttribs  Result parameter containing the nsILoadAttributes
     *                      instance.
     *
     * @return  Returns NS_OK if successful, otherwise NS_ERROR_FAILURE.
     */
    NS_IMETHOD GetDefaultLoadAttributes(nsILoadAttribs*& aLoadAttribs) = 0;

    /**
     * Set the nsILoadAttributes for this nsIURLGroup and its children...
     *
     * @param aLoadAttribs  nsILoadAttributes instance to be used by this
     *                      nsIURLGroup and its children.
     *
     * @return  Returns NS_OK if successful, otherwise NS_ERROR_FAILURE.
     */
    NS_IMETHOD SetDefaultLoadAttributes(nsILoadAttribs*  aLoadAttribs) = 0;

    /**
     * Add a child to an nsIURLGroup.
     *
     * @param aGroup    The nsIURLGroup to be added as a child.  When the
     *                  child is added it will receive the nsILoadAttribs
     *                  of its parent...
     *
     * @return  Returns NS_OK if successful, otherwise NS_ERROR_FAILURE.
     */
    NS_IMETHOD AddChildGroup(nsIURLGroup* aGroup) = 0;

    /**
     * Remove a child from an nsIURLGroup.
     *
     * @param aGroup    The nsIURLGroup to be removed.
     *
     * @return  Returns NS_OK if successful, otherwise NS_ERROR_FAILURE.
     */
    NS_IMETHOD RemoveChildGroup(nsIURLGroup* aGroup) = 0;
};

#endif /* nsIURLGroup_h__ */
