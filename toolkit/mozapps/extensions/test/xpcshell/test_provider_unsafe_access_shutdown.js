createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

var shutdownOrder = [];

function mockAddonProvider(name) {
  let mockProvider = {
    hasShutdown: false,
    unsafeAccess: false,

    shutdownCallback: null,

    startup() { },
    shutdown() {
      this.hasShutdown = true;
      shutdownOrder.push(this.name);
      if (this.shutdownCallback)
        return this.shutdownCallback();
    },
    getAddonByID(id, callback) {
      if (this.hasShutdown) {
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

add_task(function* unsafeProviderShutdown() {
  let firstProvider = mockAddonProvider("Mock1");
  AddonManagerPrivate.registerProvider(firstProvider);
  let secondProvider = mockAddonProvider("Mock2");
  AddonManagerPrivate.registerProvider(secondProvider);

  startupManager();

  let shutdownPromise = null;
  yield new Promise(resolve => {
    secondProvider.shutdownCallback = function() {
      return new Promise(shutdownResolve => {
        AddonManager.getAddonByID("does-not-exist", () => {
          shutdownResolve();
          resolve();
        });
      });
    };

    shutdownPromise = promiseShutdownManager();
  });
  yield shutdownPromise;

  equal(shutdownOrder.join(","), ["Mock1", "Mock2"].join(","), "Mock providers should have shutdown in expected order");
  ok(!firstProvider.unsafeAccess, "First registered mock provider should not have been accessed unsafely");
});
