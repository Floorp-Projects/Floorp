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
 * The Original Code is XPI Dialog Service.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Finkle <mfinkle@mozilla.com>
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
    // first check if the extensions panel is open : fast path to return true
    let browser = Services.wm.getMostRecentWindow("navigator:browser");
    if (browser.ExtensionsView.visible) {
      aInstalls.forEach(function(install) {
        install.install();
      });
      return;
    }
    
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
