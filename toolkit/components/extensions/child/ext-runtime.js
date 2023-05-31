/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  WebNavigationFrames: "resource://gre/modules/WebNavigationFrames.sys.mjs",
});

/* eslint-disable jsdoc/check-param-names */
/**
 * With optional arguments on both ends, this case is ambiguous:
 *     runtime.sendMessage("string", {} or nullish)
 *
 * Sending a message within the extension is more common than sending
 * an empty object to another extension, so we prefer that conclusion.
 *
 * @param {string?}  [extensionId]
 * @param {any}      message
 * @param {object?}  [options]
 * @param {Function} [callback]
 * @returns {{extensionId: string?, message: any, callback: Function?}}
 */
/* eslint-enable jsdoc/check-param-names */
function parseBonkersArgs(...args) {
  let Error = ExtensionUtils.ExtensionError;
  let callback = typeof args[args.length - 1] === "function" && args.pop();

  // We don't support any options anymore, so only an empty object is valid.
  function validOptions(v) {
    return v == null || (typeof v === "object" && !Object.keys(v).length);
  }

  if (args.length === 1 || (args.length === 2 && validOptions(args[1]))) {
    // Interpret as passing null for extensionId (message within extension).
    args.unshift(null);
  }
  let [extensionId, message, options] = args;

  if (!args.length) {
    throw new Error("runtime.sendMessage's message argument is missing");
  } else if (!validOptions(options)) {
    throw new Error("runtime.sendMessage's options argument is invalid");
  } else if (args.length === 4 && args[3] && !callback) {
    throw new Error("runtime.sendMessage's last argument is not a function");
  } else if (args[3] != null || args.length > 4) {
    throw new Error("runtime.sendMessage received too many arguments");
  } else if (extensionId && typeof extensionId !== "string") {
    throw new Error("runtime.sendMessage's extensionId argument is invalid");
  }
  return { extensionId, message, callback };
}

this.runtime = class extends ExtensionAPI {
  getAPI(context) {
    let { extension } = context;

    return {
      runtime: {
        onConnect: context.messenger.onConnect.api(),
        onMessage: context.messenger.onMessage.api(),

        onConnectExternal: context.messenger.onConnectEx.api(),
        onMessageExternal: context.messenger.onMessageEx.api(),

        connect(extensionId, options) {
          let name = options?.name ?? "";
          return context.messenger.connect({ name, extensionId });
        },

        sendMessage(...args) {
          let arg = parseBonkersArgs(...args);
          return context.messenger.sendRuntimeMessage(arg);
        },

        connectNative(name) {
          return context.messenger.connect({ name, native: true });
        },

        sendNativeMessage(nativeApp, message) {
          return context.messenger.sendNativeMessage(nativeApp, message);
        },

        get lastError() {
          return context.lastError;
        },

        getManifest() {
          return Cu.cloneInto(extension.manifest, context.cloneScope);
        },

        id: extension.id,

        getURL(url) {
          return extension.baseURI.resolve(url);
        },

        getFrameId(target) {
          let frameId = WebNavigationFrames.getFromWindow(target);
          if (frameId >= 0) {
            return frameId;
          }
          // Not a WindowProxy, perhaps an embedder element?

          let type;
          try {
            type = Cu.getClassName(target, true);
          } catch (e) {
            // Not a valid object, will throw below.
          }

          const embedderTypes = [
            "HTMLIFrameElement",
            "HTMLFrameElement",
            "HTMLEmbedElement",
            "HTMLObjectElement",
          ];

          if (embedderTypes.includes(type)) {
            if (!target.browsingContext) {
              return -1;
            }
            return WebNavigationFrames.getFrameId(target.browsingContext);
          }

          throw new ExtensionUtils.ExtensionError("Invalid argument");
        },
      },
    };
  }

  getAPIObjectForRequest(context, request) {
    if (request.apiObjectType === "Port") {
      const port = context.messenger.getPortById(request.apiObjectId);
      if (!port) {
        throw new Error(`Port API object not found: ${request}`);
      }
      return port.api;
    }

    throw new Error(`Unexpected apiObjectType: ${request}`);
  }
};
