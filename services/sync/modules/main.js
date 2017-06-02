/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["Weave"];

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

this.Weave = {};
Cu.import("resource://services-sync/constants.js", Weave);
var lazies = {
  "service.js":           ["Service"],
  "status.js":            ["Status"],
  "util.js":              ["Utils", "Svc"]
};

function lazyImport(module, dest, props) {
  function getter(prop) {
    return function() {
      let ns = {};
      Cu.import(module, ns);
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
  let { WeaveCrypto } = Cu.import("resource://services-crypto/WeaveCrypto.js", {});
  return new WeaveCrypto();
});
