/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const INSTALL_URL =
  "http://mochi.test:8888/webapprtChrome/webapprt/test/chrome/install.html";

Cu.import("resource://gre/modules/Services.jsm");

/**
 * Installs the given webapp and navigates to it.
 *
 * @param manifestPath
 *        The path of the webapp's manifest relative to
 *        http://mochi.test:8888/webapprtChrome/webapprt/test/chrome/.
 * @param parameters
 *        The value to pass as the "parameters" argument to
 *        mozIDOMApplicationRegistry.install, e.g., { receipts: ... }.  Use
 *        undefined to pass nothing.
 * @param callback
 *        Called when the newly installed webapp is navigated to.  It's passed
 *        the webapp's config object.
 */
function installWebapp(manifestPath, parameters, onInstall) {
  // Three steps: (1) Load install.html, (2) listen for webapprt-test-did-
  // install to get the app config object, and then (3) listen for load of the
  // webapp page to call the callback.  webapprt-test-did-install will be
  // broadcasted before install.html navigates to the webapp page.  (This is due
  // to some implementation details: WebappRT.jsm confirms the installation by
  // calling DOMApplicationRegistry.confirmInstall, and then it immediately
  // broadcasts webapprt-test-did-install.  confirmInstall asynchronously
  // notifies the mozApps consumer via onsuccess, which is when install.html
  // navigates to the webapp page.)

  let content = document.getElementById("content");

  Services.obs.addObserver(function observe(subj, topic, data) {
    // step 2
    Services.obs.removeObserver(observe, "webapprt-test-did-install");
    let appConfig = JSON.parse(data);

    content.addEventListener("load", function onLoad(event) {
      // step 3
      content.removeEventListener("load", onLoad, true);
      let webappURL = appConfig.app.origin + appConfig.app.manifest.launch_path;
      is(event.target.URL, webappURL,
         "No other page should have loaded between installation and " +
         "the webapp's page load: " + event.target.URL);
      onInstall(appConfig);
    }, true);
  }, "webapprt-test-did-install", false);

  // step 1
  let args = [["manifestPath", manifestPath]];
  if (parameters !== undefined) {
    args.push(["parameters", parameters]);
  }
  let queryStr = args.map(function ([key, val])
                          key + "=" + encodeURIComponent(JSON.stringify(val))).
                 join("&");
  let installURL = INSTALL_URL + "?" + queryStr;
  content.loadURI(installURL);
}
