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

  checkBox: function(aEvent) {
    let elem = aEvent.originalTarget;
    let perm = elem.getAttribute("perm");
    if (this._manifest.capabilities && this._manifest.capabilities.indexOf(perm) != -1) {
      if (elem.checked) {
        elem.classList.remove("webapps-noperm");
        elem.classList.add("webapps-perm");
      } else {
        elem.classList.remove("webapps-perm");
        elem.classList.add("webapps-noperm");
      }
    }
  },

  show: function show(aManifest) {
    if (!aManifest) {
      // Try every way to get an icon
      let browser = Browser.selectedBrowser;
      let icon = browser.appIcon.href;
      if (!icon)
        icon = browser.mIconURL;
      if (!icon)
        icon = gFaviconService.getFaviconImageForPage(browser.currentURI).spec;

      // Create a simple manifest
      aManifest = {
        uri: browser.currentURI.spec,
        name: browser.contentTitle,
        icon: icon,
        capabilities: [],
      };
    }

    this._manifest = aManifest;
    this._dialog = importDialog(window, "chrome://browser/content/webapps.xul", null);

    if (aManifest.name)
      document.getElementById("webapps-title").value = aManifest.name;
    if (aManifest.icon)
      document.getElementById("webapps-icon").src = aManifest.icon;

    let uri = Services.io.newURI(aManifest.uri, null, null);

    let perms = [["offline", "offline-app"], ["geoloc", "geo"], ["notifications", "desktop-notification"]];
    let self = this;
    perms.forEach(function(tuple) {
      let elem = document.getElementById("webapps-" + tuple[0] + "-checkbox");
      let currentPerm = Services.perms.testExactPermission(uri, tuple[1]);
      self._perms[tuple[1]] = (currentPerm == Ci.nsIPermissionManager.ALLOW_ACTION);
      if ((aManifest.capabilities && (aManifest.capabilities.indexOf(tuple[1]) != -1)) || (currentPerm == Ci.nsIPermissionManager.ALLOW_ACTION))
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
    this._dialog.close();
    this._dialog = null;
    BrowserUI.popPopup(this);
  },

  _updatePermission: function updatePermission(aId, aPerm) {
    try {
      let uri = Services.io.newURI(this._manifest.uri, null, null);
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

    this._updatePermission("webapps-offline-checkbox", "offline-app");
    this._updatePermission("webapps-geoloc-checkbox", "geo");
    this._updatePermission("webapps-notifications-checkbox", "desktop-notification");

    this.hide();
    this.install(this._manifest.uri, title, this._manifest.icon);
  },

  updateWebappsInstall: function updateWebappsInstall(aNode) {
    if (document.getElementById("main-window").hasAttribute("webapp"))
      return false;

    let browser = Browser.selectedBrowser;

    let webapp = Cc["@mozilla.org/webapps/support;1"].getService(Ci.nsIWebappsSupport);
    return !(webapp && webapp.isApplicationInstalled(browser.currentURI.spec));
  },

  install: function(aURI, aTitle, aIcon) {
    const kIconSize = 64;

    let canvas = document.createElementNS("http://www.w3.org/1999/xhtml", "canvas");
    canvas.setAttribute("style", "display: none");

    let self = this;
    let image = new Image();
    image.onload = function() {
      canvas.width = canvas.height = kIconSize; // clears the canvas
      let ctx = canvas.getContext("2d");
      ctx.drawImage(image, 0, 0, kIconSize, kIconSize);
      let data = canvas.toDataURL("image/png", "");
      canvas = null;
      try {
        let webapp = Cc["@mozilla.org/webapps/support;1"].getService(Ci.nsIWebappsSupport);
        webapp.installApplication(aTitle, aURI, aIcon, data);
      } catch(e) {
        Cu.reportError(e);
      }
    }
    image.onerror = function() {
      // can't load the icon (bad URI) : fallback to the default one from chrome
      self.install(aURI, aTitle, "chrome://browser/skin/images/favicon-default-30.png");
    }
    image.src = aIcon;
  }
};
