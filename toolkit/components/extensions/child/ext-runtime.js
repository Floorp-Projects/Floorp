/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.runtime = class extends ExtensionAPI {
  getAPI(context) {
    let { extension } = context;

    return {
      runtime: {
        onConnect: context.messenger.nm.onConnect.api(),

        onMessage: context.messenger.onMessage("runtime.onMessage"),

        onConnectExternal: context.messenger.nm.onConnectEx.api(),

        onMessageExternal: context.messenger.onMessageExternal(
          "runtime.onMessageExternal"
        ),

        connect(extensionId, options) {
          let { name = "" } = options || {};
          extensionId = extensionId || extension.id;
          return context.messenger.nm.connect({ name, extensionId });
        },

        sendMessage(...args) {
          let extensionId, message, options, responseCallback;

          if (typeof args[args.length - 1] === "function") {
            responseCallback = args.pop();
          }

          function checkOptions(options) {
            if (typeof options !== "object") {
              return [
                false,
                "runtime.sendMessage's options argument is invalid",
              ];
            }

            for (let key of Object.keys(options)) {
              return [false, `Unexpected property ${key}`];
            }

            return [true, {}];
          }

          if (!args.length) {
            return Promise.reject({
              message: "runtime.sendMessage's message argument is missing",
            });
          } else if (args.length === 1) {
            message = args[0];
          } else if (args.length === 2) {
            // With two optional arguments, this is the ambiguous case,
            // particularly sendMessage("string", {} or null)
            // Given that sending a message within the extension is generally
            // more common than sending the empty object to another extension,
            // we prefer that conclusion, as long as the second argument looks
            // like valid options object, or is null/undefined.
            let [validOpts] = checkOptions(args[1]);
            if (validOpts || args[1] == null) {
              [message, options] = args;
            } else {
              [extensionId, message] = args;
            }
          } else if (
            args.length === 3 ||
            (args.length === 4 && args[3] == null)
          ) {
            [extensionId, message, options] = args;
          } else if (args.length === 4 && !responseCallback) {
            return Promise.reject({
              message: "runtime.sendMessage's last argument is not a function",
            });
          } else {
            return Promise.reject({
              message: "runtime.sendMessage received too many arguments",
            });
          }

          if (extensionId != null && typeof extensionId !== "string") {
            return Promise.reject({
              message: "runtime.sendMessage's extensionId argument is invalid",
            });
          }

          extensionId = extensionId || extension.id;
          let recipient = { extensionId };

          if (options != null) {
            let [valid, arg] = checkOptions(options);
            if (!valid) {
              return Promise.reject({ message: arg });
            }
            Object.assign(recipient, arg);
          }

          return context.messenger.sendMessage(
            context.messageManager,
            message,
            recipient,
            responseCallback
          );
        },

        connectNative(name) {
          return context.messenger.nm.connect({ name, native: true });
        },

        sendNativeMessage(nativeApp, message) {
          return context.messenger.nm.sendNativeMessage(nativeApp, message);
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
      },
    };
  }
};
