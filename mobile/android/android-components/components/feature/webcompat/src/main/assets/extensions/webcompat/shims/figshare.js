/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Bug 1895990 - figshare login broken with dFPI enabled
 *
 * The websites that use figshare for login require unpartitioned third-party
 * cookie access for https://figshare.com. The figshare login process sets a
 * third-party cookie for https://figshare.com, which is used as an proof of
 * authentication on redirect back to the main site. This shim adds a request
 * for storage access for https://figshare.com when the user tries to log in.
 */

// Third-party origin we need to request storage access for.
const STORAGE_ACCESS_ORIGIN = "https://figshare.com";

console.warn(
  `When logging in, Firefox calls the Storage Access API on behalf of the site. See https://bugzilla.mozilla.org/show_bug.cgi?id=1895990 for details.`
);

document.documentElement.addEventListener(
  "click",
  e => {
    const { target, isTrusted } = e;
    if (!isTrusted) {
      return;
    }

    // If the user clicks the login link, we need to request storage access
    // for https://figshare.com.
    const link = target.closest(`a[href^="https://login.figshare.com"]`);
    if (!link) {
      return;
    }

    console.warn(
      "Calling the Storage Access API on behalf of " + STORAGE_ACCESS_ORIGIN
    );

    e.stopPropagation();
    e.preventDefault();
    document.requestStorageAccessForOrigin(STORAGE_ACCESS_ORIGIN).then(() => {
      link.click();
    });
  },
  true
);

function watchFirefoxNotificationDialogAndHide() {
  const observer = new MutationObserver((mutations, obs) => {
    const element = document.querySelector(
      "div[data-alerts-channel='firefox-notifications']"
    );
    if (element) {
      element.style.display = "none";
      obs.disconnect(); // Stop observing once we've found and hidden the element
    }
  });

  // Start observing the document.
  observer.observe(document.body, {
    childList: true,
    subtree: true,
  });
}

// Hide the Firefox error notification
const notificationElement = document.querySelector(
  "div[data-alerts-channel='firefox-notifications']"
);
if (notificationElement) {
  notificationElement.style.display = "none";
} else {
  // Add a listener to watch for the Firefox notification element to load.
  window.addEventListener(
    "DOMContentLoaded",
    watchFirefoxNotificationDialogAndHide
  );
}
