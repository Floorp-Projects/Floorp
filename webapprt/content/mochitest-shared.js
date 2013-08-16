/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Note: this script is loaded by both mochitest.js and head.js, so make sure
 * the code you put here can be evaluated by both! */

Cu.import("resource://webapprt/modules/WebappRT.jsm");

// When WebappsHandler opens an install confirmation dialog for apps we install,
// close it, which will be seen as the equivalent of cancelling the install.
// This doesn't prevent us from installing those apps, as we listen for the same
// notification as WebappsHandler and do the install ourselves.  It just
// prevents the modal installation confirmation dialogs from hanging tests.
Services.ww.registerNotification({
  observe: function(win, topic) {
    if (topic == "domwindowopened") {
      // Wait for load because the window is not yet sufficiently initialized.
      win.addEventListener("load", function onLoadWindow() {
        win.removeEventListener("load", onLoadWindow, false);
        if (win.location == "chrome://global/content/commonDialog.xul" &&
            win.opener == window) {
          win.close();
        }
      }, false);
    }
  }
});

/**
 * Transmogrify the runtime session into one for the given webapp.
 *
 * @param {String} manifestURL
 *        The URL of the webapp's manifest, relative to the base URL.
 *        Note that the base URL points to the *chrome* WebappRT mochitests,
 *        so you must supply an absolute URL to manifests elsewhere.
 * @param {Object} parameters
 *        The value to pass as the "parameters" argument to
 *        mozIDOMApplicationRegistry.install, e.g., { receipts: ... }.
 *        Use undefined to pass nothing.
 * @param {Function} onBecome
 *        The callback to call once the transmogrification is complete.
 */
function becomeWebapp(manifestURL, parameters, onBecome) {
  function observeInstall(subj, topic, data) {
    Services.obs.removeObserver(observeInstall, "webapps-ask-install");

    // Step 2: Configure the runtime session to represent the app.
    // We load DOMApplicationRegistry into a local scope to avoid appearing
    // to leak it.

    let scope = {};
    Cu.import("resource://gre/modules/Webapps.jsm", scope);
    Cu.import("resource://webapprt/modules/Startup.jsm", scope);
    scope.DOMApplicationRegistry.confirmInstall(JSON.parse(data));

    let installRecord = JSON.parse(data);
    installRecord.mm = subj;
    installRecord.registryDir = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
    WebappRT.config = installRecord;

    let win = Services.wm.getMostRecentWindow("webapprt:webapp");
    if (!win) {
      win = Services.ww.openWindow(null,
                                   "chrome://webapprt/content/webapp.xul",
                                   "_blank",
                                   "chrome,dialog=no,resizable,scrollbars,centerscreen",
                                   null);
    }

    let promise = scope.startup(win);

    // During chrome tests, we use the same window to load all the tests. We
    // need to change the buildID so that the permissions for the currently
    // tested application get installed.
    Services.prefs.setCharPref("webapprt.buildID", WebappRT.config.app.manifestURL);

    // During tests, the webapps registry is already loaded.
    // The Startup module needs to be notified when the webapps registry
    // gets loaded, so we do that now.
    Services.obs.notifyObservers(this, "webapps-registry-start", null);

    promise.then(onBecome);
  }
  Services.obs.addObserver(observeInstall, "webapps-ask-install", false);

  // Step 1: Install the app at the URL specified by the manifest.
  let url = Services.io.newURI(manifestURL, null, null);
  navigator.mozApps.install(url.spec, parameters);
}
