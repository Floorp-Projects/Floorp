"use strict";

Components.utils.import("resource://gre/modules/ExtensionUtils.jsm");
var {
  EventManager,
} = ExtensionUtils;

// WeakMap[Extension -> Set(callback)]
var messageHandlers = new WeakMap();

/* eslint-disable mozilla/balanced-listeners */
extensions.on("startup", (type, extension) => {
  messageHandlers.set(extension, new Set());
});

extensions.on("shutdown", (type, extension) => {
  messageHandlers.delete(extension);
});

extensions.on("test-message", (type, extension, ...args) => {
  let handlers = messageHandlers.get(extension);
  for (let handler of handlers) {
    handler(...args);
  }
});
/* eslint-enable mozilla/balanced-listeners */

function makeTestAPI(context) {
  let {extension} = context;
  return {
    test: {
      sendMessage: function(...args) {
        extension.emit("test-message", ...args);
      },

      notifyPass: function(msg) {
        extension.emit("test-done", true, msg);
      },

      notifyFail: function(msg) {
        extension.emit("test-done", false, msg);
      },

      log: function(msg) {
        extension.emit("test-log", true, msg);
      },

      fail: function(msg) {
        extension.emit("test-result", false, msg);
      },

      succeed: function(msg) {
        extension.emit("test-result", true, msg);
      },

      assertTrue: function(value, msg) {
        extension.emit("test-result", Boolean(value), String(msg));
      },

      assertFalse: function(value, msg) {
        extension.emit("test-result", !value, String(msg));
      },

      assertEq: function(expected, actual, msg) {
        let equal = expected === actual;
        expected += "";
        actual += "";
        if (!equal && expected === actual) {
          actual += " (different)";
        }
        extension.emit("test-eq", equal, String(msg), expected, actual);
      },

      onMessage: new EventManager(context, "test.onMessage", fire => {
        let handlers = messageHandlers.get(extension);
        handlers.add(fire);

        return () => {
          handlers.delete(fire);
        };
      }).api(),
    },
  };
}
extensions.registerSchemaAPI("test", "addon_parent", makeTestAPI);
extensions.registerSchemaAPI("test", "content_parent", makeTestAPI);
