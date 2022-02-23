/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This verifies that custom types can be defined and undefined

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

add_task(async function setup() {
  await promiseStartupManager();
});

add_task(async function test_new_addonType() {
  Assert.equal(false, AddonManager.hasAddonType("test"));

  // The dumbest provider possible
  const provider = {};

  AddonManagerPrivate.registerProvider(provider, ["test"]);

  Assert.equal(true, AddonManager.hasAddonType("test"));
  Assert.equal(false, AddonManager.hasAddonType("t$e%st"));
  Assert.equal(false, AddonManager.hasAddonType(null));
  Assert.equal(false, AddonManager.hasAddonType(undefined));

  AddonManagerPrivate.unregisterProvider(provider);

  Assert.equal(false, AddonManager.hasAddonType("test"));
});

add_task(async function test_bad_addonType() {
  const provider = {};
  Assert.throws(
    () => AddonManagerPrivate.registerProvider(provider, /* addonTypes =*/ {}),
    /aTypes must be an array or null/
  );

  Assert.throws(
    () => AddonManagerPrivate.registerProvider(provider, new Set()),
    /aTypes must be an array or null/
  );
});

add_task(async function test_addonTypes_should_be_immutable() {
  const provider = {};
  const addonTypes = [];

  addonTypes.push("test");
  AddonManagerPrivate.registerProvider(provider, addonTypes);
  addonTypes.pop();
  addonTypes.push("test_added");
  // Modifications to addonTypes should not affect AddonManager.
  Assert.equal(true, AddonManager.hasAddonType("test"));
  Assert.equal(false, AddonManager.hasAddonType("test_added"));
  AddonManagerPrivate.unregisterProvider(provider);

  AddonManagerPrivate.registerProvider(provider, addonTypes);
  // After re-registering the provider, the type change should have been processed.
  Assert.equal(false, AddonManager.hasAddonType("test"));
  Assert.equal(true, AddonManager.hasAddonType("test_added"));
  AddonManagerPrivate.unregisterProvider(provider);
});

add_task(async function test_missing_addonType() {
  const dummyAddon = {
    id: "some dummy addon from provider without .addonTypes property",
  };
  const provider = {
    // addonTypes Set is missing. This only happens in unit tests, but let's
    // verify that the implementation behaves reasonably.
    // A provider without an explicitly registered type may still return an
    // entry when getAddonsByTypes is called.
    async getAddonsByTypes(types) {
      Assert.equal(null, types);
      return [dummyAddon];
    },
  };

  AddonManagerPrivate.registerProvider(provider); // addonTypes not set.
  Assert.equal(false, AddonManager.hasAddonType("test"));
  Assert.equal(false, AddonManager.hasAddonType(null));
  Assert.equal(false, AddonManager.hasAddonType(undefined));

  const addons = await AddonManager.getAddonsByTypes(null);
  Assert.equal(1, addons.length);
  Assert.equal(dummyAddon, addons[0]);

  AddonManagerPrivate.unregisterProvider(provider);
});

add_task(async function test_getAddonTypesByProvider() {
  let defaultTypes = AddonManagerPrivate.getAddonTypesByProvider("XPIProvider");
  Assert.ok(defaultTypes.includes("extension"), `extension in ${defaultTypes}`);
  Assert.throws(
    () => AddonManagerPrivate.getAddonTypesByProvider(),
    /No addonTypes found for provider: undefined/
  );
  Assert.throws(
    () => AddonManagerPrivate.getAddonTypesByProvider("MaybeExistent"),
    /No addonTypes found for provider: MaybeExistent/
  );

  const provider = { name: "MaybeExistent" };
  AddonManagerPrivate.registerProvider(provider, ["test"]);
  Assert.deepEqual(
    AddonManagerPrivate.getAddonTypesByProvider("MaybeExistent"),
    ["test"],
    "Newly registered type returned by getAddonTypesByProvider"
  );

  AddonManagerPrivate.unregisterProvider(provider);

  Assert.throws(
    () => AddonManagerPrivate.getAddonTypesByProvider("MaybeExistent"),
    /No addonTypes found for provider: MaybeExistent/
  );
});
