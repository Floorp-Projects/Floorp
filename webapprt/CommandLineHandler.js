/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function CommandLineHandler() {}

CommandLineHandler.prototype = {
  classID: Components.ID("{6d69c782-40a3-469b-8bfd-3ee366105a4a}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),

  handle: function handle(cmdLine) {
    let args = Cc["@mozilla.org/hash-property-bag;1"].
               createInstance(Ci.nsIWritablePropertyBag);
    let inTestMode = this._handleTestMode(cmdLine, args);
    Services.obs.notifyObservers(args, "webapprt-command-line", null);

    // Initialize DOMApplicationRegistry by importing Webapps.jsm, but only
    // after broadcasting webapprt-command-line.  Webapps.jsm calls
    // DOMApplicationRegistry.init() when it's first imported.  init() accesses
    // the WebappRegD directory, which in test mode is special-cased by
    // DirectoryProvider.js after it observes webapprt-command-line.
    Cu.import("resource://gre/modules/Webapps.jsm");

    if (!inTestMode) {
      startUp(inTestMode);
    } else {
      // startUp() accesses WebappRT.config, which in test mode is not valid
      // until WebappRT.jsm observes an app installation.
      Services.obs.addObserver(function onInstall(subj, topic, data) {
        Services.obs.removeObserver(onInstall, "webapprt-test-did-install");
        startUp(inTestMode);
      }, "webapprt-test-did-install", false);
    }

    // Open the window with arguments to identify it as the main window
    Services.ww.openWindow(null,
                           "chrome://webapprt/content/webapp.xul",
                           "_blank",
                           "chrome,dialog=no,resizable,scrollbars,centerscreen",
                           args);
  },

  _handleTestMode: function _handleTestMode(cmdLine, args) {
    // -test-mode [url]
    let idx = cmdLine.findFlag("test-mode", true);
    if (idx < 0)
      return false;
    let url = null;
    let urlIdx = idx + 1;
    if (urlIdx < cmdLine.length) {
      let potentialURL = cmdLine.getArgument(urlIdx);
      if (potentialURL && potentialURL[0] != "-") {
        url = potentialURL;
        try {
          Services.io.newURI(url, null, null);
        } catch (err) {
          throw Components.Exception(
            "-test-mode argument is not a valid URL: " + url,
            Components.results.NS_ERROR_INVALID_ARG);
        }
        cmdLine.removeArguments(urlIdx, urlIdx);
      }
    }
    cmdLine.removeArguments(idx, idx);
    args.setProperty("test-mode", url);
    return true;
  },

  helpInfo : "",
};

let components = [CommandLineHandler];
let NSGetFactory = XPCOMUtils.generateNSGetFactory(components);

/* There's some work we need to do on startup, before we load the webapp,
 * and this seems as good a place as any to do it, although it's possible
 * that in the future we will find a lazier place to do it.
 *
 * NOTE: it's very important that the stuff we do here doesn't prevent
 * the command-line handler from being registered/accessible, since otherwise
 * the app won't start, which is catastrophic failure.  That's why it's all
 * wrapped in a try/catch block. */
function startUp(inTestMode) {
  try {
    if (!inTestMode) {
      // Initialize window-independent handling of webapps- notifications.  Skip
      // this in test mode, since it interferes with test app installations.
      // We'll have to revisit this when we actually want to test installations
      // and other functionality provided by WebappsHandler.
      Cu.import("resource://webapprt/modules/WebappsHandler.jsm");
      WebappsHandler.init();
    }

    // On firstrun, set permissions to their default values.
    if (!Services.prefs.getBoolPref("webapprt.firstrun")) {
      Cu.import("resource://webapprt/modules/WebappRT.jsm");
      let uri = Services.io.newURI(WebappRT.config.app.origin, null, null);

      // Set AppCache-related permissions.
      Services.perms.add(uri, "pin-app",
                         Ci.nsIPermissionManager.ALLOW_ACTION);
      Services.perms.add(uri, "offline-app",
                         Ci.nsIPermissionManager.ALLOW_ACTION);

      Services.perms.add(uri, "indexedDB",
                         Ci.nsIPermissionManager.ALLOW_ACTION);
      Services.perms.add(uri, "indexedDB-unlimited",
                         Ci.nsIPermissionManager.ALLOW_ACTION);

      // Now that we've set the appropriate permissions, twiddle the firstrun
      // flag so we don't try to do so again.
      Services.prefs.setBoolPref("webapprt.firstrun", true);
    }
  } catch(ex) {
#ifdef MOZ_DEBUG
    dump(ex + "\n");
#endif
  }
}
