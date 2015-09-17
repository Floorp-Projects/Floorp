function run_test() {
  Components.utils.import("resource://gre/modules/jsdebugger.jsm");
  addDebuggerToGlobal(this);
  var g = testGlobal("test");
  var dbg = new Debugger();
  var gw = dbg.addDebuggee(g);

  g.eval(`
    // This is not a CCW.
    Object.defineProperty(this, "bar", {
      get: function() { return "bar"; },
      configurable: true,
      enumerable: true
    });

    Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");

    // This is a CCW.
    XPCOMUtils.defineLazyGetter(this, "foo", function() { return "foo"; });
  `);

  // Neither scripted getter should be considered safe.
  assert(!DevToolsUtils.hasSafeGetter(gw.getOwnPropertyDescriptor("bar")));
  assert(!DevToolsUtils.hasSafeGetter(gw.getOwnPropertyDescriptor("foo")));
}
