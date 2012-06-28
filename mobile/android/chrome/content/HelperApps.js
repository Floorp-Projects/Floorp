/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";


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
    let handlerInfoProto = this.urlHandlerService.getURLHandlerInfoFromOS(uri, {});
    let urlHandlers = handlerInfoProto.possibleApplicationHandlers;
    for (var i = 0; i < urlHandlers.length; i++) {
      let urlApp = urlHandlers.queryElementAt(i, Ci.nsIHandlerApp);
      if (!this.defaultHttpHandlers[urlApp.name]) {
        found.push(urlApp);
      }
    }
    return found;
  },
  
  openUriInApp: function openUriInApp(uri) { 
    var possibleHandlers = this.getAppsForUri(uri);
    if (possibleHandlers.length == 1) {
      possibleHandlers[0].launchWithURI(uri);
    } else if (possibleHandlers.length > 0) {
      let handlerInfoProto = this.urlHandlerService.getURLHandlerInfoFromOS(uri, {});
      handlerInfoProto.preferredApplicationHandler.launchWithURI(uri);
    }
  }
};
