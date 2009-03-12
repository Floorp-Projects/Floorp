/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 3 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 *   Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsProxyRelease_h__
#define nsProxyRelease_h__

#include "nsIEventTarget.h"
#include "nsCOMPtr.h"

#ifdef XPCOM_GLUE_AVOID_NSPR
#error NS_ProxyRelease implementation depends on NSPR.
#endif

/**
 * Ensure that a nsCOMPtr is released on the target thread.
 *
 * @see NS_ProxyRelease(nsIEventTarget*, nsISupports*, PRBool)
 */
template <class T>
inline NS_HIDDEN_(nsresult)
NS_ProxyRelease
    (nsIEventTarget *target, nsCOMPtr<T> &doomed, PRBool alwaysProxy=PR_FALSE)
{
   T* raw = nsnull;
   doomed.swap(raw);
   return NS_ProxyRelease(target, raw, alwaysProxy);
}

/**
 * Ensures that the delete of a nsISupports object occurs on the target thread.
 *
 * @param target
 *        the target thread where the doomed object should be released.
 * @param doomed
 *        the doomed object; the object to be released on the target thread.
 * @param alwaysProxy
 *        normally, if NS_ProxyRelease is called on the target thread, then the
 *        doomed object will released directly.  however, if this parameter is
 *        true, then an event will always be posted to the target thread for
 *        asynchronous release.
 */
NS_COM_GLUE nsresult
NS_ProxyRelease
    (nsIEventTarget *target, nsISupports *doomed, PRBool alwaysProxy=PR_FALSE);

#endif
