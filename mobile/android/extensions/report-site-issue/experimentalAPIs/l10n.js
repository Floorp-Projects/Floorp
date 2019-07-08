/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI, XPCOMUtils */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "l10nStrings", function() {
  return Services.strings.createBundle(
    "chrome://browser/locale/webcompatReporter.properties"
  );
});

let l10nManifest;

this.l10n = class extends ExtensionAPI {
  getAPI(context) {
    return {
      l10n: {
        getMessage(name) {
          try {
            return Promise.resolve(l10nStrings.GetStringFromName(name));
          } catch (e) {
            return Promise.reject(e);
          }
        },
      },
    };
  }
};
