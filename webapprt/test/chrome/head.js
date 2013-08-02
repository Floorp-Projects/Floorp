/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/Services.jsm");

// Some of the code we want to provide to chrome mochitests is in another file
// so we can share it with the mochitest shim window, thus we need to load it.
Services.scriptloader.loadSubScript("chrome://webapprt/content/mochitest.js",
                                    this);

const MANIFEST_URL_BASE = Services.io.newURI(
  "http://test/webapprtChrome/webapprt/test/chrome/", null, null);

/**
 * Load the webapp in the app browser.
 *
 * @param {String} manifestURL
 *        @see becomeWebapp
 * @param {Object} parameters
 *        @see becomeWebapp
 * @param {Function} onLoad
 *        The callback to call once the webapp is loaded.
 */
function loadWebapp(manifest, parameters, onLoad) {
  let url = Services.io.newURI(manifest, null, MANIFEST_URL_BASE);

  becomeWebapp(url.spec, parameters, function onBecome() {
    function onLoadApp() {
      gAppBrowser.removeEventListener("load", onLoadApp, true);
      onLoad();
    }
    gAppBrowser.addEventListener("load", onLoadApp, true);
    gAppBrowser.setAttribute("src", WebappRT.launchURI.spec);
  });

  registerCleanupFunction(function() {
    // We load DOMApplicationRegistry into a local scope to avoid appearing
    // to leak it.
    let scope = {};
    Cu.import("resource://gre/modules/Webapps.jsm", scope);
    scope.DOMApplicationRegistry.uninstall(url.spec);
  });
}
