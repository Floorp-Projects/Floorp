/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export { lazy as Weave };

const lazy = {};

// We want these to be lazily loaded, which helps performance and also tests
// to not have these loaded before they are ready.
ChromeUtils.defineESModuleGetters(lazy, {
  Service: "resource://services-sync/service.sys.mjs",
  Status: "resource://services-sync/status.sys.mjs",
  Svc: "resource://services-sync/util.sys.mjs",
  Utils: "resource://services-sync/util.sys.mjs",
});

ChromeUtils.defineLazyGetter(lazy, "Crypto", () => {
  let { WeaveCrypto } = ChromeUtils.importESModule(
    "resource://services-crypto/WeaveCrypto.sys.mjs"
  );
  return new WeaveCrypto();
});
