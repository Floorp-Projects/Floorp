/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __nznetworkzonepolicy_h__
#define __nznetworkzonepolicy_h__

#include "nsCOMPtr.h"
#include "mozilla/StaticPtr.h"
#include "nsINetworkZonePolicy.h"
#include "nsIObserver.h"

class nsILoadGroup;

namespace mozilla
{
namespace net
{

/**
 * class nsNetworkZonePolicy
 *
 * Implements nsINetworkZonePolicy: used by nsIRequest objects to check if
 * they have permission to load from private, RFC1918-like IP addresses.
 * See nsINetworkZonePolicy for more info.
 */
class nsNetworkZonePolicy : public nsINetworkZonePolicy
                          , public nsIObserver
{
public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSINETWORKZONEPOLICY
  NS_DECL_NSIOBSERVER

  static already_AddRefed<nsNetworkZonePolicy> GetSingleton();

private:
  nsNetworkZonePolicy();
  virtual ~nsNetworkZonePolicy();

  // Returns the parent loadgroup of aLoadGroup, or nullptr if none exists.
  already_AddRefed<nsILoadGroup> GetLoadGroupParent(nsILoadGroup *aLoadGroup);

  // Returns the owning loadgroup of aLoadGroup, or nullptr if none exists.
  already_AddRefed<nsILoadGroup> GetOwningLoadGroup(nsILoadGroup *aLoadGroup);

  // Returns the loadgroup of the parent docshell of aLoadGroup's docshell, or
  // nullptr if none exists.
  already_AddRefed<nsILoadGroup>
    GetParentDocShellsLoadGroup(nsILoadGroup *aLoadGroup);

  // Checks aLoadGroup and its ancestors (parent-, owning- and docshell
  // parent's loadgroups) to check for permission to load from private IP
  // addresses. The function follows the ancestor hierarchy to the root
  // loadgroup, or until a loadgroup is forbidden to load from private
  // networks. In this way, the loadgroup and all of its ancestor loadgroups
  // must have permission for this function to return true.
  bool CheckLoadGroupHierarchy(nsILoadGroup *aLoadGroup);

  // Similar to CheckLoadGroupHierarchy, except this function checks the
  // ancestor hierarchy only for permission to load from private networks;
  // aLoadGroup is not checked.
  bool CheckLoadGroupAncestorHierarchies(nsILoadGroup *aLoadGroup);

  // Keeps track of whether or not NZP is enabled.
  static bool sNZPEnabled;

  // True if shutdown notification has been received.
  static bool sShutdown;

  // Singleton pointer.
  static StaticRefPtr<nsNetworkZonePolicy> sSingleton;
};

} // namespace net
} // namespace mozilla

#endif /* __nznetworkzonepolicy_h__ */
