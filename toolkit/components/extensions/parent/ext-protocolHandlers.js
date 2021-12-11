/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "handlerService",
  "@mozilla.org/uriloader/handler-service;1",
  "nsIHandlerService"
);
XPCOMUtils.defineLazyServiceGetter(
  this,
  "protocolService",
  "@mozilla.org/uriloader/external-protocol-service;1",
  "nsIExternalProtocolService"
);

const hasHandlerApp = handlerConfig => {
  let protoInfo = protocolService.getProtocolHandlerInfo(
    handlerConfig.protocol
  );
  let appHandlers = protoInfo.possibleApplicationHandlers;
  for (let i = 0; i < appHandlers.length; i++) {
    let handler = appHandlers.queryElementAt(i, Ci.nsISupports);
    if (
      handler instanceof Ci.nsIWebHandlerApp &&
      handler.uriTemplate === handlerConfig.uriTemplate
    ) {
      return true;
    }
  }
  return false;
};

this.protocolHandlers = class extends ExtensionAPI {
  onManifestEntry(entryName) {
    let { extension } = this;
    let { manifest } = extension;

    for (let handlerConfig of manifest.protocol_handlers) {
      if (hasHandlerApp(handlerConfig)) {
        continue;
      }

      let handler = Cc[
        "@mozilla.org/uriloader/web-handler-app;1"
      ].createInstance(Ci.nsIWebHandlerApp);
      handler.name = handlerConfig.name;
      handler.uriTemplate = handlerConfig.uriTemplate;

      let protoInfo = protocolService.getProtocolHandlerInfo(
        handlerConfig.protocol
      );
      let handlers = protoInfo.possibleApplicationHandlers;
      if (protoInfo.preferredApplicationHandler || handlers.length) {
        protoInfo.alwaysAskBeforeHandling = true;
      } else {
        protoInfo.preferredApplicationHandler = handler;
        protoInfo.alwaysAskBeforeHandling = false;
      }
      handlers.appendElement(handler);
      handlerService.store(protoInfo);
    }
  }

  onShutdown(isAppShutdown) {
    let { extension } = this;
    let { manifest } = extension;

    if (isAppShutdown) {
      return;
    }

    for (let handlerConfig of manifest.protocol_handlers) {
      let protoInfo = protocolService.getProtocolHandlerInfo(
        handlerConfig.protocol
      );
      let appHandlers = protoInfo.possibleApplicationHandlers;
      for (let i = 0; i < appHandlers.length; i++) {
        let handler = appHandlers.queryElementAt(i, Ci.nsISupports);
        if (
          handler instanceof Ci.nsIWebHandlerApp &&
          handler.uriTemplate === handlerConfig.uriTemplate
        ) {
          appHandlers.removeElementAt(i);
          if (protoInfo.preferredApplicationHandler === handler) {
            protoInfo.preferredApplicationHandler = null;
            protoInfo.alwaysAskBeforeHandling = true;
          }
          handlerService.store(protoInfo);
          break;
        }
      }
    }
  }
};
