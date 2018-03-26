/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.defineModuleGetter(this, "ProxyScriptContext",
                               "resource://gre/modules/ProxyScriptContext.jsm");
ChromeUtils.defineModuleGetter(this, "ProxyChannelFilter",
                               "resource://gre/modules/ProxyScriptContext.jsm");

// WeakMap[Extension -> ProxyScriptContext]
const proxyScriptContextMap = new WeakMap();

// EventManager-like class specifically for Proxy filters. Inherits from
// EventManager. Takes care of converting |details| parameter
// when invoking listeners.
class ProxyFilterEventManager extends EventManager {
  constructor(context, eventName) {
    let name = `proxy.${eventName}`;
    let register = (fire, filterProps, extraInfoSpec = []) => {
      let listener = (data) => {
        return fire.sync(data);
      };

      let filter = {...filterProps};
      if (filter.urls) {
        let perms = new MatchPatternSet([...context.extension.whiteListedHosts.patterns,
                                         ...context.extension.optionalOrigins.patterns]);

        filter.urls = new MatchPatternSet(filter.urls);

        if (!perms.overlapsAll(filter.urls)) {
          throw new context.cloneScope.Error("The proxy.addListener filter doesn't overlap with host permissions.");
        }
      }

      let proxyFilter = new ProxyChannelFilter(context, listener, filter, extraInfoSpec);
      return () => {
        proxyFilter.destroy();
      };
    };

    super(context, name, register);
  }
}

this.proxy = class extends ExtensionAPI {
  onShutdown() {
    let {extension} = this;

    let proxyScriptContext = proxyScriptContextMap.get(extension);
    if (proxyScriptContext) {
      proxyScriptContext.unload();
      proxyScriptContextMap.delete(extension);
    }
  }

  getAPI(context) {
    let {extension} = context;

    let onError = new EventManager(context, "proxy.onError", fire => {
      let listener = (name, error) => {
        fire.async(error);
      };
      extension.on("proxy-error", listener);
      return () => {
        extension.off("proxy-error", listener);
      };
    }).api();

    return {
      proxy: {
        register(url) {
          this.unregister();

          let proxyScriptContext = new ProxyScriptContext(extension, url);
          if (proxyScriptContext.load()) {
            proxyScriptContextMap.set(extension, proxyScriptContext);
          }
        },

        unregister() {
          // Unload the current proxy script if one is loaded.
          if (proxyScriptContextMap.has(extension)) {
            proxyScriptContextMap.get(extension).unload();
            proxyScriptContextMap.delete(extension);
          }
        },

        registerProxyScript(url) {
          this.register(url);
        },

        onRequest: new ProxyFilterEventManager(context, "onRequest").api(),

        onError,

        // TODO Bug 1388619 deprecate onProxyError.
        onProxyError: onError,
      },
    };
  }
};
