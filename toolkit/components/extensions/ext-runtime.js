var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
  ignoreEvent,
} = ExtensionUtils;

function processRuntimeConnectParams(win, ...args) {
  let extensionId, connectInfo;

  // connect("...") and connect("...", { ... })
  if (typeof args[0] == "string") {
    extensionId = args.shift();
  }

  // connect({ ... }) and connect("...", { ... })
  if (!!args[0] && typeof args[0] == "object") {
    connectInfo = args.shift();
  }

  // raise errors on unexpected connect params (but connect() is ok)
  if (args.length > 0) {
    throw win.Error("invalid arguments to runtime.connect");
  }

  return { extensionId, connectInfo };
}

extensions.registerAPI((extension, context) => {
  return {
    runtime: {
      onStartup: new EventManager(context, "runtime.onStartup", fire => {
        extension.onStartup = fire;
        return () => {
          extension.onStartup = null;
        };
      }).api(),

      onInstalled: ignoreEvent(context, "runtime.onInstalled"),

      onMessage: context.messenger.onMessage("runtime.onMessage"),

      onConnect: context.messenger.onConnect("runtime.onConnect"),

      connect: function(...args) {
        let { extensionId, connectInfo } = processRuntimeConnectParams(context.contentWindow, ...args);

        let name = connectInfo && connectInfo.name || "";
        let recipient = extensionId ? {extensionId} : {extensionId: extension.id};

        return context.messenger.connect(Services.cpmm, name, recipient);
      },

      sendMessage: function(...args) {
        let extensionId, message, options, responseCallback;
        if (args.length == 1) {
          message = args[0];
        } else if (args.length == 2) {
          [message, responseCallback] = args;
        } else {
          [extensionId, message, options, responseCallback] = args;
        }
        let recipient = {extensionId: extensionId ? extensionId : extension.id};
        return context.messenger.sendMessage(Services.cpmm, message, recipient, responseCallback);
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
});
