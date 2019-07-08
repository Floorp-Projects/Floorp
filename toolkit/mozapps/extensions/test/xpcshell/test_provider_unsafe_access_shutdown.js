createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

var shutdownOrder = [];

function mockAddonProvider(name) {
  let mockProvider = {
    hasShutdown: false,
    unsafeAccess: false,

    shutdownCallback: null,

    startup() {},
    shutdown() {
      this.hasShutdown = true;
      shutdownOrder.push(this.name);
      if (this.shutdownCallback) {
        return this.shutdownCallback();
      }
      return undefined;
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

add_task(async function unsafeProviderShutdown() {
  let firstProvider = mockAddonProvider("Mock1");
  AddonManagerPrivate.registerProvider(firstProvider);
  let secondProvider = mockAddonProvider("Mock2");
  AddonManagerPrivate.registerProvider(secondProvider);

  await promiseStartupManager();

  let shutdownPromise = null;
  await new Promise(resolve => {
    secondProvider.shutdownCallback = function() {
      return AddonManager.getAddonByID("does-not-exist").then(() => {
        resolve();
      });
    };

    shutdownPromise = promiseShutdownManager();
  });
  await shutdownPromise;

  equal(
    shutdownOrder.join(","),
    ["Mock1", "Mock2"].join(","),
    "Mock providers should have shutdown in expected order"
  );
  ok(
    !firstProvider.unsafeAccess,
    "First registered mock provider should not have been accessed unsafely"
  );
});
