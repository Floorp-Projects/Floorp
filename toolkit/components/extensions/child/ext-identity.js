/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

var {Constructor: CC} = Components;

ChromeUtils.defineModuleGetter(this, "CommonUtils",
                               "resource://services-common/utils.js");
XPCOMUtils.defineLazyPreferenceGetter(this, "redirectDomain",
                                      "extensions.webextensions.identity.redirectDomain");

let CryptoHash = CC("@mozilla.org/security/hash;1", "nsICryptoHash", "initWithString");

XPCOMUtils.defineLazyGlobalGetters(this, ["URL", "TextEncoder"]);

const computeHash = str => {
  let byteArr = new TextEncoder().encode(str);
  let hash = new CryptoHash("sha1");
  hash.update(byteArr, byteArr.length);
  return CommonUtils.bytesAsHex(hash.finish(false));
};

this.identity = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;
    return {
      identity: {
        getRedirectURL: function(path = "") {
          let hash = computeHash(extension.id);
          let url = new URL(`https://${hash}.${redirectDomain}/`);
          url.pathname = path;
          return url.href;
        },
      },
    };
  }
};
