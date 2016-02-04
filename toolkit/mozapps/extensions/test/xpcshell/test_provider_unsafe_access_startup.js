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

function run_test() {
  run_next_test();
}

add_task(function* unsafeProviderStartup() {
  let secondProvider = null;

  yield new Promise(resolve => {
    let firstProvider = mockAddonProvider("Mock1");
    firstProvider.startupCallback = function() {
      AddonManager.getAddonByID("does-not-exist", resolve);
    };
    AddonManagerPrivate.registerProvider(firstProvider);

    secondProvider = mockAddonProvider("Mock2");
    AddonManagerPrivate.registerProvider(secondProvider);

    startupManager();
  });

  equal(startupOrder.join(","), ["Mock1", "Mock2"].join(","), "Mock providers should have hasStarted in expected order");
  ok(!secondProvider.unsafeAccess, "Second registered mock provider should not have been accessed unsafely");
});
