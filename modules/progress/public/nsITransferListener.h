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

#ifndef nsITransferListener_h__
#define nsITransferListener_h__

#include "nsISupports.h"
#include "net.h" // for URL_Struct


// {663f0fa1-edfe-11d9-8031-006008159b5a}
#define NS_ITRANSFERLISTENER_IID \
{0x663f0fa1, 0xedfe, 0x11d9,  \
    {0x80, 0x31, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x5a}}

/**
 * This interface is <i>almost</i> identical to the <tt>nsIStreamListener</tt>
 * interface. The reason that I don't just use that interface is that it's
 * a little bit too heavy-weight right now: it brings in <tt>nsIURL</tt> and
 * <tt>nsString</tt>, which in turn bring in a bunch of stuff that just isn't
 * really running in Mozilla yet.
 *
 * <p>The interface defines an object to which a stream or "transfer"
 * can report its status.
 */
class nsITransferListener : public nsISupports
{
public:
    /**
     * Notify the observer that the URL has started to load.  This method is
     * called only once, at the beginning of a URL load.
     */
    NS_IMETHOD
    OnStartBinding(const URL_Struct* url) = 0;

    /**
     * Notify the observer that progress as occurred for the URL load.<BR>
     */
    NS_IMETHOD
    OnProgress(const URL_Struct* url, PRUint32 bytesReceived, PRUint32 contentLength) = 0;

    /**
     * Notify the observer with a status message for the URL load.<BR>
     */
    NS_IMETHOD
    OnStatus(const URL_Struct* url, const char* message) = 0;

    /**
     * Notify the observer that the transfer has been suspended.
     */
    NS_IMETHOD
    OnSuspend(const URL_Struct* url) = 0;

    /**
     * Notify the observer that the transfer has been resumed.
     */
    NS_IMETHOD
    OnResume(const URL_Struct* url) = 0;

    /**
     * Notify the observer that the URL has finished loading.  This method is 
     * called once when the networking library has finished processing the 
     * URL transaction.<BR><BR>
     * 
     * This method is called regardless of whether the URL loaded successfully.<BR><BR>
     * 
     * @param status    Status code for the URL load.
     * @param msg   A text string describing the error.
     * @return The return value is currently ignored.
     */
    NS_IMETHOD
    OnStopBinding(const URL_Struct* url, PRInt32 status, const char* message) = 0;
};


#endif /* nsITransferListener_h__ */
