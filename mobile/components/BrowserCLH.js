/* -*- Mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil; -*- */
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
 * The Original Code is BrowserCLH.js
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brian Crowder <crowder@fiverocks.com>
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
const Cr = Components.results;
const Cu = Components.utils;


Cu.import("resource://gre/modules/XPCOMUtils.jsm");

function BrowserCLH() { }

BrowserCLH.prototype = {
  //
  // nsICommandLineHandler
  //
  handle: function fs_handle(cmdLine) {
    // Instantiate the search service so the search engine cache is created now
    // instead when the application is running. The install process will register
    // this component by using the -silent command line flag, thereby creating
    // the cache during install, not runtime.
    // NOTE: This code assumes this CLH is run before the nsDefaultCLH, which
    // consumes the "-silent" flag.
    if (cmdLine.findFlag("silent", false) > -1) {
      let searchService = Cc["@mozilla.org/browser/search-service;1"].
                          getService(Ci.nsIBrowserSearchService);
    }

    let win;
    try {
      var windowMediator =
        Cc["@mozilla.org/appshell/window-mediator;1"].getService(Ci.nsIWindowMediator);

      win = windowMediator.getMostRecentWindow("navigator:browser");
      if (!win)
        return;

      win.focus();
      cmdLine.preventDefault = true;
    } catch (e) { }

    // Assumption:  All CLH arguments we've received have been sent remotely,
    // or we wouldn't already have a window.  Therefore: open 'em all!
    for (let i = 0; i < cmdLine.length; i++) {
      let arg = cmdLine.getArgument(i);
      if (!arg || arg[0] == '-')
        continue;

      let uri = cmdLine.resolveURI(arg);
      if (uri)
        win.browserDOMWindow.openURI(uri, null, Ci.nsIBrowserDOMWindow.OPEN_NEWTAB, null);
    }
  },

  // QI
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),

  // XPCOMUtils factory
  classDescription: "Command Line Handler",
  contractID: "@mozilla.org/mobile/browser-clh;1",
  classID: Components.ID("{be623d20-d305-11de-8a39-0800200c9a66}"),
  _xpcom_categories: [{ category: "command-line-handler", entry: "m-browser" }],
};

var components = [ BrowserCLH ];

function NSGetModule(compMgr, fileSpec) {
  return XPCOMUtils.generateModule(components);
}
