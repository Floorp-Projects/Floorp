/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieJarSettings_h
#define mozilla_net_CookieJarSettings_h

#include "nsICookieJarSettings.h"
#include "nsDataHashtable.h"
#include "nsTArray.h"

class nsIPermission;

namespace mozilla {
namespace net {

class CookieJarSettingsArgs;

/**
 * CookieJarSettings
 * ~~~~~~~~~~~~~~
 *
 * CookieJarSettings is a snapshot of the cookie jar's configurations in a
 * precise moment of time, such as the cookie policy and cookie permissions.
 * This object is used by top-level documents to have a consistent cookie jar
 * configuration also in case the user changes it. New configurations will apply
 * only to new top-level documents.
 *
 * CookieJarSettings creation
 * ~~~~~~~~~~~~~~~~~~~~~~~
 *
 * CookieJarSettings is created when the top-level document's nsIChannel's
 * nsILoadInfo is constructed. Any sub-resource and any sub-document inherits it
 * from that nsILoadInfo. Also dedicated workers and their resources inherit it
 * from the parent document.
 *
 * SharedWorkers and ServiceWorkers have their own CookieJarSettings because
 * they don't have a single parent document (SharedWorkers could have more than
 * one, ServiceWorkers have none).
 *
 * In Chrome code, we have a new CookieJarSettings when we download resources
 * via 'Save-as...' and we also have a new CookieJarSettings for favicon
 * downloading.
 *
 * Content-scripts WebExtensions also have their own CookieJarSettings because
 * they don't have a direct access to the document they are running into.
 *
 * Anything else will have a special CookieJarSettings which blocks everything
 * (CookieJarSettings::GetBlockingAll()) by forcing BEHAVIOR_REJECT as policy.
 * When this happens, that context will not have access to the cookie jar and no
 * cookies are sent or received.
 *
 * Propagation of CookieJarSettings
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * CookieJarSettings are shared inside the same top-level document via its
 * nsIChannel's nsILoadInfo.  This is done automatically if you pass a nsINode
 * to NS_NewChannel(), and it must be done manually if you use a different
 * channel constructor. For instance, this happens for any worker networking
 * operation.
 *
 * We use the same CookieJarSettings for any resource belonging to the top-level
 * document even if cross-origin. This makes the browser behave consistently a
 * scenario where A loads B which loads A again, and cookie policy/permission
 * changes in the meantime.
 *
 * Cookie Permissions propagation
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * CookieJarSettings populates the known cookie permissions only when required.
 * Initially the list is empty, but when CookieJarSettings::CookiePermission()
 * is called, the requested permission is stored in the internal list if it
 * doesn't exist yet.
 *
 * This is actually nice because it relies on the permission propagation from
 * parent to content process. No extra IPC is required.
 *
 * Note that we store permissions with UNKNOWN_ACTION values too because they
 * can be set after the loading of the top-level document and we don't want to
 * return a different value when this happens.
 *
 * Use of CookieJarSettings
 * ~~~~~~~~~~~~~~~~~~~~~
 *
 * In theory, there should not be direct access to cookie permissions or
 * cookieBehavior pref. Everything should pass through CookieJarSettings.
 *
 * A reference to CookieJarSettings can be obtained from
 * nsILoadInfo::GetCookieJarSettings(), from Document::CookieJarSettings() and
 * from the WorkerPrivate::CookieJarSettings().
 *
 * CookieJarSettings is thread-safe, but the permission list must be touched
 * only on the main-thread.
 *
 * Testing
 * ~~~~~~~
 *
 * If you need to test the changing of cookie policy or a cookie permission, you
 * need to workaround CookieJarSettings. This can be done opening a new window
 * and running the test into that new global.
 */

/**
 * Class that provides an nsICookieJarSettings implementation.
 */
class CookieJarSettings final : public nsICookieJarSettings {
 public:
  typedef nsTArray<RefPtr<nsIPermission>> CookiePermissionList;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICOOKIEJARSETTINGS

  static already_AddRefed<nsICookieJarSettings> GetBlockingAll();

  static already_AddRefed<nsICookieJarSettings> Create();

  static already_AddRefed<nsICookieJarSettings> Create(
      uint32_t aCookieBehavior);

  static CookieJarSettings* Cast(nsICookieJarSettings* aCS) {
    return static_cast<CookieJarSettings*>(aCS);
  }

  void Serialize(CookieJarSettingsArgs& aData);

  static void Deserialize(const CookieJarSettingsArgs& aData,
                          nsICookieJarSettings** aCookieJarSettings);

  void Merge(const CookieJarSettingsArgs& aData);

  // We don't want to send this object from parent to child process if there are
  // no reasons. HasBeenChanged() returns true if the object has changed its
  // internal state and it must be sent beck to the content process.
  bool HasBeenChanged() const { return mToBeMerged; }

  void UpdateIsOnContentBlockingAllowList(nsIChannel* aChannel);

  // Utility function to test if the passed cookiebahvior is
  // BEHAVIOR_REJECT_TRACKER, BEHAVIOR_REJECT_TRACKER_AND_PARTITION_FOREIGN or
  // BEHAVIOR_REJECT_FOREIGN when
  // network.cookie.rejectForeignWithExceptions.enabled pref is set to true.
  static bool IsRejectThirdPartyContexts(uint32_t aCookieBehavior);

  // This static method returns true if aCookieBehavior is
  // BEHAVIOR_REJECT_FOREIGN and
  // network.cookie.rejectForeignWithExceptions.enabled pref is set to true.
  static bool IsRejectThirdPartyWithExceptions(uint32_t aCookieBehavior);

 private:
  enum State {
    // No cookie permissions are allowed to be stored in this object.
    eFixed,

    // Cookie permissions can be stored in case they are unknown when they are
    // asked or when they are sent from the parent process.
    eProgressive,
  };

  CookieJarSettings(uint32_t aCookieBehavior, State aState);
  ~CookieJarSettings();

  uint32_t mCookieBehavior;
  CookiePermissionList mCookiePermissions;
  bool mIsOnContentBlockingAllowList;

  State mState;

  bool mToBeMerged;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieJarSettings_h
