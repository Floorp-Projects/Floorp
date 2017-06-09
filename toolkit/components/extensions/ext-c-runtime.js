"use strict";

this.runtime = class extends ExtensionAPI {
  getAPI(context) {
    let {extension} = context;

    return {
      runtime: {
        onConnect: context.messenger.onConnect("runtime.onConnect"),

        onMessage: context.messenger.onMessage("runtime.onMessage"),

        onConnectExternal: context.messenger.onConnectExternal("runtime.onConnectExternal"),

        onMessageExternal: context.messenger.onMessageExternal("runtime.onMessageExternal"),

        connect: function(extensionId, connectInfo) {
          let name = (connectInfo !== null && connectInfo.name) || "";
          extensionId = extensionId || extension.id;
          let recipient = {extensionId};

          return context.messenger.connect(context.messageManager, name, recipient);
        },

        sendMessage(...args) {
          let extensionId, message, options, responseCallback;

          if (typeof args[args.length - 1] === "function") {
            responseCallback = args.pop();
          }

          function checkOptions(options) {
            let toProxyScript = false;
            if (typeof options !== "object") {
              return [false, "runtime.sendMessage's options argument is invalid"];
            }

            for (let key of Object.keys(options)) {
              if (key === "toProxyScript") {
                let value = options[key];
                if (typeof value !== "boolean") {
                  return [false, "runtime.sendMessage's options.toProxyScript argument is invalid"];
                }
                toProxyScript = value;
              } else {
                return [false, `Unexpected property ${key}`];
              }
            }

            return [true, {toProxyScript}];
          }

          if (!args.length) {
            return Promise.reject({message: "runtime.sendMessage's message argument is missing"});
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
          } else if (args.length === 3 || (args.length === 4 && args[3] == null)) {
            [extensionId, message, options] = args;
          } else if (args.length === 4 && !responseCallback) {
            return Promise.reject({message: "runtime.sendMessage's last argument is not a function"});
          } else {
            return Promise.reject({message: "runtime.sendMessage received too many arguments"});
          }

          if (extensionId != null && typeof extensionId !== "string") {
            return Promise.reject({message: "runtime.sendMessage's extensionId argument is invalid"});
          }

          extensionId = extensionId || extension.id;
          let recipient = {extensionId};

          if (options != null) {
            let [valid, arg] = checkOptions(options);
            if (!valid) {
              return Promise.reject({message: arg});
            }
            Object.assign(recipient, arg);
          }

          return context.messenger.sendMessage(context.messageManager, message, recipient, responseCallback);
        },

        connectNative(application) {
          let recipient = {
            childId: context.childManager.id,
            toNativeApp: application,
          };

          return context.messenger.connectNative(context.messageManager, "", recipient);
        },

        sendNativeMessage(application, message) {
          let recipient = {
            childId: context.childManager.id,
            toNativeApp: application,
          };
          return context.messenger.sendNativeMessage(context.messageManager, message, recipient);
        },

        get lastError() {
          return context.lastError;
        },

        getManifest() {
          return Cu.cloneInto(extension.manifest, context.cloneScope);
        },

        id: extension.id,

        getURL: function(url) {
          return extension.baseURI.resolve(url);
        },
      },
    };
  }
};
