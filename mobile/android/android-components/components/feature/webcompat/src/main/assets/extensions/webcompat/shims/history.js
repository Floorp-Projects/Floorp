/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Bug 1624853 - Shim Storage Access API on history.com
 *
 * history.com uses Adobe as a necessary third party to authenticating
 * with a TV provider. In order to accomodate this, we grant storage access
 * to the Adobe domain via the Storage Access API when the login or logout
 * buttons are clicked, then forward the click to continue as normal.
 */

console.warn(
  `When using oauth, Firefox calls the Storage Access API on behalf of the site. See https://bugzilla.mozilla.org/show_bug.cgi?id=1624853 for details.`
);

// Third-party origin we need to request storage access for.
const STORAGE_ACCESS_ORIGIN = "https://sp.auth.adobe.com";

document.documentElement.addEventListener(
  "click",
  e => {
    const { target, isTrusted } = e;
    if (!isTrusted) {
      return;
    }

    const button = target.closest("a");
    if (!button) {
      return;
    }

    const buttonLink = button.href;
    if (buttonLink?.startsWith("https://www.history.com/mvpd-auth")) {
      button.disabled = true;
      button.style.opacity = 0.5;
      e.stopPropagation();
      e.preventDefault();
      document
        .requestStorageAccessForOrigin(STORAGE_ACCESS_ORIGIN)
        .then(() => {
          target.click();
        })
        .catch(() => {
          button.disabled = false;
          button.style.opacity = 1.0;
        });
    }
  },
  true
);
