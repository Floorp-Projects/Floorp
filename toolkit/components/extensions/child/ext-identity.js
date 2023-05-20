/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { Constructor: CC } = Components;

ChromeUtils.defineESModuleGetters(this, {
  CommonUtils: "resource://services-common/utils.sys.mjs",
});
XPCOMUtils.defineLazyPreferenceGetter(
  this,
  "redirectDomain",
  "extensions.webextensions.identity.redirectDomain"
);

let CryptoHash = CC(
  "@mozilla.org/security/hash;1",
  "nsICryptoHash",
  "initWithString"
);

XPCOMUtils.defineLazyGlobalGetters(this, ["URL", "TextEncoder"]);

const computeHash = str => {
  let byteArr = new TextEncoder().encode(str);
  let hash = new CryptoHash("sha1");
  hash.update(byteArr, byteArr.length);
  return CommonUtils.bytesAsHex(hash.finish(false));
};

this.identity = class extends ExtensionAPI {
  getAPI(context) {
    let { extension } = context;
    return {
      identity: {
        getRedirectURL: function (path = "") {
          let hash = computeHash(extension.id);
          let url = new URL(`https://${hash}.${redirectDomain}/`);
          url.pathname = path;
          return url.href;
        },
        launchWebAuthFlow: function (details) {
          // Validate the url and retreive redirect_uri if it was provided.
          let url, redirectURI;
          let baseRedirectURL = this.getRedirectURL();

          // Allow using loopback address for native OAuth flows as some
          //  providers do not accept the URL provided by getRedirectURL.
          // For more context, see bug 1635344.
          let loopbackURL = `http://127.0.0.1/mozoauth2/${computeHash(
            extension.id
          )}`;
          try {
            url = new URL(details.url);
          } catch (e) {
            return Promise.reject({ message: "details.url is invalid" });
          }
          try {
            redirectURI = new URL(
              url.searchParams.get("redirect_uri") || baseRedirectURL
            );
            if (
              !redirectURI.href.startsWith(baseRedirectURL) &&
              !redirectURI.href.startsWith(loopbackURL)
            ) {
              return Promise.reject({ message: "redirect_uri not allowed" });
            }
          } catch (e) {
            return Promise.reject({ message: "redirect_uri is invalid" });
          }

          return context.childManager.callParentAsyncFunction(
            "identity.launchWebAuthFlowInParent",
            [details, redirectURI.href]
          );
        },
      },
    };
  }
};
