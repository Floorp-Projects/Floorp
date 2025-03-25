/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const overrides = [
  () => {
    // Override the identity-icon value for replace "Firefox" with "Floorp"
    window.SessionStore.promiseAllWindowsRestored.then(() => {
      document?.getElementById("bundle_brand")?.remove();
      const IdentityIconLabel = document?.getElementById(
        "identity-icon-label",
      ) as XULElement;

      if (IdentityIconLabel) {
        IdentityIconLabel.setAttribute("value", "Floorp");
        IdentityIconLabel.textContent = "Floorp";
        IdentityIconLabel.setAttribute("collapsed", "false");
      }
    });
  },
];
