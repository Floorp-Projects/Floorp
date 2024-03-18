/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1713695 - Shim Adform tracking
 *
 * Sites such as m.tim.it may gate content behind AdForm's trackpoint,
 * breaking download links and such if blocked. This shim stubs out the
 * script and its related tracking pixel, so the content still works.
 */

if (!window.Adform) {
  window.Adform = {
    Opt: {
      disableRedirect() {},
      getStatus(clientID, callback) {
        callback({
          clientID,
          errorMessage: undefined,
          optIn() {},
          optOut() {},
          status: "nocookie",
        });
      },
    },
  };
}
