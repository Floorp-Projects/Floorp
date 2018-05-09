createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

var startupOrder = [];

function mockAddonProvider(name) {
  let mockProvider = {
    hasStarted: false,
    unsafeAccess: false,

    startupCallback: null,

    startup() {
      this.hasStarted = true;
      startupOrder.push(this.name);
      if (this.startupCallback)
        this.startupCallback();
    },
    getAddonByID(id, callback) {
      if (!this.hasStarted) {
        this.unsafeAccess = true;
      }
      callback(null);
    },

    get name() {
      return name;
    },
  };

  return mockProvider;
}

add_task(async function unsafeProviderStartup() {
  let secondProvider = null;

  await new Promise(resolve => {
    let firstProvider = mockAddonProvider("Mock1");
    firstProvider.startupCallback = function() {
      resolve(AddonManager.getAddonByID("does-not-exist"));
    };
    AddonManagerPrivate.registerProvider(firstProvider);

    secondProvider = mockAddonProvider("Mock2");
    AddonManagerPrivate.registerProvider(secondProvider);

    promiseStartupManager();
  });

  equal(startupOrder.join(","), ["Mock1", "Mock2"].join(","), "Mock providers should have hasStarted in expected order");
  ok(!secondProvider.unsafeAccess, "Second registered mock provider should not have been accessed unsafely");
});
