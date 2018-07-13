/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_antitrackingservice_h
#define mozilla_antitrackingservice_h

#include "nsString.h"

class nsIHttpChannel;
class nsIPrincipal;
class nsIURI;
class nsPIDOMWindowInner;

namespace mozilla {

class AntiTrackingCommon final
{
public:
  // This method returns true if the URI has first party storage access when
  // loaded inside the passed 3rd party context tracking resource window.
  static bool
  IsFirstPartyStorageAccessGrantedFor(nsPIDOMWindowInner* a3rdPartyTrackingWindow,
                                      nsIURI* aURI);

  // This can be called only if the a3rdPartyTrackingChannel is _really_ a 3rd
  // party context and marked as tracking resource.
  // It returns true if the URI has access to the first party storage.
  static bool
  IsFirstPartyStorageAccessGrantedFor(nsIHttpChannel* a3rdPartyTrackingChannel,
                                      nsIURI* aURI);

  // Grant the permission for aOrigin to have access to the first party storage
  // when loaded in that particular 3rd party context.
  static void
  AddFirstPartyStorageAccessGrantedFor(const nsAString& aOrigin,
                                       nsPIDOMWindowInner* a3rdPartyTrackingWindow);

  // For IPC only.
  static void
  SaveFirstPartyStorageAccessGrantedForOriginOnParentProcess(nsIPrincipal* aPrincipal,
                                                             const nsCString& aParentOrigin,
                                                             const nsCString& aGrantedOrigin);

};

} // namespace mozilla

#endif // mozilla_antitrackingservice_h
