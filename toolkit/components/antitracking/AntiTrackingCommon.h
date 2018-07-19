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
  // If the window is first party context, please use
  // MaybeIsFirstPartyStorageAccessGrantedFor();
  static bool
  IsFirstPartyStorageAccessGrantedFor(nsPIDOMWindowInner* a3rdPartyTrackingWindow,
                                      nsIURI* aURI);

  // Note: you should use IsFirstPartyStorageAccessGrantedFor() passing the
  // nsIHttpChannel! Use this method _only_ if the channel is not available.
  // For first party window, it's impossible to know if the aURI is a tracking
  // resource synchronously, so here we return the best guest: if we are sure
  // that the permission is granted for the origin of aURI, this method returns
  // true, otherwise false.
  static bool
  MaybeIsFirstPartyStorageAccessGrantedFor(nsPIDOMWindowInner* aFirstPartyWindow,
                                           nsIURI* aURI);

  // This can be called only if the a3rdPartyTrackingChannel is _really_ a 3rd
  // party context and marked as tracking resource.
  // It returns true if the URI has access to the first party storage.
  static bool
  IsFirstPartyStorageAccessGrantedFor(nsIHttpChannel* a3rdPartyTrackingChannel,
                                      nsIURI* aURI);

  // Grant the permission for aOrigin to have access to the first party storage.
  // This method can handle 2 different scenarios:
  // - aParentWindow is a 3rd party context, it opens an aOrigin window and the
  //   user interacts with it. We want to grant the permission at the
  //   combination: top-level + aParentWindow + aOrigin.
  //   Ex: example.net loads an iframe tracker.com, which opens a popup
  //   tracker.prg and the user interacts with it. tracker.org is allowed if
  //   loaded by tracker.com when loaded by example.net.
  // - aParentWindow is a first party context and a 3rd party resource (probably
  //   becuase of a script) opens a popup and the user interacts with it. We
  //   want to grant the permission for the 3rd party context to have access to
  //   the first party stoage when loaded in aParentWindow.
  //   Ex: example.net import tracker.com/script.js which does opens a popup and
  //   the user interacts with it. tracker.com is allowed when loaded by
  //   example.net.
  static void
  AddFirstPartyStorageAccessGrantedFor(const nsAString& aOrigin,
                                       nsPIDOMWindowInner* aParentWindow);

  // For IPC only.
  static void
  SaveFirstPartyStorageAccessGrantedForOriginOnParentProcess(nsIPrincipal* aPrincipal,
                                                             const nsCString& aParentOrigin,
                                                             const nsCString& aGrantedOrigin);

};

} // namespace mozilla

#endif // mozilla_antitrackingservice_h
