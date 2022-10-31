/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals exportFunction */

"use strict";

/**
 * Blogger uses Google as the auth provider. The account panel uses a
 * third-party iframe of https://ogs.google.com, which requires first-party
 * storage access to authenticate. This shim calls requestStorageAccess on
 * behalf of the site when the user opens the account panel.
 */

console.warn(
  `When logging in with Google, Firefox calls the Storage Access API on behalf of the site. See https://bugzilla.mozilla.org/show_bug.cgi?id=1777690 for details.`
);

const STORAGE_ACCESS_ORIGIN = "https://ogs.google.com";

document.documentElement.addEventListener(
  "click",
  e => {
    const { target, isTrusted } = e;
    if (!isTrusted) {
      return;
    }

    const anchorEl = target.closest("a");
    if (!anchorEl) {
      return;
    }

    if (
      !anchorEl.href.startsWith("https://accounts.google.com/SignOutOptions")
    ) {
      return;
    }

    // The storage access request below runs async so the panel won't open
    // immediately. Mitigate this UX issue by updating the clicked element's
    // style so the user gets some immediate feedback.
    anchorEl.style.opacity = 0.5;
    e.stopPropagation();
    e.preventDefault();

    document
      .requestStorageAccessForOrigin(STORAGE_ACCESS_ORIGIN)
      .then(() => {
        // Reload all iframes of ogs.google.com so the first-party cookies are
        // sent to the server.
        // The reload mechanism here is a bit of a hack, since we don't have
        // access to the content window of a cross-origin iframe.
        document
          .querySelectorAll("iframe[src^='https://ogs.google.com/']")
          .forEach(frame => (frame.src += ""));
      })
      // Show the panel in both success and error state. When the user denies
      // the storage access prompt they will see an error message in the account
      // panel.
      .finally(() => {
        anchorEl.style.opacity = 1.0;
        target.click();
      });
  },
  true
);
