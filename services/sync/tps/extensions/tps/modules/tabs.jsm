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
 * The Original Code is Crossweave.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonathan Griffin <jgriffin@mozilla.com>
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

 /* This is a JavaScript module (JSM) to be imported via 
   Components.utils.import() and acts as a singleton.
   Only the following listed symbols will exposed on import, and only when 
   and where imported. */

var EXPORTED_SYMBOLS = ["BrowserTabs"];

const CC = Components.classes;
const CI = Components.interfaces;
const CU = Components.utils;

CU.import("resource://services-sync/engines.js");

var BrowserTabs = {
  /**
   * Add
   *
   * Opens a new tab in the current browser window for the
   * given uri.  Throws on error.
   *
   * @param uri The uri to load in the new tab
   * @return nothing
   */
  Add: function(uri, fn) {
    // Open the uri in a new tab in the current browser window, and calls
    // the callback fn from the tab's onload handler.
    let wm = CC["@mozilla.org/appshell/window-mediator;1"]
             .getService(CI.nsIWindowMediator);
    let mainWindow = wm.getMostRecentWindow("navigator:browser");
    let newtab = mainWindow.getBrowser().addTab(uri);
    mainWindow.getBrowser().selectedTab = newtab;
    let win = mainWindow.getBrowser().getBrowserForTab(newtab);
    win.addEventListener("load", function() { fn.call(); }, true);
  },

  /**
   * Find
   *
   * Finds the specified uri and title in Weave's list of remote tabs
   * for the specified profile.
   *
   * @param uri The uri of the tab to find
   * @param title The page title of the tab to find
   * @param profile The profile to search for tabs
   * @return true if the specified tab could be found, otherwise false
   */
  Find: function(uri, title, profile) {
    // Find the uri in Weave's list of tabs for the given profile.
    let engine = Engines.get("tabs");
    for (let [guid, client] in Iterator(engine.getAllClients())) {
      for each (tab in client.tabs) {
        let weaveTabUrl = tab.urlHistory[0];
        if (uri == weaveTabUrl && profile == client.clientName)
          if (title == undefined || title == tab.title)
            return true;
      }
    }
    return false;
  },
};

