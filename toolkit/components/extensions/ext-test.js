Cu.import("resource://gre/modules/ExtensionUtils.jsm");
let {
  EventManager,
} = ExtensionUtils;

let messageHandlers = new WeakMap();

extensions.on("test-message", (type, extension, ...args) => {
  let fire = messageHandlers.get(extension);
  if (fire) {
    fire(...args);
  }
});

extensions.registerAPI((extension, context) => {
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
        extension.emit("test-result", value ? true : false, msg);
      },

      assertFalse: function(value, msg) {
        extension.emit("test-result", !value ? true : false, msg);
      },

      assertEq: function(expected, actual, msg) {
        extension.emit("test-eq", expected === actual, msg, String(expected), String(actual));
      },

      onMessage: new EventManager(context, "test.onMessage", fire => {
        messageHandlers.set(extension, fire);
        return () => {
          messageHandlers.delete(extension);
        };
      }).api(),
    },
  };
});
