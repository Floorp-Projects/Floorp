/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const modules = [
  "Identity.jsm",
  "IdentityProvider.jsm",
  "IdentityStore.jsm",
  "jwcrypto.jsm",
  "RelyingParty.jsm",
  "Sandbox.jsm",
];

function run_test() {
  for each (let m in modules) {
    let resource = "resource://gre/modules/identity/" + m;
    Components.utils.import(resource, {});
    do_print("loaded " + resource);
  }
}
