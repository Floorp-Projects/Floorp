/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals browser */

// Helper for calling the internal requestStorageAccessForOrigin method. The
// method is called on the first-party document for the third-party which needs
// first-party storage access.
browser.runtime.onMessage.addListener(request => {
  let { requestStorageAccessOrigin, warning } = request;
  if (!requestStorageAccessOrigin) {
    return false;
  }

  // Log a warning to the web console, informing about the shim.
  console.warn(warning);

  // Call the internal storage access API. Passing false means we don't require
  // user activation, but will always show the storage access prompt. The user
  // has to explicitly allow storage access.
  return document
    .requestStorageAccessForOrigin(requestStorageAccessOrigin, false)
    .then(() => {
      return { success: true };
    })
    .catch(() => {
      return { success: false };
    });
});
