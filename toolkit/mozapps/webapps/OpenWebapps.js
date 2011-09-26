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
 * The Original Code is Open Web Apps.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

function OpenWebapps() {
  this.messages = ["OpenWebapps:InstallDone", "OpenWebapps:InstallAborted", "OpenWebapps:GetInstalledBy:Return",
                   "OpenWebapps:AmInstalled:Return", "OpenWebapps:MgmtLaunch:Return", "OpenWebapps:MgmtList:Return", 
                   "OpenWebapps:MgmtUninstall:Return"];

  this.mm = Cc["@mozilla.org/childprocessmessagemanager;1"].getService(Ci.nsISyncMessageSender);

  this.messages.forEach((function(msgName) {
    this.mm.addMessageListener(msgName, this);
  }).bind(this));

  this._callbacks = [];
  this._window = null;
  this._watchId = 0;
}

OpenWebapps.prototype = {
  
  /** from https://developer.mozilla.org/en/OpenWebApps/The_Manifest
   * only the name property is mandatory
   */
  checkManifest: function(aManifest) {
    return ("name" in aManifest);
  },
  
  getCallbackId: function(aCallback) {
    let id = "id" + this._getRandomId();
    this._callbacks[id] = aCallback;
    return id;
  },
  
  getCallback: function(aId) {
    return this._callbacks[aId];
  },

  removeCallback: function(aId) {
    if (this._callbacks[aId])
      delete this._callbacks[aId];
  },
  
  _getRandomId: function() {
    return Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  },

  _convertAppsArray: function(aApps) {
    let apps = new Array();
    for (let i = 0; i < aApps.length; i++) {
      let app = aApps[i];
      let xapp = Cc["@mozilla.org/openwebapps/application;1"].createInstance(Ci.nsIOpenWebappsApplication);
      xapp.origin = app.origin;
      xapp.manifest = app.manifest;
      xapp.install_data = app.install_data;
      xapp.install_origin = app.install_origin;
      xapp.install_time = app.install_time;
      apps.push(xapp);
    }
    return apps;
  },

  receiveMessage: function(aMessage) {
    let msg = aMessage.json;
    let callbacks = this.getCallback(msg.callbackID);

    // if we have no such callback and this is not a broadcast message, bail out
    if (!callbacks && aMessage.name != "OpenWebapps:InstallDone"
                   && aMessage.name != "OpenWebapps:MgmtUninstall:Return")
      return;

    switch(aMessage.name) {
      case "OpenWebapps:InstallAborted" :
        if (callbacks.error)
          callbacks.error.handle({ code: "denied", message: "User denied installation" });
        break;
      case "OpenWebapps:InstallDone" :
        if (callbacks && callbacks.success)
          callbacks.success.handle();
        this._onInstalled([msg.app]);
        break;
      case "OpenWebapps:GetInstalledBy:Return":
        if (callbacks && callbacks.success) {
          let apps = this._convertAppsArray(msg.apps);
          callbacks.success.handle(apps, apps.length);
        }
        break;
      case "OpenWebapps:AmInstalled:Return":
        if (callbacks.success)
          callbacks.success.handle(msg.installed ? msg.app : null);
        break;
      case "OpenWebapps:MgmtLaunch:Return":
        if (msg.ok && callbacks && callbacks.success)
          callbacks.success.handle();
        else if (!msg.ok && callbacks.error)
          callbacks.error.handle({ code: "noSuchApp", message: "Unable to launch application"});
        break;
      case "OpenWebapps:MgmtList:Return":
        if (msg.ok && callbacks && callbacks.success) {
          let apps = this._convertAppsArray(msg.apps);
          callbacks.success.handle(apps, apps.length);
        }
        else if (!msg.ok && callbacks && callbacks.error) {
          callbacks.error.handle({ code: "noAppList", message: "Unable to get application list"});
        }
        break;
      case "OpenWebapps:MgmtUninstall:Return":
        if (msg.ok) {
          if (callbacks && callbacks.success)
            callbacks.success.handle();
          this._onUninstalled([msg.app]);
        }
        else if (!msg.ok && callbacks.error)
          callbacks.error.handle({ code: "noSuchApp", message: "Unable to uninstall application"});
        break;
    }
    this.removeCallback(msg.callbackID);
  },
  
  // nsIOpenWebapps implementation
  
  install: function(aURL, aInstallData, aSuccess, aError) {
    let self = this;

    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", aURL, true);

    xhr.onload = function() {
      if (xhr.status == 200) {
        try {
          let manifest = JSON.parse(xhr.responseText);
          if (!self.checkManifest(manifest)) {
            if (aError)
              aError.handle({ code: "invalidManifest", message: "Invalid manifest" });
          } else {
            self.mm.sendAsyncMessage("OpenWebapps:Install", { storeURI: self._window.location.href, manifestURI: aURL, manifest: xhr.responseText,
                             installData: aInstallData, callbackID: self.getCallbackId({ success: aSuccess, error: aError }) });
          }
        } catch(e) {
          if (aError)
            aError.handle({ code: "manifestParseError", message: "Unable to parse the manifest" });
        }
      }
      else if (aError) {
        aError.handle({ code: "networkError", message: "Unable to retrieve manifest" });
      }      
    }

    xhr.onerror = function() {
      if (aError)
        aError.handle({ code: "networkError", message: "Unable to retrieve manifest" });
    }

    xhr.send(null);
  },
  
  amInstalled: function(aSuccess, aError) {
    this.mm.sendAsyncMessage("OpenWebapps:AmInstalled", { appURI: this._window.location.href, callbackID:  this.getCallbackId({ success: aSuccess, error: aError }) });
  },
  
  getInstalledBy: function(aSuccess, aError) {
    this.mm.sendAsyncMessage("OpenWebapps:GetInstalledBy", { storeURI: this._window.location.href, callbackID:  this.getCallbackId({ success: aSuccess, error: aError }) });
  },
  
  // nsIOpenWebappsMgmt implementation
  launch: function(aOrigin, aSuccess, aError) {
    this.mm.sendAsyncMessage("OpenWebapps:MgmtLaunch", { origin: aOrigin, callbackID:  this.getCallbackId({ success: aSuccess, error: aError }) });
  },
  
  list: function(aSuccess, aError) {
    this.mm.sendAsyncMessage("OpenWebapps:MgmtList", { from: this._window.location.href, callbackID:  this.getCallbackId({ success: aSuccess, error: aError }) });
  },
  
  uninstall: function(aOrigin, aSuccess, aError) {
    this.mm.sendAsyncMessage("OpenWebapps:MgmtUninstall", { from: this._window.location.href, origin: aOrigin, callbackID:  this.getCallbackId({ success: aSuccess, error: aError }) });
  },

  _onRepoChange: function(aWhat, aApps) {
    for (let prop in this._callbacks) {
      if (this._callbacks[prop].isWatch) {
        let apps = this._convertAppsArray(aApps);
        this._callbacks[prop].callback.update(aWhat, apps, apps.length);
      }
    }
  },

  _onInstalled: function(aApps) {
    this._onRepoChange("add", aApps);
  },

  _onUninstalled: function(aApps) {
    this._onRepoChange("remove", aApps);
  },

  watchUpdates: function(aCallback) {
    this._watchId++;
    this._callbacks["_watch" + this._getRandomId()] = { isWatch: true, callback: aCallback };
    return this._watchId;
  },

  clearWatch: function(aWatchId) {
    this.removeCallback("_watch" + aWatchId);
  },

  handleEvent: function(aEvent) {
    if (aEvent.type == "unload") {
      // remove all callbacks so we don't call anything on a cleared scope
      this._callbacks = [];
    }
  },
  
  // nsIDOMGlobalPropertyInitializer implementation
  init: function(aWindow) {
    this._window = aWindow;
    this._window.addEventListener("unload", this, false);
  },
  
  get mgmt() {
    return this.QueryInterface(Ci.nsIOpenWebappsMgmt);
  },
  
  classID: Components.ID("{d8fd4d63-27ea-47b9-a931-481214bb8b5b}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIOpenWebapps, Ci.nsIOpenWebappsMgmt, Ci.nsIDOMGlobalPropertyInitializer]),
  
  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{d8fd4d63-27ea-47b9-a931-481214bb8b5b}"),
                                    contractID: "@mozilla.org/openwebapps;1",
                                    interfaces: [Ci.nsIOpenWebapps],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "OpenWebapps"})
}

function OpenWebappsApplication() {
}

OpenWebappsApplication.prototype = {
  origin: null,
  manifest: null,
  install_data: null,
  install_origin: null,
  install_time: 0,

  classID: Components.ID("{34456347-0792-45a4-8eb1-7b5f94f2d700}"),

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIOpenWebappsApplication]),

  classInfo: XPCOMUtils.generateCI({classID: Components.ID("{34456347-0792-45a4-8eb1-7b5f94f2d700}"),
                                    contractID: "@mozilla.org/openwebapps/application;1",
                                    interfaces: [Ci.nsIOpenWebappsApplication],
                                    flags: Ci.nsIClassInfo.DOM_OBJECT,
                                    classDescription: "OpenWebapps Application"})
}

const NSGetFactory = XPCOMUtils.generateNSGetFactory([OpenWebapps, OpenWebappsApplication]);

