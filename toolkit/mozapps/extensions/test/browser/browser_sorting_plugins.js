/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests that sorting of plugins works correctly
// (this test checks that plugins with "ask to activate" state appear after those with
//  "always activate" and before those with "never activate")

var gManagerWindow;
var gProvider;

// This test is testing XUL about:addons UI features that are not supported in the
// HTML about:addons.
SpecialPowers.pushPrefEnv({
  set: [["extensions.htmlaboutaddons.enabled", false]],
});

async function test() {
  waitForExplicitFinish();

  gProvider = new MockProvider();
  gProvider.createAddons([{
    //  enabledInstalled group
    //    * Always activate
    //    * Ask to activate
    //    * Never activate
    id: "test1@tests.mozilla.org",
    name: "Java Applet Plug-in Java 7 Update 51",
    description: "foo",
    type: "plugin",
    isActive: true,
    userDisabled: AddonManager.STATE_ASK_TO_ACTIVATE,
  }, {
    id: "test2@tests.mozilla.org",
    name: "Quick Time Plug-in",
    description: "foo",
    type: "plugin",
    isActive: true,
    userDisabled: false,
  }, {
    id: "test3@tests.mozilla.org",
    name: "Shockwave Flash",
    description: "foo",
    type: "plugin",
    isActive: false,
    userDisabled: true,
  }, {
    id: "test4@tests.mozilla.org",
    name: "Adobe Reader Plug-in",
    description: "foo",
    type: "plugin",
    isActive: true,
    userDisabled: AddonManager.STATE_ASK_TO_ACTIVATE,
  }, {
    id: "test5@tests.mozilla.org",
    name: "3rd Party Plug-in",
    description: "foo",
    type: "plugin",
    isActive: true,
    userDisabled: false,
  }]);

  let aWindow = await open_manager("addons://list/plugin");
  gManagerWindow = aWindow;
  run_next_test();
}

async function end_test() {
  await close_manager(gManagerWindow);
  finish();
}

function check_order(aExpectedOrder) {
  var order = [];
  var list = gManagerWindow.document.getElementById("addon-list");
  var node = list.firstChild;
  while (node) {
    var id = node.getAttribute("value");
    if (id && id.endsWith("@tests.mozilla.org"))
      order.push(node.getAttribute("value"));
    node = node.nextSibling;
  }

  is(order.toSource(), aExpectedOrder.toSource(), "Should have seen the right order");
}

// Tests that ascending name ordering was the default
add_test(function() {
  check_order([
    "test5@tests.mozilla.org",
    "test2@tests.mozilla.org",
    "test4@tests.mozilla.org",
    "test1@tests.mozilla.org",
    "test3@tests.mozilla.org",
  ]);

  run_next_test();
});
