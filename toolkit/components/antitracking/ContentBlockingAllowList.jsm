/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ContentBlockingAllowList"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm"
);

/**
 * A helper module to manage the Content Blocking Allow List.
 *
 * This module provides a couple of utility APIs for adding or
 * removing a given browser object to the Content Blocking allow
 * list.
 */
const ContentBlockingAllowList = {
  _observingLastPBContext: false,

  _maybeSetupLastPBContextObserver() {
    if (!this._observingLastPBContext) {
      this._observer = {
        QueryInterface: ChromeUtils.generateQI([
          "nsIObserver",
          "nsISupportsWeakReference",
        ]),

        observe(subject, topic, data) {
          if (topic == "last-pb-context-exited") {
            Services.perms.removeByType("trackingprotection-pb");
          }
        },
      };
      Services.obs.addObserver(this._observer, "last-pb-context-exited", true);
      this._observingLastPBContext = true;
    }
  },

  _basePrincipalForAntiTrackingCommon(browser) {
    let principal = browser.contentBlockingAllowListPrincipal;
    // We can only use content principals for this purpose.
    if (!principal || !principal.isContentPrincipal) {
      return null;
    }
    return principal;
  },

  _permissionTypeFor(browser) {
    return PrivateBrowsingUtils.isBrowserPrivate(browser)
      ? "trackingprotection-pb"
      : "trackingprotection";
  },

  _expiryFor(browser) {
    return PrivateBrowsingUtils.isBrowserPrivate(browser)
      ? Ci.nsIPermissionManager.EXPIRE_SESSION
      : Ci.nsIPermissionManager.EXPIRE_NEVER;
  },

  /**
   * Returns false if this module cannot handle the current document loaded in
   * the browser object.  This can happen for example for about: or file:
   * documents.
   */
  canHandle(browser) {
    return this._basePrincipalForAntiTrackingCommon(browser) != null;
  },

  /**
   * Add the given browser object to the Content Blocking allow list.
   */
  add(browser) {
    // Start observing PB last-context-exit notification to do the needed cleanup.
    this._maybeSetupLastPBContextObserver();

    let prin = this._basePrincipalForAntiTrackingCommon(browser);
    let type = this._permissionTypeFor(browser);
    let expire = this._expiryFor(browser);
    Services.perms.addFromPrincipal(
      prin,
      type,
      Services.perms.ALLOW_ACTION,
      expire
    );
  },

  /**
   * Remove the given browser object from the Content Blocking allow list.
   */
  remove(browser) {
    let prin = this._basePrincipalForAntiTrackingCommon(browser);
    let type = this._permissionTypeFor(browser);
    Services.perms.removeFromPrincipal(prin, type);
  },

  /**
   * Remove the given principal from the Content Blocking allow list if present.
   */
  removeByPrincipal(principal) {
    Services.perms.removeFromPrincipal(principal, "trackingprotection");
    Services.perms.removeFromPrincipal(principal, "trackingprotection-pb");
  },

  /**
   * Returns true if the current browser has loaded a document that is on the
   * Content Blocking allow list.
   */
  includes(browser) {
    let prin = this._basePrincipalForAntiTrackingCommon(browser);
    let type = this._permissionTypeFor(browser);
    return (
      Services.perms.testExactPermissionFromPrincipal(prin, type) ==
      Services.perms.ALLOW_ACTION
    );
  },

  /**
   * Returns a list of all non-private browsing principals that are on the
   * content blocking allow list.
   */
  getAllowListedPrincipals() {
    const exceptions = Services.perms
      .getAllWithTypePrefix("trackingprotection")
      .filter(
        // Only export non-private exceptions for security reasons.
        p => p.type == "trackingprotection"
      );
    return exceptions.map(e => e.principal);
  },

  /**
   * Takes a list of nsIPrincipals and uses it to update the content blocking allow
   * list.
   */
  addAllowListPrincipals(principals) {
    principals.forEach(p =>
      Services.perms.addFromPrincipal(
        p,
        "trackingprotection",
        Services.perms.ALLOW_ACTION,
        Ci.nsIPermissionManager.EXPIRE_SESSION
      )
    );
  },

  /**
   * Removes all content blocking exceptions.
   */
  wipeLists() {
    Services.perms.removeByType("trackingprotection");
    Services.perms.removeByType("trackingprotection-pb");
  },
};
