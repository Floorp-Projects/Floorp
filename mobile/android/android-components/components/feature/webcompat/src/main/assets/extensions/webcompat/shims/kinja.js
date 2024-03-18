/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals exportFunction */

"use strict";

/**
 * Kinja powered blogs rely on storage access to https://kinja.com to enable
 * oauth with external providers. For dFPI, sites need to use the Storage Access
 * API to gain first party storage access. This shim calls requestStorageAccess
 * on behalf of the site when a user wants to log in via oauth.
 */

// Third-party origin we need to request storage access for.
const STORAGE_ACCESS_ORIGIN = "https://kinja.com";

// Prefix of the path opened in a new window when users click the oauth login
// buttons.
const OAUTH_PATH_PREFIX = "/oauthlogin?provider=";

console.warn(
  `When using oauth, Firefox calls the Storage Access API on behalf of the site. See https://bugzilla.mozilla.org/show_bug.cgi?id=1656171 for details.`
);

// Overwrite the window.open method so we can detect oauth related popups.
const origOpen = window.wrappedJSObject.open;
Object.defineProperty(window.wrappedJSObject, "open", {
  value: exportFunction((url, ...args) => {
    // Filter oauth popups.
    if (!url.startsWith(OAUTH_PATH_PREFIX)) {
      return origOpen(url, ...args);
    }
    // Request storage access for Kinja.
    document.requestStorageAccessForOrigin(STORAGE_ACCESS_ORIGIN).then(() => {
      origOpen(url, ...args);
    });
    // We don't have the window object yet which window.open returns, since the
    // sign-in flow is dependent on the async storage access request. This isn't
    // a problem as long as the website does not consume it.
    return null;
  }, window),
});
