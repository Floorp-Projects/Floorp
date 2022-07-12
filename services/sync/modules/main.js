/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["Weave"];

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

var Weave = ChromeUtils.import("resource://services-sync/constants.js");

XPCOMUtils.defineLazyModuleGetters(Weave, {
  Service: "resource://services-sync/service.js",
  Status: "resource://services-sync/status.js",
  Utils: "resource://services-sync/util.js",
  Svc: "resource://services-sync/util.js",
});

XPCOMUtils.defineLazyGetter(Weave, "Crypto", function() {
  let { WeaveCrypto } = ChromeUtils.import(
    "resource://services-crypto/WeaveCrypto.js"
  );
  return new WeaveCrypto();
});
