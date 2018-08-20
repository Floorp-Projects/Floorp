/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

function PrivateBrowsingTrackingProtectionWhitelist() {
  Services.obs.addObserver(this, "last-pb-context-exited", true);
}

PrivateBrowsingTrackingProtectionWhitelist.prototype = {
  classID: Components.ID("{a319b616-c45d-4037-8d86-01c592b5a9af}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIPrivateBrowsingTrackingProtectionWhitelist, Ci.nsIObserver, Ci.nsISupportsWeakReference]),
  _xpcom_factory: XPCOMUtils.generateSingletonFactory(PrivateBrowsingTrackingProtectionWhitelist),

  /**
   * Add the provided URI to the list of allowed tracking sites.
   *
   * @param uri nsIURI
   *        The URI to add to the list.
   */
  addToAllowList(uri) {
    Services.perms.add(uri, "trackingprotection-pb", Ci.nsIPermissionManager.ALLOW_ACTION,
                       Ci.nsIPermissionManager.EXPIRE_SESSION);
  },

  /**
   * Remove the provided URI from the list of allowed tracking sites.
   *
   * @param uri nsIURI
   *        The URI to add to the list.
   */
  removeFromAllowList(uri) {
    Services.perms.remove(uri, "trackingprotection-pb");
  },

  /**
   * Check if the provided URI exists in the list of allowed tracking sites.
   *
   * @param uri nsIURI
   *        The URI to add to the list.
   */
  existsInAllowList(uri) {
    return Services.perms.testPermission(uri, "trackingprotection-pb") ==
           Ci.nsIPermissionManager.ALLOW_ACTION;
  },

  observe(subject, topic, data) {
    if (topic == "last-pb-context-exited") {
      Services.perms.removeByType("trackingprotection-pb");
    }
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PrivateBrowsingTrackingProtectionWhitelist]);
