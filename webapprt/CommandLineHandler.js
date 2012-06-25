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
    // Open the window with arguments to identify it as the main window
    Services.ww.openWindow(null,
                           "chrome://webapprt/content/webapp.xul",
                           "_blank",
                           "chrome,dialog=no,resizable,scrollbars",
                           []);

    // Initialize window-independent handling of webapps- notifications
    Cu.import("resource://webapprt/modules/WebappsHandler.jsm");
    WebappsHandler.init();
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

try {
  // Initialize DOMApplicationRegistry so it can receive/respond to messages.
  Cu.import("resource://gre/modules/Webapps.jsm");

  // On firstrun, set permissions to their default values.
  if (!Services.prefs.getBoolPref("webapprt.firstrun")) {
    Cu.import("resource://webapprt/modules/WebappRT.jsm");
    let uri = Services.io.newURI(WebappRT.config.app.origin, null, null);

    // Set AppCache-related permissions.
    Services.perms.add(uri, "pin-app", Ci.nsIPermissionManager.ALLOW_ACTION);
    Services.perms.add(uri, "offline-app",
                       Ci.nsIPermissionManager.ALLOW_ACTION);

    Services.perms.add(uri, "indexedDB", Ci.nsIPermissionManager.ALLOW_ACTION);
    Services.perms.add(uri, "indexedDB-unlimited",
                       Ci.nsIPermissionManager.ALLOW_ACTION);

    // Now that we've set the appropriate permissions, twiddle the firstrun flag
    // so we don't try to do so again.
    Services.prefs.setBoolPref("webapprt.firstrun", true);
  }
} catch(ex) {
#ifdef MOZ_DEBUG
  dump(ex + "\n");
#endif
}
