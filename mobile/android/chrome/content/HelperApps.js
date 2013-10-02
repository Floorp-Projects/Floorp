/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

XPCOMUtils.defineLazyGetter(this, "ContentAreaUtils", function() {
  let ContentAreaUtils = {};
  Services.scriptloader.loadSubScript("chrome://global/content/contentAreaUtils.js", ContentAreaUtils);
  return ContentAreaUtils;
});

function getBridge() {
  return Cc["@mozilla.org/android/bridge;1"].getService(Ci.nsIAndroidBridge);
}

function sendMessageToJava(aMessage) {
  return getBridge().handleGeckoMessage(JSON.stringify(aMessage));
}

var HelperApps =  {
  get defaultHttpHandlers() {
    let protoHandlers = this.getAppsForProtocol("http");

    var results = {};
    for (var i = 0; i < protoHandlers.length; i++) {
      try {
        let protoApp = protoHandlers.queryElementAt(i, Ci.nsIHandlerApp);
        results[protoApp.name] = protoApp;
      } catch(e) {}
    }

    delete this.defaultHttpHandlers;
    return this.defaultHttpHandlers = results;
  },

  get protoSvc() {
    delete this.protoSvc;
    return this.protoSvc = Cc["@mozilla.org/uriloader/external-protocol-service;1"].getService(Ci.nsIExternalProtocolService);
  },

  get urlHandlerService() {
    delete this.urlHandlerService;
    return this.urlHandlerService = Cc["@mozilla.org/uriloader/external-url-handler-service;1"].getService(Ci.nsIExternalURLHandlerService);
  },

  getAppsForProtocol: function getAppsForProtocol(uri) {
    let handlerInfoProto = this.protoSvc.getProtocolHandlerInfoFromOS(uri, {});
    return handlerInfoProto.possibleApplicationHandlers;
  },

  getAppsForUri: function getAppsFor(uri) {
    let found = [];
    let mimeType = ContentAreaUtils.getMIMETypeForURI(uri) || "";
    // empty action string defaults to android.intent.action.VIEW
    let msg = {
      type: "Intent:GetHandlers",
      mime: mimeType,
      action: "",
      url: uri.spec,
      packageName: "",
      className: ""
    };
    let data = sendMessageToJava(msg);
    if (!data)
      return found;

    let apps = this._parseApps(JSON.parse(data));
    for (let i = 0; i < apps.length; i++) {
      let appName = apps[i].name;
      if (appName.length > 0 && !this.defaultHttpHandlers[appName])
        found.push(apps[i]);
    }
    return found;
  },

  updatePageAction: function setPageAction(uri) {
    let apps = this.getAppsForUri(uri);
    if (apps.length > 0)
      this._setPageActionFor(uri, apps);
    else
      this._removePageAction();
  },

  _setPageActionFor: function setPageActionFor(uri, apps) {
    this._pageActionUri = uri;

    // If the pageaction is already added, simply update the URI to be launched when 'onclick' is triggered.
    if (this._pageActionId != undefined)
      return;

    this._pageActionId = NativeWindow.pageactions.add({
      title: Strings.browser.GetStringFromName("openInApp.pageAction"),
      icon: "drawable://icon_openinapp",
      clickCallback: function() {
        if (apps.length == 1)
          this._launchApp(apps[0], this._pageActionUri);
        else
          this.openUriInApp(this._pageActionUri);
      }.bind(this)
    });
  },

  _removePageAction: function removePageAction() {
    if(!this._pageActionId)
      return;

    NativeWindow.pageactions.remove(this._pageActionId);
    delete this._pageActionId;
  },

  _launchApp: function launchApp(appData, uri) {
    let mimeType = ContentAreaUtils.getMIMETypeForURI(uri) || "";
    let msg = {
      type: "Intent:Open",
      mime: mimeType,
      action: "android.intent.action.VIEW",
      url: uri.spec,
      packageName: appData.packageName,
      className: appData.activityName
    };
    sendMessageToJava(msg);
  },

  openUriInApp: function openUriInApp(uri) {
    let mimeType = ContentAreaUtils.getMIMETypeForURI(uri) || "";
    let msg = {
      type: "Intent:Open",
      mime: mimeType,
      action: "",
      url: uri.spec,
      packageName: "",
      className: ""
    };
    sendMessageToJava(msg);
  },

  _parseApps: function _parseApps(aJSON) {
    // aJSON -> {apps: [app1Label, app1Default, app1PackageName, app1ActivityName, app2Label, app2Defaut, ...]}
    // see GeckoAppShell.java getHandlersForIntent function for details
    let appInfo = aJSON.apps;
    const numAttr = 4; // 4 elements per ResolveInfo: label, default, package name, activity name.
    let apps = [];
    for (let i = 0; i < appInfo.length; i += numAttr) {
      apps.push({"name" : appInfo[i],
                 "isDefault" : appInfo[i+1],
                 "packageName" : appInfo[i+2],
                 "activityName" : appInfo[i+3]});
    }
    return apps;
  },

  showDoorhanger: function showDoorhanger(aUri, aCallback) {
    let permValue = Services.perms.testPermission(aUri, "native-intent");
    if (permValue != Services.perms.UNKNOWN_ACTION) {
      if (permValue == Services.perms.ALLOW_ACTION) {
        if (aCallback)
          aCallback(aUri);
        else
          this.openUriInApp(aUri);
      } else if (permValue == Services.perms.DENY_ACTION) {
        // do nothing
      }
      return;
    }

    let apps = this.getAppsForUri(aUri);
    let strings = Strings.browser;

    let message = "";
    if (apps.length == 1)
      message = strings.formatStringFromName("helperapps.openWithApp2", [apps[0].name], 1);
    else
      message = strings.GetStringFromName("helperapps.openWithList2");

    let buttons = [{
      label: strings.GetStringFromName("helperapps.open"),
      callback: function(aChecked) {
        if (aChecked)
          Services.perms.add(aUri, "native-intent", Ci.nsIPermissionManager.ALLOW_ACTION);
        if (aCallback)
          aCallback(aUri);
        else
          this.openUriInApp(aUri);
      }
    }, {
      label: strings.GetStringFromName("helperapps.ignore"),
      callback: function(aChecked) {
        if (aChecked)
          Services.perms.add(aUri, "native-intent", Ci.nsIPermissionManager.DENY_ACTION);
      }
    }];

    let options = { checkbox: Strings.browser.GetStringFromName("helperapps.dontAskAgain") };
    NativeWindow.doorhanger.show(message, "helperapps-open", buttons, BrowserApp.selectedTab.id, options);
  }
};
