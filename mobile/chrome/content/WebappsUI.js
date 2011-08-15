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
 * The Original Code is Mozilla Mobile Browser.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fabrice Desré <fabrice@mozilla.com>
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

var WebappsUI = {
  _dialog: null,
  _manifest: null,
  _perms: [],
  _application: null,
  _browser: null,

  init: function() {
    Cu.import("resource://gre/modules/OpenWebapps.jsm");
    this.messageManager = Cc["@mozilla.org/parentprocessmessagemanager;1"].getService(Ci.nsIFrameMessageManager);
    this.messageManager.addMessageListener("OpenWebapps:Install", this);
    this.messageManager.addMessageListener("OpenWebapps:GetInstalledBy", this);
    this.messageManager.addMessageListener("OpenWebapps:AmInstalled", this);
    this.messageManager.addMessageListener("OpenWebapps:MgmtLaunch", this);
    this.messageManager.addMessageListener("OpenWebapps:MgmtList", this);
    this.messageManager.addMessageListener("OpenWebapps:MgmtUninstall", this);
  },

  // converts a manifest to an application as expected by openwebapps.install()
  convertManifest: function(aData) {
    let app = {
      manifest : JSON.parse(aData.manifest),
      installData : aData.installData,
      storeURI : aData.storeURI,
      manifestURI : aData.manifestURI,
      capabilities : [],
      callbackID : aData.callbackID
    }

    let chrome = Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIXULChromeRegistry).QueryInterface(Ci.nsIToolkitChromeRegistry);
    let locale = chrome.getSelectedLocale("browser");

    let localeRoot;
    if (app.manifest.locales)
      localeRoot = app.manifest.locales[locale];

    if (!localeRoot)
      localeRoot = app.manifest;

    let baseURI = Services.io.newURI(aData.manifestURI, null, null);
    app.title = localeRoot.name || app.manifest.name;

    // choose the largest icon
    let max = 0;
    let icon;
    for (let size in app.manifest.icons) {
      let iSize = parseInt(size);
      if (iSize > max) {
        icon = baseURI.resolve(app.manifest.icons[size]);
        max = iSize;
      }
    }
    if (icon)
      app.iconURI = icon;

    let root = baseURI.resolve("/").toString();
    app.appURI = root.substring(0, root.length - 1);

    return app;
  },

  receiveMessage: function(aMessage) {
    this._browser = aMessage.target.QueryInterface(Ci.nsIFrameMessageManager);
    switch(aMessage.name) {
      case "OpenWebapps:Install":
        WebappsUI.show(WebappsUI.convertManifest(aMessage.json));
        break;
      case "OpenWebapps:GetInstalledBy":
        let apps = OpenWebapps.getInstalledBy(aMessage.json.storeURI);
        this._browser.sendAsyncMessage("OpenWebapps:GetInstalledBy:Return",
            { apps: apps, callbackID: aMessage.json.callbackID });
        break;
      case "OpenWebapps:AmInstalled":
        let app = OpenWebapps.amInstalled(aMessage.json.appURI);
        this._browser.sendAsyncMessage("OpenWebapps:AmInstalled:Return",
            { installed: app != null, app: app, callbackID: aMessage.json.callbackID });
        break;
      case "OpenWebapps:MgmtList":
        let list = OpenWebapps.mgmtList();
        this._browser.sendAsyncMessage("OpenWebapps:MgmtList:Return",
            { ok: true, apps: list, callbackID: aMessage.json.callbackID });
        break;
      case "OpenWebapps:MgmtLaunch":
        let res = OpenWebapps.mgmtLaunch(aMessage.json.origin);
        this._browser.sendAsyncMessage("OpenWebapps:MgmtLaunch:Return",
            { ok: res, callbackID: aMessage.json.callbackID });
        break;
      case "OpenWebapps:MgmtUninstall":
        app = OpenWebapps.amInstalled(aMessage.json.origin);
        let uninstalled = OpenWebapps.mgmtUninstall(aMessage.json.origin);
        this.messageManager.sendAsyncMessage("OpenWebapps:MgmtUninstall:Return",
            { ok: uninstalled, app: app, callbackID: aMessage.json.callbackID });
        break;
    }
  },

  checkBox: function(aEvent) {
    let elem = aEvent.originalTarget;
    let perm = elem.getAttribute("perm");
    if (this._application.capabilities && this._application.capabilities.indexOf(perm) != -1) {
      if (elem.checked) {
        elem.classList.remove("webapps-noperm");
        elem.classList.add("webapps-perm");
      } else {
        elem.classList.remove("webapps-perm");
        elem.classList.add("webapps-noperm");
      }
    }
  },

  show: function show(aApplication) {
    this._application = aApplication;
    this._dialog = importDialog(window, "chrome://browser/content/webapps.xul", null);

    if (aApplication.title)
      document.getElementById("webapps-title").value = aApplication.title;
    if (aApplication.iconURI)
      document.getElementById("webapps-icon").src = aApplication.iconURI;

    let uri = Services.io.newURI(aApplication.appURI, null, null);

    let perms = [["offline", "offline-app"], ["geoloc", "geo"], ["notifications", "desktop-notification"]];
    let self = this;
    perms.forEach(function(tuple) {
      let elem = document.getElementById("webapps-" + tuple[0] + "-checkbox");
      let currentPerm = Services.perms.testExactPermission(uri, tuple[1]);
      self._perms[tuple[1]] = (currentPerm == Ci.nsIPermissionManager.ALLOW_ACTION);
      if ((aApplication.capabilities && (aApplication.capabilities.indexOf(tuple[1]) != -1)) || (currentPerm == Ci.nsIPermissionManager.ALLOW_ACTION))
        elem.checked = true;
      else
        elem.checked = (currentPerm == Ci.nsIPermissionManager.ALLOW_ACTION);
      elem.classList.remove("webapps-noperm");
      elem.classList.add("webapps-perm");
    });

    BrowserUI.pushPopup(this, this._dialog);

    // Force a modal dialog
    this._dialog.waitForClose();
  },

  hide: function hide() {
    this.close();

    this._browser.sendAsyncMessage("OpenWebapps:InstallAborted", { callbackID: this._application.callbackID });
  },
  
  close: function close() {
    this._dialog.close();
    this._dialog = null;
    BrowserUI.popPopup(this);
  },

  _updatePermission: function updatePermission(aId, aPerm) {
    try {
      let uri = Services.io.newURI(this._application.appURI, null, null);
      let currentState = document.getElementById(aId).checked;
      if (currentState != this._perms[aPerm])
        Services.perms.add(uri, aPerm, currentState ? Ci.nsIPermissionManager.ALLOW_ACTION : Ci.nsIPermissionManager.DENY_ACTION);
    } catch(e) {
      Cu.reportError(e);
    }
  },

  launch: function launch() {
    let title = document.getElementById("webapps-title").value;
    if (!title)
      return;

    this._application.title = title;

    this._updatePermission("webapps-offline-checkbox", "offline-app");
    this._updatePermission("webapps-geoloc-checkbox", "geo");
    this._updatePermission("webapps-notifications-checkbox", "desktop-notification");

    this.close();
    try {
      OpenWebapps.install(this._application);
      let app = OpenWebapps.amInstalled(this._application.appURI);
      this.messageManager.sendAsyncMessage("OpenWebapps:InstallDone", { app: app, callbackID: this._application.callbackID });
    } catch(e) {
      Cu.reportError(e);
    }
  }
};
