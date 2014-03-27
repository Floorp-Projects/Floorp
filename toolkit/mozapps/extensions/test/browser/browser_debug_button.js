/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * Tests debug button for addons in list view
 */

let { Promise } = Components.utils.import("resource://gre/modules/Promise.jsm", {});
let { Task } = Components.utils.import("resource://gre/modules/Task.jsm", {});

const getDebugButton = node =>
    node.ownerDocument.getAnonymousElementByAttribute(node, "anonid", "debug-btn");
const addonDebuggingEnabled = bool =>
  Services.prefs.setBoolPref("devtools.debugger.addon-enabled", !!bool);
const remoteDebuggingEnabled = bool =>
  Services.prefs.setBoolPref("devtools.debugger.remote-enabled", !!bool);

function test() {
  requestLongerTimeout(2);

  waitForExplicitFinish();


  var gProvider = new MockProvider();
  gProvider.createAddons([{
    id: "non-debuggable@tests.mozilla.org",
    name: "No debug",
    description: "foo"
  },
  {
    id: "debuggable@tests.mozilla.org",
    name: "Debuggable",
    description: "bar",
    isDebuggable: true
  }]);

  Task.spawn(function* () {
    addonDebuggingEnabled(false);
    remoteDebuggingEnabled(false);

    yield testDOM((nondebug, debuggable) => {
      is(nondebug.disabled, true,
        "addon:disabled::remote:disabled button is disabled for legacy addons");
      is(nondebug.hidden, true,
        "addon:disabled::remote:disabled button is hidden for legacy addons");
      is(debuggable.disabled, true,
        "addon:disabled::remote:disabled button is disabled for debuggable addons");
      is(debuggable.hidden, true,
        "addon:disabled::remote:disabled button is hidden for debuggable addons");
    });
    
    addonDebuggingEnabled(true);
    remoteDebuggingEnabled(false);

    yield testDOM((nondebug, debuggable) => {
      is(nondebug.disabled, true,
        "addon:enabled::remote:disabled button is disabled for legacy addons");
      is(nondebug.disabled, true,
        "addon:enabled::remote:disabled button is hidden for legacy addons");
      is(debuggable.disabled, true,
        "addon:enabled::remote:disabled button is disabled for debuggable addons");
      is(debuggable.disabled, true,
        "addon:enabled::remote:disabled button is hidden for debuggable addons");
    });
    
    addonDebuggingEnabled(false);
    remoteDebuggingEnabled(true);

    yield testDOM((nondebug, debuggable) => {
      is(nondebug.disabled, true,
        "addon:disabled::remote:enabled button is disabled for legacy addons");
      is(nondebug.disabled, true,
        "addon:disabled::remote:enabled button is hidden for legacy addons");
      is(debuggable.disabled, true,
        "addon:disabled::remote:enabled button is disabled for debuggable addons");
      is(debuggable.disabled, true,
        "addon:disabled::remote:enabled button is hidden for debuggable addons");
    });
    
    addonDebuggingEnabled(true);
    remoteDebuggingEnabled(true);

    yield testDOM((nondebug, debuggable) => {
      is(nondebug.disabled, true,
        "addon:enabled::remote:enabled button is disabled for legacy addons");
      is(nondebug.disabled, true,
        "addon:enabled::remote:enabled button is hidden for legacy addons");
      is(debuggable.disabled, false,
        "addon:enabled::remote:enabled button is enabled for debuggable addons");
      is(debuggable.hidden, false,
        "addon:enabled::remote:enabled button is visible for debuggable addons");
    });

    finish();
  });

  function testDOM (testCallback) {
    let deferred = Promise.defer();
    open_manager("addons://list/extension", function(aManager) {
      const {document} = aManager;
      const addonList = document.getElementById("addon-list");
      const nondebug = addonList.querySelector("[name='No debug']");
      const debuggable = addonList.querySelector("[name='Debuggable']");

      testCallback.apply(null, [nondebug, debuggable].map(getDebugButton));

      close_manager(aManager, deferred.resolve);
    });
    return deferred.promise;
  }
}
