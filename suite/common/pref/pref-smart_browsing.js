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
 * The Original Code is Mozilla Communicator client code, released
 * August 15, 2001.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Diego Biurrun   <diego@biurrun.de>
 *   Ian Neal        <bugzilla@arlen.demon.co.uk>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

function moreInfo()
{
  var browserURL = null;
  var regionBundle = document.getElementById("bundle_region");
  var smartBrowsingURL = regionBundle.getString("smartBrowsingURL");
  if (smartBrowsingURL) {
    try {
      browserURL = parent.hPrefWindow.getPref("string", "browser.chromeURL");
    } catch(e) {
    }
    if (browserURL == null)
      browserURL = "chrome://navigator/content/navigator.xul";
    window.openDialog( browserURL, "_blank", "chrome,all,dialog=no", smartBrowsingURL );
  }
}
   
function showACAdvanced()
{
  window.openDialog("chrome://communicator/content/pref/pref-smart_browsing-ac.xul", "", 
                    "modal=yes,chrome,resizable=no",
                    document.getElementById("browserUrlbarAutoFill").getAttribute("value"),
                    document.getElementById("browserUrlbarShowPopup").getAttribute("value"),
                    document.getElementById("browserUrlbarShowSearch").getAttribute("value"),
                    document.getElementById("browserUrlbarMatchOnlyTyped").getAttribute("value"),
                    document.getElementById("browserUrlbarAutoFill").getAttribute("disabled"),
                    document.getElementById("browserUrlbarShowPopup").getAttribute("disabled"),
                    document.getElementById("browserUrlbarShowSearch").getAttribute("disabled"),
                    document.getElementById("browserUrlbarMatchOnlyTyped").getAttribute("disabled"));
}

function receiveACPrefs(aAutoFill, aShowPopup, aShowSearch, aAutoType)
{
  document.getElementById("browserUrlbarAutoFill").setAttribute("value", aAutoFill);
  document.getElementById("browserUrlbarShowPopup").setAttribute("value", aShowPopup);
  document.getElementById("browserUrlbarShowSearch").setAttribute("value", aShowSearch);
  document.getElementById("browserUrlbarMatchOnlyTyped").setAttribute("value", aAutoType);
}

 
function toggleAutoCompleteAdvancedButton()
{
  var browserAutoCompleteEnabled = document.getElementById("browserAutoCompleteEnabled");
  var autoCompleteAdvancedButton = document.getElementById("autoCompleteAdvancedButton");
  autoCompleteAdvancedButton.disabled = !browserAutoCompleteEnabled.checked;
}
