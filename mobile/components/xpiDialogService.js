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

const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

// -----------------------------------------------------------------------
// XPInstall Dialog Service
// -----------------------------------------------------------------------

function XPInstallDialogService() { }

XPInstallDialogService.prototype = {
  classDescription: "XPInstall Dialog Service",
  contractID: "@mozilla.org/embedui/xpinstall-dialog-service;1",
  classID: Components.ID("{c1242012-27d8-477e-a0f1-0b098ffc329b}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIXPIDialogService, Ci.nsIXPIProgressDialog]),

  confirmInstall: function(aParent, aPackages, aCount) {
    // Return true to get the install started
    return true;
  },

  openProgressDialog: function(aPackages, aCount, aManager) {
    // Create a param block with the packages
    let dpb = Cc["@mozilla.org/embedcomp/dialogparam;1"].createInstance(Ci.nsIDialogParamBlock);
    dpb.SetInt(0, 2);                       // OK and Cancel buttons
    dpb.SetInt(1, aPackages.length);        // Number of strings
    dpb.SetNumberStrings(aPackages.length); // Add strings
    for (let i = 0; i < aPackages.length; ++i)
      dpb.SetString(i, aPackages[i]);

    let dpbWrap = Cc["@mozilla.org/supports-interface-pointer;1"].createInstance(Ci.nsISupportsInterfacePointer);
    dpbWrap.data = dpb;
    dpbWrap.dataIID = Ci.nsIDialogParamBlock;

    let obsWrap = Cc["@mozilla.org/supports-interface-pointer;1"].createInstance(Ci.nsISupportsInterfacePointer);
    obsWrap.data = aManager;
    obsWrap.dataIID = Ci.nsIObserver;

    let params = Cc["@mozilla.org/supports-array;1"].createInstance(Ci.nsISupportsArray);
    params.AppendElement(dpbWrap);
    params.AppendElement(obsWrap);

    let os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
    os.notifyObservers(params, "xpinstall-download-started", null);

    // Give the nsXPInstallManager a progress dialog
    aManager.observe(this, "xpinstall-progress", "open");
  },

  onStateChange: function(aIndex, aState, aError) { },
  onProgress: function(aIndex, aValue, aMax) { }
};

function NSGetModule(aCompMgr, aFileSpec) {
  return XPCOMUtils.generateModule([XPInstallDialogService]);
}
