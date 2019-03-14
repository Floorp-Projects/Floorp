/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_CookieSettings_h
#define mozilla_net_CookieSettings_h

#include "nsICookieSettings.h"
#include "nsDataHashtable.h"

class nsIPermission;

namespace mozilla {
namespace net {

class CookieSettingsArgs;

/**
 * CookieSettings
 * ~~~~~~~~~~~~~~
 *
 * CookieSettings is a snapshot of cookie policy and cookie permissions in a
 * precise moment of time. This object is used by top-level documents to have a
 * consistent cookie configuration also in case the user changes it. New cookie
 * configurations will apply only to new top-level documents.
 *
 * CookieSettings creation
 * ~~~~~~~~~~~~~~~~~~~~~~~
 *
 * CookieSettings is created when the top-level document's nsIChannel's
 * nsILoadInfo is constructed. Any sub-resource and any sub-document inherits it
 * from that nsILoadInfo. Also dedicated workers and their resources inherit it
 * from the parent document.
 *
 * SharedWorkers and ServiceWorkers have their own CookieSettings because they
 * don't have a single parent document (SharedWorkers could have more than one,
 * ServiceWorkers have none).
 *
 * In Chrome code, we have a new CookieSettings when we download resources via
 * 'Save-as...' and we also have a new CookieSettings for favicon downloading.
 *
 * Content-scripts WebExtensions also have their own CookieSettings because they
 * don't have a direct access to the document they are running into.
 *
 * Anything else will have a special CookieSettings which blocks everything
 * (CookieSettings::CreateBlockingAll()) by forcing BEHAVIOR_REJECT as policy.
 * When this happens, that context will not have access to the cookie jar and no
 * cookies are sent or received.
 *
 * Propagation of CookieSettings
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * CookieSettings are shared inside the same top-level document via its
 * nsIChannel's nsILoadInfo.  This is done automatically if you pass a nsINode
 * to NS_NewChannel(), and it must be done manually if you use a different
 * channel constructor. For instance, this happens for any worker networking
 * operation.
 *
 * We use the same CookieSettings for any resource belonging to the top-level
 * document even if cross-origin. This makes the browser behave consistently a
 * scenario where A loads B which loads A again, and cookie policy/permission
 * changes in the meantime.
 *
 * Cookie Permissions propagation
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * CookieSettings populates the known cookie permissions only when required.
 * Initially the list is empty, but when CookieSettings::CookiePermission() is
 * called, the requested permission is stored in the internal list if it doesn't
 * exist yet.
 *
 * This is actually nice because it relies on the permission propagation from
 * parent to content process. No extra IPC is required.
 *
 * Note that we store permissions with UNKNOWN_ACTION values too because they
 * can be set after the loading of the top-level document and we don't want to
 * return a different value when this happens.
 *
 * Use of CookieSettings
 * ~~~~~~~~~~~~~~~~~~~~~
 *
 * In theory, there should not be direct access to cookie permissions or
 * cookieBehavior pref. Everything should pass through CookieSettings.
 *
 * A reference to CookieSettings can be obtained from
 * nsILoadInfo::GetCookieSettings(), from Document::CookieSettings() and from
 * the WorkerPrivate::CookieSettings().
 *
 * CookieSettings is thread-safe, but the permission list must be touched only
 * on the main-thread.
 *
 * Testing
 * ~~~~~~~
 *
 * If you need to test the changing of cookie policy or a cookie permission, you
 * need to workaround CookieSettings. This can be done opening a new window and
 * running the test into that new global.
 */

/**
 * Class that provides an nsICookieSettings implementation.
 */
class CookieSettings final : public nsICookieSettings {
 public:
  typedef nsTArray<RefPtr<nsIPermission>> CookiePermissionList;

  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICOOKIESETTINGS

  static already_AddRefed<nsICookieSettings> CreateBlockingAll();

  static already_AddRefed<nsICookieSettings> Create();

  void Serialize(CookieSettingsArgs& aData);

  static void Deserialize(const CookieSettingsArgs& aData,
                          nsICookieSettings** aCookieSettings);

  void Merge(const CookieSettingsArgs& aData);

  // We don't want to send this object from parent to child process if there are
  // no reasons. HasBeenChanged() returns true if the object has changed its
  // internal state and it must be sent beck to the content process.
  bool HasBeenChanged() const { return mToBeMerged; }

 private:
  enum State {
    // No cookie permissions are allowed to be stored in this object.
    eFixed,

    // Cookie permissions can be stored in case they are unknown when they are
    // asked or when they are sent from the parent process.
    eProgressive,
  };

  CookieSettings(uint32_t aCookieBehavior, State aState);
  ~CookieSettings();

  uint32_t mCookieBehavior;
  CookiePermissionList mCookiePermissions;

  State mState;

  bool mToBeMerged;
};

}  // namespace net
}  // namespace mozilla

#endif  // mozilla_net_CookieSettings_h
