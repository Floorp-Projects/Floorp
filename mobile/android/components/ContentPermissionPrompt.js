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
 * The Original Code is mozilla.org code
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Gian-Carlo Pascutto <gpascutto@mozilla.com>
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
const Cr = Components.results;
const Cu = Components.utils;
const Cc = Components.classes;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const kCountBeforeWeRemember = 5;

function dump(a) {
    Cc["@mozilla.org/consoleservice;1"]
        .getService(Ci.nsIConsoleService)
        .logStringMessage(a);
}

function setPagePermission(type, uri, allow) {
  let pm = Services.perms;
  let contentPrefs = Services.contentPrefs;
  let contentPrefName = type + ".request.remember";

  if (!contentPrefs.hasPref(uri, contentPrefName))
      contentPrefs.setPref(uri, contentPrefName, 0);

  let count = contentPrefs.getPref(uri, contentPrefName);

  if (allow == false)
    count--;
  else
    count++;
    
  contentPrefs.setPref(uri, contentPrefName, count);
  if (count == kCountBeforeWeRemember)
    pm.add(uri, type, Ci.nsIPermissionManager.ALLOW_ACTION);
  else if (count == -kCountBeforeWeRemember)
    pm.add(uri, type, Ci.nsIPermissionManager.DENY_ACTION);
}

const kEntities = { "geolocation": "geolocation", "desktop-notification": "desktopNotification",
                    "indexedDB": "offlineApps", "indexedDBQuota": "indexedDBQuota",
                    "openWebappsManage": "openWebappsManage" };

function ContentPermissionPrompt() {}

ContentPermissionPrompt.prototype = {
  classID: Components.ID("{C6E8C44D-9F39-4AF7-BCC0-76E38A8310F5}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIContentPermissionPrompt]),

  handleExistingPermission: function handleExistingPermission(request) {
    let result = Services.perms.testExactPermission(request.uri, request.type);
    if (result == Ci.nsIPermissionManager.ALLOW_ACTION) {
      request.allow();
      return true;
    }
    if (result == Ci.nsIPermissionManager.DENY_ACTION) {
      request.cancel();
      return true;
    }
    return false;
  },

  getChromeWindow: function getChromeWindow(aWindow) {
     let chromeWin = aWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIWebNavigation)
                            .QueryInterface(Ci.nsIDocShellTreeItem)
                            .rootTreeItem
                            .QueryInterface(Ci.nsIInterfaceRequestor)
                            .getInterface(Ci.nsIDOMWindow)
                            .QueryInterface(Ci.nsIDOMChromeWindow);
     return chromeWin;
  },

  getChromeForRequest: function getChromeForRequest(request) {
    if (request.window) {
      let requestingWindow = request.window.top;
      return this.getChromeWindow(requestingWindow).wrappedJSObject;
    }
    return request.element.ownerDocument.defaultView;
  },

  getTabForRequest: function getTabForRequest(request) {
    let chromeWin = this.getChromeForRequest(request);
    if (request.window) {
      let browser = chromeWin.BrowserApp.getBrowserForWindow(request.window);
      let tabID = chromeWin.BrowserApp.getTabForBrowser(browser).id;
      return tabID;
    }
    // Fix this if e10s is needed again
    return null;
  },

  prompt: function(request) {
    // returns true if the request was handled
    if (this.handleExistingPermission(request))
       return;

    let pm = Services.perms;
    let browserBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");

    let entityName = kEntities[request.type];

    let tabID = this.getTabForRequest(request);
    let chromeWin = this.getChromeForRequest(request);

    let buttons = [{
      label: browserBundle.GetStringFromName(entityName + ".allow"),
      accessKey: null,
      callback: function(notification) {
        setPagePermission(request.type, request.uri, true);
        request.allow();
      }
    },
    {
      label: browserBundle.GetStringFromName(entityName + ".dontAllow"),
      accessKey: null,
      callback: function(notification) {
        setPagePermission(request.type, request.uri, false);
        request.cancel();
      }
    }];

    let message = browserBundle.formatStringFromName(entityName + ".wantsTo",
                                                     [request.uri.host], 1);

    chromeWin.NativeWindow.doorhanger.show(message,
                                           entityName + request.uri.host,
                                           buttons, tabID);
  }
};


//module initialization
const NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentPermissionPrompt]);
