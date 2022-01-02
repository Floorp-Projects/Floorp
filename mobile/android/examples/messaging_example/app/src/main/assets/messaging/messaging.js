/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const manifest = document.querySelector("head > link[rel=manifest]");
if (manifest) {
  fetch(manifest.href)
    .then(response => response.json())
    .then(json => {
      const message = { type: "WPAManifest", manifest: json };
      browser.runtime.sendNativeMessage("browser", message);
    });
}
