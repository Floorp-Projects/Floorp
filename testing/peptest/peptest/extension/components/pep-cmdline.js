/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const PEP_CONTRACTID  = "@mozilla.org/commandlinehandler/general-startup;1?type=pep";
const PEP_CID         = Components.ID('{807b1ae9-df22-40bd-8d0a-2a583da551bb}');
const PEP_CATEGORY    = "m-pep";
const PEP_DESCRIPTION = "Responsiveness Testing Harness";

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

// Command Line Handler
function CommandLineHandler() {
    this.wrappedJSObject = this;
    this.firstRun = true;
};

CommandLineHandler.prototype = {
  classID: PEP_CID,
  classDescription: PEP_DESCRIPTION,
  contractID: PEP_CONTRACTID,

  QueryInterface: XPCOMUtils.generateQI([
      Components.interfaces.nsISupports,
      Components.interfaces.nsICommandLineHandler
  ]),

  _xpcom_categories: [{
      category: "command-line-handler",
      entry: PEP_CATEGORY,
  }],

  /* nsICommandLineHandler */
  handle : function (cmdLine) {
    try {
      this.manifest = cmdLine.handleFlagWithParam("pep-start", false);
      if (cmdLine.handleFlag("pep-noisy", false)) {
        this.noisy = true;
      }
    }
    catch (e) {
      dump("incorrect parameter passed to pep on the command line.");
      return;
    }
  },

  helpInfo : "  -pep-start <file>    Run peptests described in given manifest\n" +
             "  -pep-noisy           Dump debug messages to console during test run\n"
};

/**
* XPCOMUtils.generateNSGetFactory was introduced in Mozilla 2 (Firefox 4).
* XPCOMUtils.generateNSGetModule is for Mozilla 1.9.2 (Firefox 3.6).
*/
if (XPCOMUtils.generateNSGetFactory)
    var NSGetFactory = XPCOMUtils.generateNSGetFactory([CommandLineHandler]);
else
    var NSGetModule = XPCOMUtils.generateNSGetModule([CommandLineHandler]);
