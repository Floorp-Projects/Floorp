/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Prompt.jsm");
Cu.import("resource://gre/modules/Messaging.jsm");

XPCOMUtils.defineLazyGetter(this, "ContentAreaUtils", function() {
  let ContentAreaUtils = {};
  Services.scriptloader.loadSubScript("chrome://global/content/contentAreaUtils.js", ContentAreaUtils);
  return ContentAreaUtils;
});

this.EXPORTED_SYMBOLS = ["App","HelperApps"];

function App(data) {
  this.name = data.name;
  this.isDefault = data.isDefault;
  this.packageName = data.packageName;
  this.activityName = data.activityName;
  this.iconUri = "-moz-icon://" + data.packageName;
}

App.prototype = {
  // callback will be null if a result is not requested
  launch: function(uri, callback) {
    HelperApps._launchApp(this, uri, callback);
    return false;
  }
}

var HelperApps =  {
  get defaultHttpHandlers() {
    delete this.defaultHttpHandlers;
    this.defaultHttpHandlers = this.getAppsForProtocol("http");
    return this.defaultHttpHandlers;
  },

  get defaultHtmlHandlers() {
    delete this.defaultHtmlHandlers;
    this.defaultHtmlHandlers = {};
    let handlers = this.getAppsForUri(Services.io.newURI("http://www.example.com/index.html", null, null), {
      filterHtml: false
    });

    handlers.forEach(function(app) {
      this.defaultHtmlHandlers[app.name] = app;
    }, this);
    return this.defaultHtmlHandlers;
  },

  get protoSvc() {
    delete this.protoSvc;
    return this.protoSvc = Cc["@mozilla.org/uriloader/external-protocol-service;1"].getService(Ci.nsIExternalProtocolService);
  },

  get urlHandlerService() {
    delete this.urlHandlerService;
    return this.urlHandlerService = Cc["@mozilla.org/uriloader/external-url-handler-service;1"].getService(Ci.nsIExternalURLHandlerService);
  },

  prompt: function showPicker(apps, promptOptions, callback) {
    let p = new Prompt(promptOptions).addIconGrid({ items: apps });
    p.show(callback);
  },

  getAppsForProtocol: function getAppsForProtocol(scheme) {
    let protoHandlers = this.protoSvc.getProtocolHandlerInfoFromOS(scheme, {}).possibleApplicationHandlers;

    let results = {};
    for (let i = 0; i < protoHandlers.length; i++) {
      try {
        let protoApp = protoHandlers.queryElementAt(i, Ci.nsIHandlerApp);
        results[protoApp.name] = new App({
          name: protoApp.name,
          description: protoApp.detailedDescription,
        });
      } catch(e) {}
    }

    return results;
  },

  getAppsForUri: function getAppsForUri(uri, flags = { filterHttp: true, filterHtml: true }, callback) {
    flags.filterHttp = "filterHttp" in flags ? flags.filterHttp : true;
    flags.filterHtml = "filterHtml" in flags ? flags.filterHtml : true;

    // Query for apps that can/can't handle the mimetype
    let msg = this._getMessage("Intent:GetHandlers", uri, flags);
    let parseData = (d) => {
      let apps = []

      if (!d)
        return apps;

      apps = this._parseApps(d.apps);

      if (flags.filterHttp) {
        apps = apps.filter(function(app) {
          return app.name && !this.defaultHttpHandlers[app.name];
        }, this);
      }

      if (flags.filterHtml) {
        // Matches from the first '.' to the end of the string, '?', or '#'
        let ext = /\.([^\?#]*)/.exec(uri.path);
        if (ext && (ext[1] === "html" || ext[1] === "htm")) {
          apps = apps.filter(function(app) {
            return app.name && !this.defaultHtmlHandlers[app.name];
          }, this);
        }
      }

      return apps;
    };

    if (!callback) {
      let data = this._sendMessageSync(msg);
      if (!data)
        return [];
      return parseData(data);
    } else {
      sendMessageToJava(msg, function(data) {
        callback(parseData(data));
      });
    }
  },

  launchUri: function launchUri(uri) {
    let msg = this._getMessage("Intent:Open", uri);
    sendMessageToJava(msg);
  },

  _parseApps: function _parseApps(appInfo) {
    // appInfo -> {apps: [app1Label, app1Default, app1PackageName, app1ActivityName, app2Label, app2Defaut, ...]}
    // see GeckoAppShell.java getHandlersForIntent function for details
    const numAttr = 4; // 4 elements per ResolveInfo: label, default, package name, activity name.

    let apps = [];
    for (let i = 0; i < appInfo.length; i += numAttr) {
      apps.push(new App({"name" : appInfo[i],
                 "isDefault" : appInfo[i+1],
                 "packageName" : appInfo[i+2],
                 "activityName" : appInfo[i+3]}));
    }

    return apps;
  },

  _getMessage: function(type, uri, options = {}) {
    let mimeType = options.mimeType;
    if (uri && mimeType == undefined)
      mimeType = ContentAreaUtils.getMIMETypeForURI(uri) || "";
      
    return {
      type: type,
      mime: mimeType,
      action: options.action || "", // empty action string defaults to android.intent.action.VIEW
      url: uri ? uri.spec : "",
      packageName: options.packageName || "",
      className: options.className || ""
    };
  },

  _launchApp: function launchApp(app, uri, callback) {
    if (callback) {
        let msg = this._getMessage("Intent:OpenForResult", uri, {
            packageName: app.packageName,
            className: app.activityName
        });

        sendMessageToJava(msg, function(data) {
            callback(data);
        });
    } else {
        let msg = this._getMessage("Intent:Open", uri, {
            packageName: app.packageName,
            className: app.activityName
        });

        sendMessageToJava(msg);
    }
  },

  _sendMessageSync: function(msg) {
    let res = null;
    sendMessageToJava(msg, function(data) {
      res = data;
    });

    let thread = Services.tm.currentThread;
    while (res == null)
      thread.processNextEvent(true);

    return res;
  },
};
