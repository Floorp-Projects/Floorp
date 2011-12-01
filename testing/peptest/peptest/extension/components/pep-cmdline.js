/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is peptest.
 *
 * The Initial Developer of the Original Code is
 *    The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *    Andrew Halberstadt <halbersa@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
