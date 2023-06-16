/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_EarlyHintRegistrar_h__
#define mozilla_net_EarlyHintRegistrar_h__

#include "mozilla/RefCounted.h"
#include "mozilla/dom/ipc/IdType.h"
#include "nsRefPtrHashtable.h"
#include "mozilla/AlreadyAddRefed.h"

class nsIParentChannel;

namespace mozilla::net {

class EarlyHintPreloader;

/*
 * Registrar for pairing EarlyHintPreloader and HttpChannelParent via
 * earlyHintPreloaderId. EarlyHintPreloader has to be registered first.
 * EarlyHintPreloader::OnParentReady will be invoked to notify the
 * EarlyHintpreloader about the existence of the associated HttpChannelParent.
 */
class EarlyHintRegistrar final : public RefCounted<EarlyHintRegistrar> {
  using EarlyHintHashtable =
      nsRefPtrHashtable<nsUint64HashKey, EarlyHintPreloader>;

 public:
  MOZ_DECLARE_REFCOUNTED_TYPENAME(EarlyHintRegistrar)

  EarlyHintRegistrar();
  ~EarlyHintRegistrar();

  /*
   * Store EarlyHintPreloader for the HttpChannelParent to connect back to.
   *
   * @param aEarlyHintPreloaderId identifying the EarlyHintPreloader
   * @param aEhp the EarlyHintPreloader to store
   */
  void RegisterEarlyHint(uint64_t aEarlyHintPreloaderId,
                         EarlyHintPreloader* aEhp);

  /*
   * Link the provided nsIParentChannel with the associated EarlyHintPreloader.
   *
   * @param aEarlyHintPreloaderId identifying the EarlyHintPreloader
   * @param aParent the nsIParentChannel to be paired
   * @param aChannelId the id of the nsIChannel connecting to the
   *        EarlyHintPreloader.
   */
  bool LinkParentChannel(dom::ContentParentId aCpId,
                         uint64_t aEarlyHintPreloaderId,
                         nsIParentChannel* aParent);

  /*
   * Delete previous stored EarlyHintPreloader
   *
   * @param aEarlyHintPreloaderId identifying the EarlyHintPreloader
   */
  void DeleteEntry(dom::ContentParentId aCpId, uint64_t aEarlyHintPreloaderId);

  /*
   * This is called when "profile-change-net-teardown" is observed. We use this
   * to cancel the ongoing preload and clear the linked EarlyHintPreloader
   * objects.
   */
  static void CleanUp();

  // Singleton accessor
  static already_AddRefed<EarlyHintRegistrar> GetOrCreate();

 private:
  // Store unlinked EarlyHintPreloader objects.
  EarlyHintHashtable mEarlyHint;
};

}  // namespace mozilla::net

#endif  // mozilla_net_EarlyHintRegistrar_h__
