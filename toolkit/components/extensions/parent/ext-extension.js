/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.extension = class extends ExtensionAPI {
  getAPI(context) {
    return {
      extension: {
        get lastError() {
          return context.lastError;
        },

        isAllowedIncognitoAccess() {
          return context.privateBrowsingAllowed;
        },

        isAllowedFileSchemeAccess() {
          return false;
        },
      },
    };
  }
};
