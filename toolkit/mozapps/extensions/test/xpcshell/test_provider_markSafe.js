createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

function mockAddonProvider(name) {
  let mockProvider = {
    markSafe: false,
    apiAccessed: false,

    startup() {
      if (this.markSafe)
        AddonManagerPrivate.markProviderSafe(this);

      AddonManager.isInstallEnabled("made-up-mimetype");
    },
    supportsMimetype(mimetype) {
      this.apiAccessed = true;
      return false;
    },

    get name() {
      return name;
    },
  };

  return mockProvider;
}

add_task(async function testMarkSafe() {
  info("Starting with provider normally");
  let provider = mockAddonProvider("Mock1");
  AddonManagerPrivate.registerProvider(provider);
  await promiseStartupManager();
  ok(!provider.apiAccessed, "Provider API should not have been accessed");
  AddonManagerPrivate.unregisterProvider(provider);
  await promiseShutdownManager();

  info("Starting with provider that marks itself safe");
  provider.apiAccessed = false;
  provider.markSafe = true;
  AddonManagerPrivate.registerProvider(provider);
  await promiseStartupManager();
  ok(provider.apiAccessed, "Provider API should have been accessed");
});
