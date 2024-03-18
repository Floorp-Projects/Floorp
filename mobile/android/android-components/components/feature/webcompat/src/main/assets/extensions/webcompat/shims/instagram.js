/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Bug 1804445 - instagram login broken with dFPI enabled
 *
 * Instagram login with Facebook account requires Facebook to have the storage
 * access under Instagram. This shim adds a request for storage access for
 * Facebook when the user tries to log in with a Facebook account.
 */

console.warn(
  `When logging in, Firefox calls the Storage Access API on behalf of the site. See https://bugzilla.mozilla.org/show_bug.cgi?id=1804445 for details.`
);

// Third-party origin we need to request storage access for.
const STORAGE_ACCESS_ORIGIN = "https://www.facebook.com";

document.documentElement.addEventListener(
  "click",
  e => {
    const { target, isTrusted } = e;
    if (!isTrusted) {
      return;
    }
    const button = target.closest("button[type=button]");
    if (!button) {
      return;
    }
    const form = target.closest("#loginForm");
    if (!form) {
      return;
    }

    console.warn(
      "Calling the Storage Access API on behalf of " + STORAGE_ACCESS_ORIGIN
    );
    button.disabled = true;
    e.stopPropagation();
    e.preventDefault();
    document
      .requestStorageAccessForOrigin(STORAGE_ACCESS_ORIGIN)
      .then(() => {
        button.disabled = false;
        target.click();
      })
      .catch(() => {
        button.disabled = false;
      });
  },
  true
);
