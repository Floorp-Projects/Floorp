"use strict";

var { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AppConstants",
                                  "resource://gre/modules/AppConstants.jsm");

Cu.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
  ignoreEvent,
  runSafe,
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
        let options; // eslint-disable-line no-unused-vars
        let extensionId, message, responseCallback;
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

      getPlatformInfo: function(callback) {
        let os = AppConstants.platform;
        if (os == "macosx") {
          os = "mac";
        }

        let abi = Services.appinfo.XPCOMABI;
        let [arch] = abi.split("-");
        if (arch == "x86") {
          arch = "x86-32";
        } else if (arch == "x86_64") {
          arch = "x86-64";
        }

        let info = {os, arch};
        runSafe(context, callback, info);
      },
    },
  };
});
