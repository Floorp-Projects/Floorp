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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Terry Hayes <thayes@netscape.com>
 *   Kai Engert <kaie@netscape.com>
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

var gPrefService = null;
var gPrefBranch = null;

function onLoad() {
  doSetOKCancel(doOK, doCancel);

  // Set checkboxes from prefs
  const nsIPrefService = Components.interfaces.nsIPrefService;

  gPrefService = Components.classes["@mozilla.org/preferences-service;1"].getService(nsIPrefService);
  gPrefBranch = gPrefService.getBranch(null);

  // Enumerate each checkbox on this page and set value
  var prefElements = document.getElementsByAttribute("prefstring", "*");
  for (var i = 0; i < prefElements.length; i++) {
    var element = prefElements[i];
    var prefString = element.getAttribute("prefstring");
    var prefValue = false;

    try {
      prefValue = gPrefBranch.getBoolPref(prefString);
    } catch(e) { /* Put debug output here */ }

    element.setAttribute("checked", prefValue);
    // disable xul element if the pref is locked.
    if (gPrefBranch.prefIsLocked(prefString)) {
      element.disabled=true;
    }
  }
}

function showInfo(cipher_name) {
  window.openDialog('chrome://pippki/content/cipherinfo.xul', cipher_name,
                    'chrome,centerscreen,modal');
}

function doOK() {
 // Save the prefs
 try {
  // Enumerate each checkbox on this page and save the value
  var prefElements = document.getElementsByAttribute("prefstring", "*");
  for (var i = 0; i < prefElements.length; i++) {
    var element = prefElements[i];
    var prefString = element.getAttribute("prefstring");
    var prefValue = element.getAttribute("checked");


    if (typeof(prefValue) == "string") {
      prefValue = (prefValue == "true");
    }

    gPrefBranch.setBoolPref(prefString, prefValue);
  }

  gPrefService.savePrefFile(null);
 } catch(e) { }

 window.close();
}

function doCancel() {
  window.close();
}
