/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et ft=cpp : */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at:
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Code.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Justin Lebar <justin.lebar@gmail.com>
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

#ifndef mozilla_ClearOnShutdown_h

#include <nsAutoPtr.h>
#include <nsCOMPtr.h>
#include <nsIObserver.h>
#include <nsIObserverService.h>
#include <mozilla/Services.h>

/*
 * This header exports one method in the mozilla namespace:
 *
 *   template<class SmartPtr>
 *   void ClearOnShutdown(SmartPtr *aPtr)
 *
 * This function takes a pointer to a smart pointer (i.e., nsCOMPtr<T>*,
 * nsRefPtr<T>*, or nsAutoPtr<T>*) and nulls the smart pointer on shutdown.
 *
 * This is useful if you have a global smart pointer object which you don't
 * want to "leak" on shutdown.
 *
 * There is no way to undo a call to ClearOnShutdown, so you can call it only
 * on smart pointers which you know will live until the program shuts down.
 */

namespace mozilla {
namespace ClearOnShutdown_Internal {

template<class SmartPtr>
class ShutdownObserver : public nsIObserver
{
public:
  ShutdownObserver(SmartPtr *aPtr)
    : mPtr(aPtr)
  {}

  virtual ~ShutdownObserver()
  {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD Observe(nsISupports *aSubject, const char *aTopic,
                     const PRUnichar *aData)
  {
    MOZ_ASSERT(strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0);

    if (mPtr) {
      *mPtr = NULL;
    }

    return NS_OK;
  }

private:
  SmartPtr *mPtr;
};

// Give the full namespace in the NS_IMPL macros because NS_IMPL_ADDREF/RELEASE
// stringify the class name (using the "#" operator) and use this in
// trace-malloc.  If we didn't fully-qualify the class name and someone else
// had a refcounted class named "ShutdownObserver<SmartPtr>" in any namespace,
// trace-malloc would assert (bug 711602).
//
// (Note that because macros happen before templates, trace-malloc sees this
// class name as "ShutdownObserver<SmartPtr>"; therefore, it would also assert
// if ShutdownObserver<T> had a different size than ShutdownObserver<S>.)

template<class SmartPtr>
NS_IMPL_ADDREF(mozilla::ClearOnShutdown_Internal::
                 ShutdownObserver<SmartPtr>)

template<class SmartPtr>
NS_IMPL_RELEASE(mozilla::ClearOnShutdown_Internal::
                  ShutdownObserver<SmartPtr>)

template<class SmartPtr>
NS_IMPL_QUERY_INTERFACE1(mozilla::ClearOnShutdown_Internal::
                           ShutdownObserver<SmartPtr>,
                         nsIObserver)

} // namespace ClearOnShutdown_Internal

template<class SmartPtr>
void ClearOnShutdown(SmartPtr *aPtr)
{
  nsRefPtr<ClearOnShutdown_Internal::ShutdownObserver<SmartPtr> > observer =
    new ClearOnShutdown_Internal::ShutdownObserver<SmartPtr>(aPtr);

  nsCOMPtr<nsIObserverService> os = services::GetObserverService();
  if (!os) {
    NS_WARNING("Could not get observer service!");
    return;
  }
  os->AddObserver(observer, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
}

} // namespace mozilla

#endif
