/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that custom types can be defined and undefined

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

add_task(async function setup() {
  await promiseStartupManager();

  Assert.equal(false, "test" in AddonManager.addonTypes);
  let types = AddonManager.addonTypes;

  // The dumbest provider possible
  var provider = {
  };

  var expectedAdd = "test";
  var expectedRemove = null;

  AddonManager.addTypeListener({
    onTypeAdded(aType) {
      Assert.equal(aType.id, expectedAdd);
      expectedAdd = null;
    },

    onTypeRemoved(aType) {
      Assert.equal(aType.id, expectedRemove);
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

  Assert.equal(expectedAdd, null);

  Assert.ok("test" in types);
  Assert.equal(types.test.name, "Test");
  Assert.equal(false, "t$e%st" in types);

  delete types.test;
  Assert.ok("test" in types);

  types.foo = "bar";
  Assert.equal(false, "foo" in types);

  expectedRemove = "test";

  AddonManagerPrivate.unregisterProvider(provider);

  Assert.equal(expectedRemove, null);

  Assert.equal(false, "test" in AddonManager.addonTypes);
  // The cached reference to addonTypes is live
  Assert.equal(false, "test" in types);
});
