/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that custom types can be defined and undefined

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

function run_test() {
  startupManager();

  do_check_false("test" in AddonManager.addonTypes);
  let types = AddonManager.addonTypes;

  // The dumbest provider possible
  var provider = {
  };

  var expectedAdd = "test";
  var expectedRemove = null;

  AddonManager.addTypeListener({
    onTypeAdded(aType) {
      do_check_eq(aType.id, expectedAdd);
      expectedAdd = null;
    },

    onTypeRemoved(aType) {
      do_check_eq(aType.id, expectedRemove);
      expectedRemove = null;
    }
  });

  AddonManagerPrivate.registerProvider(provider, [{
    id: "test",
    name: "Test",
    uiPriority: 1
  }, {
    id: "t$e%st",
    name: "Test",
    uiPriority: 1
  }]);

  do_check_eq(expectedAdd, null);

  do_check_true("test" in types);
  do_check_eq(types["test"].name, "Test");
  do_check_false("t$e%st" in types);

  delete types["test"];
  do_check_true("test" in types);

  types["foo"] = "bar";
  do_check_false("foo" in types);

  expectedRemove = "test";

  AddonManagerPrivate.unregisterProvider(provider);

  do_check_eq(expectedRemove, null);

  do_check_false("test" in AddonManager.addonTypes);
  // The cached reference to addonTypes is live
  do_check_false("test" in types);
}
