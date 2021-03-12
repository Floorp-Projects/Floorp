/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_antitrackingredirectheuristic_h
#define mozilla_antitrackingredirectheuristic_h

class nsIChannel;
class nsIURI;

namespace mozilla {

// This function will be called when we know we are about to perform a http
// redirect. It will check if we need to perform the AntiTracking redirect
// heuristic from the old channel perspective. We cannot know the classification
// flags of the new channel at the point. So, we will save the result in the
// loadInfo in order to finish the heuristic once the classification flags is
// ready.
void PrepareForAntiTrackingRedirectHeuristic(nsIChannel* aOldChannel,
                                             nsIURI* aOldURI,
                                             nsIChannel* aNewChannel,
                                             nsIURI* aNewURI);

// This function will be called once the classification flags of the new channel
// is known. It will check and perform the AntiTracking redirect heuristic
// according to the flags and the result from previous preparation.
void FinishAntiTrackingRedirectHeuristic(nsIChannel* aNewChannel,
                                         nsIURI* aNewURI);

}  // namespace mozilla

#endif  // mozilla_antitrackingredirectheuristic_h
