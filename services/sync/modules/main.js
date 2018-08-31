/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["Weave"];

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

var Weave = {};
ChromeUtils.import("resource://services-sync/constants.js", Weave);
var lazies = {
  "service.js":           ["Service"],
  "status.js":            ["Status"],
  "util.js":              ["Utils", "Svc"],
};

function lazyImport(module, dest, props) {
  function getter(prop) {
    return function() {
      let ns = {};
      ChromeUtils.import(module, ns);
      delete dest[prop];
      return dest[prop] = ns[prop];
    };
  }
  props.forEach(function(prop) { dest.__defineGetter__(prop, getter(prop)); });
}

for (let mod in lazies) {
  lazyImport("resource://services-sync/" + mod, Weave, lazies[mod]);
}

XPCOMUtils.defineLazyGetter(Weave, "Crypto", function() {
  let { WeaveCrypto } = ChromeUtils.import("resource://services-crypto/WeaveCrypto.js", {});
  return new WeaveCrypto();
});
