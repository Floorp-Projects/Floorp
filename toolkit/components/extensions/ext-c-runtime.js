"use strict";

function runtimeApiFactory(context) {
  let {extension} = context;

  // TODO(robwu): Investigate which message-manager to use once we start with
  // reworking Messenger and ExtensionContext to be usable in a child process
  // instead of the parent process.
  // For now use exactly the original behavior out of caution.
  let mm = context.envType == "content_child" ? context.messageManager : Services.cpmm;

  return {
    runtime: {
      onConnect: context.messenger.onConnect("runtime.onConnect"),

      onMessage: context.messenger.onMessage("runtime.onMessage"),

      connect: function(extensionId, connectInfo) {
        let name = connectInfo !== null && connectInfo.name || "";
        extensionId = extensionId || extension.id;
        let recipient = {extensionId};

        return context.messenger.connect(mm, name, recipient);
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

        return context.messenger.sendMessage(mm, message, recipient, responseCallback);
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
