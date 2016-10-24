"use strict";

function testApiFactory(context) {
  return {
    test: {
      // These functions accept arbitrary values. Convert the parameters to
      // make sure that the values can be cloned structurally for IPC.

      sendMessage(...args) {
        args = Cu.cloneInto(args, context.cloneScope);
        context.childManager.callParentFunctionNoReturn("test.sendMessage", args);
      },

      assertTrue(value, msg) {
        context.childManager.callParentFunctionNoReturn("test.assertTrue", [
          Boolean(value),
          String(msg),
        ]);
      },

      assertFalse(value, msg) {
        context.childManager.callParentFunctionNoReturn("test.assertFalse", [
          Boolean(value),
          String(msg),
        ]);
      },

      assertEq(expected, actual, msg) {
        let equal = expected === actual;
        expected += "";
        actual += "";
        if (!equal && expected === actual) {
          // Add an extra tag so that "expected === actual" in the parent is
          // also false, despite the fact that the serialization is equal.
          actual += " (different)";
        }
        context.childManager.callParentFunctionNoReturn("test.assertEq", [
          expected,
          actual,
          String(msg),
        ]);
      },
    },
  };
}
extensions.registerSchemaAPI("test", "addon_child", testApiFactory);
extensions.registerSchemaAPI("test", "content_child", testApiFactory);

