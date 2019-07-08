/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/* globals ContentAreaUtils */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "Prompt",
  "resource://gre/modules/Prompt.jsm"
);

ChromeUtils.defineModuleGetter(
  this,
  "EventDispatcher",
  "resource://gre/modules/Messaging.jsm"
);

XPCOMUtils.defineLazyGetter(this, "ContentAreaUtils", function() {
  let ContentAreaUtils = {};
  Services.scriptloader.loadSubScript(
    "chrome://global/content/contentAreaUtils.js",
    ContentAreaUtils
  );
  return ContentAreaUtils;
});

const NON_FILE_URI_IGNORED_MIME_TYPES = new Set([
  "text/html",
  "application/xhtml+xml",
]);

var EXPORTED_SYMBOLS = ["App", "HelperApps"];

class App {
  constructor(data) {
    this.name = data.name;
    this.isDefault = data.isDefault;
    this.packageName = data.packageName;
    this.activityName = data.activityName;
    this.iconUri = "-moz-icon://" + data.packageName;
  }

  // callback will be null if a result is not requested
  launch(uri, callback) {
    HelperApps._launchApp(this, uri, callback);
    return false;
  }
}

var HelperApps = {
  get defaultBrowsers() {
    delete this.defaultBrowsers;
    this.defaultBrowsers = this._collectDefaultBrowsers();
    return this.defaultBrowsers;
  },

  _collectDefaultBrowsers() {
    let httpHandlers = this._getHandlers("http://www.example.com", {
      filterBrowsers: false,
      filterHtml: false,
    });
    let httpsHandlers = this._getHandlers("https://www.example.com", {
      filterBrowsers: false,
      filterHtml: false,
    });
    return { ...httpHandlers, ...httpsHandlers };
  },

  // Finds handlers that have registered for urls ending in html. Some apps, like
  // the Samsung Video player, will only appear for these urls.
  get defaultHtmlHandlers() {
    delete this.defaultHtmlHandlers;
    return (this.defaultHtmlHandlers = this._getHandlers(
      "http://www.example.com/index.html",
      {
        filterBrowsers: false,
        filterHtml: false,
      }
    ));
  },

  _getHandlers(url, options) {
    let values = {};

    let handlers = this.getAppsForUri(Services.io.newURI(url), options);
    handlers.forEach(app => {
      values[app.name] = app;
    });

    return values;
  },

  get protoSvc() {
    delete this.protoSvc;
    return (this.protoSvc = Cc[
      "@mozilla.org/uriloader/external-protocol-service;1"
    ].getService(Ci.nsIExternalProtocolService));
  },

  get urlHandlerService() {
    delete this.urlHandlerService;
    return (this.urlHandlerService = Cc[
      "@mozilla.org/uriloader/external-url-handler-service;1"
    ].getService(Ci.nsIExternalURLHandlerService));
  },

  prompt(apps, promptOptions, callback) {
    let p = new Prompt(promptOptions).addIconGrid({ items: apps });
    p.show(callback);
  },

  getAppsForProtocol(scheme) {
    let protoHandlers = this.protoSvc.getProtocolHandlerInfoFromOS(scheme, {})
      .possibleApplicationHandlers;

    let results = {};
    for (let i = 0; i < protoHandlers.length; i++) {
      try {
        let protoApp = protoHandlers.queryElementAt(i, Ci.nsIHandlerApp);
        results[protoApp.name] = new App({
          name: protoApp.name,
          description: protoApp.detailedDescription,
        });
      } catch (e) {}
    }

    return results;
  },

  getAppsForUri(uri, flags = {}, callback) {
    // Return early for well-known internal schemes
    if (!uri || uri.schemeIs("about") || uri.schemeIs("chrome")) {
      if (callback) {
        callback([]);
      }
      return [];
    }

    flags.filterBrowsers =
      "filterBrowsers" in flags ? flags.filterBrowsers : true;
    flags.filterHtml = "filterHtml" in flags ? flags.filterHtml : true;

    // Query for apps that can/can't handle the mimetype
    let msg = this._getMessage("Intent:GetHandlers", uri, flags);
    let parseData = apps => {
      if (!apps) {
        return [];
      }

      apps = this._parseApps(apps);

      if (flags.filterBrowsers) {
        apps = apps.filter(app => app.name && !this.defaultBrowsers[app.name]);
      }

      // Some apps will register for html files (the Samsung Video player) but should be shown
      // for non-HTML files (like videos). This filters them only if the page has an htm of html
      // file extension.
      if (flags.filterHtml) {
        // Matches from the first '.' to the end of the string, '?', or '#'
        let ext = /\.([^\?#]*)/.exec(uri.pathQueryRef);
        if (ext && (ext[1] === "html" || ext[1] === "htm")) {
          apps = apps.filter(
            app => app.name && !this.defaultHtmlHandlers[app.name]
          );
        }
      }

      return apps;
    };

    if (!callback) {
      let data = null;
      // Use dispatch to enable synchronous callback for Gecko thread event.
      EventDispatcher.instance.dispatch(msg.type, msg, {
        onSuccess: result => {
          data = result;
        },
        onError: () => {
          throw new Error("Intent:GetHandler callback failed");
        },
      });
      if (data === null) {
        throw new Error("Intent:GetHandler did not return data");
      }
      return parseData(data);
    }
    EventDispatcher.instance.sendRequestForResult(msg).then(data => {
      callback(parseData(data));
    });
  },

  launchUri(uri) {
    let msg = this._getMessage("Intent:Open", uri);
    EventDispatcher.instance.sendRequest(msg);
  },

  _parseApps(appInfo) {
    // appInfo -> {apps: [app1Label, app1Default, app1PackageName, app1ActivityName, app2Label, app2Defaut, ...]}
    // see GeckoAppShell.java getHandlersForIntent function for details
    const numAttr = 4; // 4 elements per ResolveInfo: label, default, package name, activity name.

    let apps = [];
    for (let i = 0; i < appInfo.length; i += numAttr) {
      apps.push(
        new App({
          name: appInfo[i],
          isDefault: appInfo[i + 1],
          packageName: appInfo[i + 2],
          activityName: appInfo[i + 3],
        })
      );
    }

    return apps;
  },

  _getMessage(type, uri, options = {}) {
    let mimeType = options.mimeType;
    if (uri && mimeType == undefined) {
      mimeType = ContentAreaUtils.getMIMETypeForURI(uri) || "";
      if (
        uri.scheme != "file" &&
        NON_FILE_URI_IGNORED_MIME_TYPES.has(mimeType)
      ) {
        // We're guessing the MIME type based on the extension, which especially
        // with non-local HTML documents will yield inconsistent results, as those
        // commonly use URLs without any sort of extension, too.
        // At the same time, apps offering to handle certain URLs in lieu of a
        // browser often don't expect a MIME type to be used, and correspondingly
        // register their intent filters without a MIME type.
        // This means that when we *do* guess a non-empty MIME type because this
        // time the URL *did* end on .(x)htm(l), Android won't offer any apps whose
        // intent filter doesn't explicitly include that MIME type.
        // Therefore, if the MIME type looks like something from that category,
        // don't bother including it in the Intent for non-local files.
        mimeType = "";
      }
    }

    return {
      type: type,
      mime: mimeType,
      action: options.action || "", // empty action string defaults to android.intent.action.VIEW
      url: uri ? uri.displaySpec : "",
      packageName: options.packageName || "",
      className: options.className || "",
    };
  },

  _launchApp(app, uri, callback) {
    if (callback) {
      let msg = this._getMessage("Intent:OpenForResult", uri, {
        packageName: app.packageName,
        className: app.activityName,
      });

      EventDispatcher.instance.sendRequestForResult(msg).then(callback);
    } else {
      let msg = this._getMessage("Intent:Open", uri, {
        packageName: app.packageName,
        className: app.activityName,
      });

      EventDispatcher.instance.sendRequest(msg);
    }
  },
};
