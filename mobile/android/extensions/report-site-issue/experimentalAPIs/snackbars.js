/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global ExtensionAPI */

ChromeUtils.defineModuleGetter(
  this,
  "clearTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "setTimeout",
  "resource://gre/modules/Timer.jsm"
);
ChromeUtils.defineModuleGetter(
  this,
  "Snackbars",
  "resource://gre/modules/Snackbars.jsm"
);

this.snackbars = class extends ExtensionAPI {
  getAPI(context) {
    return {
      snackbars: {
        show(message, button) {
          return new Promise((callback, rejection) => {
            Snackbars.show(message, Snackbars.LENGTH_LONG, {
              action: {
                label: button,
                callback,
                rejection,
              },
            });
          });
        },
      },
    };
  }
};
