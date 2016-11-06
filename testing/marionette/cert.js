/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.EXPORTED_SYMBOLS = ["cert"];

const registrar =
    Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
const sss = Cc["@mozilla.org/ssservice;1"]
    .getService(Ci.nsISiteSecurityService);

const CONTRACT_ID = "@mozilla.org/security/certoverride;1";
const CERT_PINNING_ENFORCEMENT_PREF =
    "security.cert_pinning.enforcement_level";
const HSTS_PRELOAD_LIST_PREF =
    "network.stricttransportsecurity.preloadlist";

/** TLS certificate service override management for Marionette. */
this.cert = {
  Error: {
    Untrusted: 1,
    Mismatch: 2,
    Time: 4,
  },

  currentOverride: null,
};

/**
 * Installs a TLS certificate service override.
 *
 * The provided |service| must implement the |register| and |unregister|
 * functions that causes a new |nsICertOverrideService| interface
 * implementation to be registered with the |nsIComponentRegistrar|.
 *
 * After |service| is registered and made the |cert.currentOverride|,
 * |nsICertOverrideService| is reinitialised to cause all Gecko components
 * to pick up the new service.
 *
 * If an override is already installed, i.e. when |cert.currentOverride|
 * is not null, this functions acts as a NOOP.
 *
 * @param {cert.Override} service
 *     Service generator that registers and unregisters the XPCOM service.
 *
 * @throws {Components.Exception}
 *     If unable to register or initialise |service|.
 */
cert.installOverride = function(service) {
  if (this.currentOverride) {
    return;
  }

  service.register();
  cert.currentOverride = service;
};

/**
 * Uninstall a TLS certificate service override.
 *
 * After the service has been unregistered, |cert.currentOverride|
 * is reset to null.
 *
 * If there no current override installed, i.e. if |cert.currentOverride|
 * is null, this function acts as a NOOP.
 */
cert.uninstallOverride = function() {
  if (!cert.currentOverride) {
    return;
  }
  cert.currentOverride.unregister();
  this.currentOverride = null;
};

/**
 * Certificate override service that acts in an all-inclusive manner
 * on TLS certificates.
 *
 * When an invalid certificate is encountered, it is overriden
 * with the |matching| bit level, which is typically a combination of
 * |cert.Error.Untrusted|, |cert.Error.Mismatch|, and |cert.Error.Time|.
 *
 * @type cert.Override
 *
 * @throws {Components.Exception}
 *     If there are any problems registering the service.
 */
cert.InsecureSweepingOverride = function() {
  const CID = Components.ID("{4b67cce0-a51c-11e6-9598-0800200c9a66}");
  const DESC = "All-encompassing cert service that matches on a bitflag";

  // This needs to be an old-style class with a function constructor
  // and prototype assignment because... XPCOM.  Any attempt at
  // modernisation will be met with cryptic error messages which will
  // make your life miserable.
  let service = function() {};
  service.prototype = {
    hasMatchingOverride: function(
        aHostName, aPort, aCert, aOverrideBits, aIsTemporary) {
      aIsTemporary.value = false;
      aOverrideBits.value =
          cert.Error.Untrusted | cert.Error.Mismatch | cert.Error.Time;

      return true;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsICertOverrideService]),
  };
  let factory = XPCOMUtils.generateSingletonFactory(service);

  return {
    register: function() {
      // make it possible to register certificate overrides for domains
      // that use HSTS or HPKP
      Preferences.set(HSTS_PRELOAD_LIST_PREF, false);
      Preferences.set(CERT_PINNING_ENFORCEMENT_PREF, 0);

      registrar.registerFactory(CID, DESC, CONTRACT_ID, factory);
    },

    unregister: function() {
      registrar.unregisterFactory(CID, factory);

      Preferences.reset(HSTS_PRELOAD_LIST_PREF);
      Preferences.reset(CERT_PINNING_ENFORCEMENT_PREF);

      // clear collected HSTS and HPKP state
      // through the site security service
      sss.clearAll();
      sss.clearPreloads();
    },
  };
};
