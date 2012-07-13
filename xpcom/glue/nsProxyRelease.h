/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 3 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsProxyRelease_h__
#define nsProxyRelease_h__

#include "nsIEventTarget.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"

#ifdef XPCOM_GLUE_AVOID_NSPR
#error NS_ProxyRelease implementation depends on NSPR.
#endif

/**
 * Ensure that a nsCOMPtr is released on the target thread.
 *
 * @see NS_ProxyRelease(nsIEventTarget*, nsISupports*, bool)
 */
template <class T>
inline NS_HIDDEN_(nsresult)
NS_ProxyRelease
    (nsIEventTarget *target, nsCOMPtr<T> &doomed, bool alwaysProxy=false)
{
   T* raw = nsnull;
   doomed.swap(raw);
   return NS_ProxyRelease(target, raw, alwaysProxy);
}

/**
 * Ensure that a nsRefPtr is released on the target thread.
 *
 * @see NS_ProxyRelease(nsIEventTarget*, nsISupports*, bool)
 */
template <class T>
inline NS_HIDDEN_(nsresult)
NS_ProxyRelease
    (nsIEventTarget *target, nsRefPtr<T> &doomed, bool alwaysProxy=false)
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
    (nsIEventTarget *target, nsISupports *doomed, bool alwaysProxy=false);

#endif
