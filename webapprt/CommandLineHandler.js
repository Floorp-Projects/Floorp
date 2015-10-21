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

    if (inTestMode) {
      // Open the mochitest shim window, which configures the runtime for tests.
      Services.ww.openWindow(null,
                             "chrome://webapprt/content/mochitest.xul",
                             "_blank",
                             "chrome,dialog=no",
                             args);
    } else {
      // We're opening the window here in order to show it as soon as possible.
      let window = Services.ww.openWindow(null,
                                          "chrome://webapprt/content/webapp.xul",
                                          "_blank",
                                          "chrome,dialog=no,resizable,scrollbars,centerscreen",
                                          null);
      // Load the module to start up the app
      Cu.import("resource://webapprt/modules/Startup.jsm");
      startup(window).then(null, function (aError) {
        dump("Error: " + aError + "\n");
        Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit);
      });
    }
  },

  _handleTestMode: function _handleTestMode(cmdLine, args) {
    // -test-mode [url]
    let idx = cmdLine.findFlag("test-mode", true);
    if (idx < 0)
      return false;
    let url;
    let urlIdx = idx + 1;
    if (urlIdx < cmdLine.length) {
      let potentialURL = cmdLine.getArgument(urlIdx);
      if (potentialURL && potentialURL[0] != "-") {
        try {
          url = Services.io.newURI(potentialURL, null, null);
        } catch (err) {
          throw Components.Exception(
            "-test-mode argument is not a valid URL: " + potentialURL,
            Components.results.NS_ERROR_INVALID_ARG);
        }
        cmdLine.removeArguments(urlIdx, urlIdx);
        args.setProperty("url", url.spec);
      }
    }
    cmdLine.removeArguments(idx, idx);
    return true;
  },

  helpInfo : "",
};

var components = [CommandLineHandler];
this.NSGetFactory = XPCOMUtils.generateNSGetFactory(components);
