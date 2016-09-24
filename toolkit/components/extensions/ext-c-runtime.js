"use strict";
XPCOMUtils.defineLazyModuleGetter(this, "NativeApp",
                                  "resource://gre/modules/NativeMessaging.jsm");

function runtimeApiFactory(context) {
  let {extension} = context;

  return {
    runtime: {
      onConnect: context.messenger.onConnect("runtime.onConnect"),

      onMessage: context.messenger.onMessage("runtime.onMessage"),

      connect: function(extensionId, connectInfo) {
        let name = connectInfo !== null && connectInfo.name || "";
        extensionId = extensionId || extension.id;
        let recipient = {extensionId};

        return context.messenger.connect(context.messageManager, name, recipient);
      },

      sendMessage: function(...args) {
        let options; // eslint-disable-line no-unused-vars
        let extensionId, message, responseCallback;
        if (typeof args[args.length - 1] == "function") {
          responseCallback = args.pop();
        }
        if (!args.length) {
          return Promise.reject({message: "runtime.sendMessage's message argument is missing"});
        } else if (args.length == 1) {
          message = args[0];
        } else if (args.length == 2) {
          if (typeof args[0] == "string" && args[0]) {
            [extensionId, message] = args;
          } else {
            [message, options] = args;
          }
        } else if (args.length == 3) {
          [extensionId, message, options] = args;
        } else if (args.length == 4 && !responseCallback) {
          return Promise.reject({message: "runtime.sendMessage's last argument is not a function"});
        } else {
          return Promise.reject({message: "runtime.sendMessage received too many arguments"});
        }

        if (extensionId != null && typeof extensionId != "string") {
          return Promise.reject({message: "runtime.sendMessage's extensionId argument is invalid"});
        }
        if (options != null && typeof options != "object") {
          return Promise.reject({message: "runtime.sendMessage's options argument is invalid"});
        }
        // TODO(robwu): Validate option keys and values when we support it.

        extensionId = extensionId || extension.id;
        let recipient = {extensionId};

        return context.messenger.sendMessage(context.messageManager, message, recipient, responseCallback);
      },

      connectNative(application) {
        let recipient = {
          childId: context.childManager.id,
          toNativeApp: application,
        };
        let rawPort = context.messenger.connectGetRawPort(context.messageManager, "", recipient);
        let port = rawPort.api();
        port.postMessage = message => {
          message = NativeApp.encodeMessage(context, message);
          rawPort.postMessage(message);
        };
        return port;
      },

      sendNativeMessage(application, message) {
        let recipient = {
          childId: context.childManager.id,
          toNativeApp: application,
        };
        message = NativeApp.encodeMessage(context, message);
        return context.messenger.sendMessage(context.messageManager, message, recipient);
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

extensions.registerSchemaAPI("runtime", "addon_child", runtimeApiFactory);
extensions.registerSchemaAPI("runtime", "content_child", runtimeApiFactory);
