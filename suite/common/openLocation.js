/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Lowe <michael.lowe@bigfoot.com>
 *   Blake Ross   <blaker@netscape.com>
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

var browser;
var dialog = {};
var pref = null;
try {
  pref = Components.classes["@mozilla.org/preferences-service;1"]
                   .getService(Components.interfaces.nsIPrefBranch);
} catch (ex) {
  // not critical, remain silent
}

function onLoad()
{
  dialog.input          = document.getElementById("dialog.input");
  dialog.open           = document.documentElement.getButton("accept");
  dialog.openAppList    = document.getElementById("openAppList");
  dialog.openTopWindow  = document.getElementById("currentWindow");
  dialog.openNewWindow  = document.getElementById("newWindow");
  dialog.openEditWindow = document.getElementById("editWindow");
  dialog.openNewTab     = document.getElementById("newTab");
  dialog.bundle         = document.getElementById("openLocationBundle");

  browser = window.arguments[0].browser;
   
  if (!browser) {
    // No browser supplied - we are calling from Composer
    dialog.openAppList.selectedItem = dialog.openEditWindow;

    // Change string to make more sense for Composer
    dialog.openTopWindow.setAttribute("label", dialog.bundle.getString("existingNavigatorWindow"));

    // Find most recent browser window
    var windowManager = Components.classes['@mozilla.org/appshell/window-mediator;1'].getService();
    var windowManagerInterface = windowManager.QueryInterface( Components.interfaces.nsIWindowMediator);
    if (windowManagerInterface)
      browser = windowManagerInterface.getMostRecentWindow( "navigator:browser" );

    // Disable "current browser" item if no browser is open
    if (!browser) {
      dialog.openTopWindow.setAttribute("disabled", "true");
      dialog.openNewTab.setAttribute("disabled", "true");
    }
  }
  else {
    dialog.openAppList.selectedItem = dialog.openTopWindow;
  }

  if (pref) {
    try {
      dialog.openAppList.value = pref.getIntPref("general.open_location.last_window_choice");
      dialog.input.value = pref.getComplexValue("general.open_location.last_url",
                                                Components.interfaces.nsISupportsString).data;
    }
    catch(ex) {
    }
    if (dialog.input.value)
      dialog.input.select(); // XXX should probably be done automatically
  }

  doEnabling();
}

function doEnabling()
{
    dialog.open.disabled = !dialog.input.value;
}

function open()
{
  var params = window.arguments[0];
  params.url = dialog.input.value;
  params.action = dialog.openAppList.value;
  if (!browser && params.action != "2")
    params.action = "1";

  if (pref) {
    var str = Components.classes["@mozilla.org/supports-string;1"]
                        .createInstance(Components.interfaces.nsISupportsString);
    str.data = dialog.input.value;
    pref.setComplexValue("general.open_location.last_url",
                         Components.interfaces.nsISupportsString, str);
    pref.setIntPref("general.open_location.last_window_choice", dialog.openAppList.value);
  }
}

const nsIFilePicker = Components.interfaces.nsIFilePicker;
function onChooseFile()
{
  try {
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, dialog.bundle.getString("chooseFileDialogTitle"), nsIFilePicker.modeOpen);
    if (dialog.openAppList.value == "2") {
      // When loading into Composer, direct user to prefer HTML files and text files,
      // so we call separately to control the order of the filter list
      fp.appendFilters(nsIFilePicker.filterHTML | nsIFilePicker.filterText);
      fp.appendFilters(nsIFilePicker.filterText);
      fp.appendFilters(nsIFilePicker.filterAll);
    }
    else {
      fp.appendFilters(nsIFilePicker.filterHTML | nsIFilePicker.filterText |
                       nsIFilePicker.filterAll | nsIFilePicker.filterImages | nsIFilePicker.filterXML);
    }

    if (fp.show() == nsIFilePicker.returnOK && fp.fileURL.spec && fp.fileURL.spec.length > 0)
      dialog.input.value = fp.fileURL.spec;
  }
  catch(ex) {
  }
  doEnabling();
}

function useUBHistoryItem(aMenuItem)
{
  var urlbar = document.getElementById("dialog.input");
  urlbar.value = aMenuItem.getAttribute("label");
  urlbar.focus();
  doEnabling();
}

