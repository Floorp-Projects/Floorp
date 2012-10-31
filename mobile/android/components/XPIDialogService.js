/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

// -----------------------------------------------------------------------
// Web Install Prompt service
// -----------------------------------------------------------------------

function WebInstallPrompt() { }

WebInstallPrompt.prototype = {
  classID: Components.ID("{c1242012-27d8-477e-a0f1-0b098ffc329b}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.amIWebInstallPrompt]),

  confirm: function(aWindow, aURL, aInstalls) {
    let bundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");

    let prompt = Services.prompt;
    let flags = prompt.BUTTON_POS_0 * prompt.BUTTON_TITLE_IS_STRING + prompt.BUTTON_POS_1 * prompt.BUTTON_TITLE_CANCEL;
    let title = bundle.GetStringFromName("addonsConfirmInstall.title");
    let button = bundle.GetStringFromName("addonsConfirmInstall.install");

    aInstalls.forEach(function(install) {
      let result = (prompt.confirmEx(aWindow, title, install.name, flags, button, null, null, null, {value: false}) == 0);
      if (result)
        install.install();
      else
        install.cancel();
    });
  }
};

const NSGetFactory = XPCOMUtils.generateNSGetFactory([WebInstallPrompt]);
