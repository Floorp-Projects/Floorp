/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

export const Weave = ChromeUtils.import(
  "resource://services-sync/constants.js"
);

// We want these to be lazily loaded, which helps performance and also tests
// to not have these loaded before they are ready.
// eslint-disable-next-line mozilla/lazy-getter-object-name
ChromeUtils.defineESModuleGetters(Weave, {
  Service: "resource://services-sync/service.sys.mjs",
  Status: "resource://services-sync/status.sys.mjs",
  Svc: "resource://services-sync/util.sys.mjs",
  Utils: "resource://services-sync/util.sys.mjs",
});

XPCOMUtils.defineLazyGetter(Weave, "Crypto", function() {
  let { WeaveCrypto } = ChromeUtils.importESModule(
    "resource://services-crypto/WeaveCrypto.sys.mjs"
  );
  return new WeaveCrypto();
});
