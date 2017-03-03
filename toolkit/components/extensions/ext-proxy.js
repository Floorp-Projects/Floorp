/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */

"use strict";

var {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/ExtensionUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "ProxyScriptContext",
                                  "resource://gre/modules/ProxyScriptContext.jsm");

var {
  SingletonEventManager,
} = ExtensionUtils;

// WeakMap[Extension -> ProxyScriptContext]
let proxyScriptContextMap = new WeakMap();

/* eslint-disable mozilla/balanced-listeners */
extensions.on("shutdown", (type, extension) => {
  let proxyScriptContext = proxyScriptContextMap.get(extension);
  if (proxyScriptContext) {
    proxyScriptContext.unload();
    proxyScriptContextMap.delete(extension);
  }
});
/* eslint-enable mozilla/balanced-listeners */

extensions.registerSchemaAPI("proxy", "addon_parent", context => {
  let {extension} = context;
  return {
    proxy: {
      registerProxyScript: (url) => {
        // Unload the current proxy script if one is loaded.
        if (proxyScriptContextMap.has(extension)) {
          proxyScriptContextMap.get(extension).unload();
          proxyScriptContextMap.delete(extension);
        }

        let proxyScriptContext = new ProxyScriptContext(extension, url);
        if (proxyScriptContext.load()) {
          proxyScriptContextMap.set(extension, proxyScriptContext);
        }
      },

      onProxyError: new SingletonEventManager(context, "proxy.onProxyError", fire => {
        let listener = (name, error) => {
          fire.async(error);
        };
        extension.on("proxy-error", listener);
        return () => {
          extension.off("proxy-error", listener);
        };
      }).api(),
    },
  };
});
