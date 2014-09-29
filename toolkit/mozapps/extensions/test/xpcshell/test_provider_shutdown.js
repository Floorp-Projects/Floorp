/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Verify that we report shutdown status for Addon Manager providers
// and AddonRepository correctly.

createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

// Make a mock AddonRepository that just lets us hang shutdown.
// Needs two promises - one to let us know that AM has called shutdown,
// and one for us to let AM know that shutdown is done.
function mockAddonProvider(aName) {
  let mockProvider = {
    donePromise: null,
    doneResolve: null,
    doneReject: null,
    shutdownPromise: null,
    shutdownResolve: null,

    get name() aName,

    shutdown() {
      this.shutdownResolve();
      return this.donePromise;
    },
  };
  mockProvider.donePromise = new Promise((resolve, reject) => {
    mockProvider.doneResolve = resolve;
    mockProvider.doneReject = reject;
  });
  mockProvider.shutdownPromise = new Promise((resolve, reject) => {
    mockProvider.shutdownResolve = resolve;
  });
  return mockProvider;
};

function run_test() {
  run_next_test();
}

// Helper to find a particular shutdown blocker's status in the JSON blob
function findInStatus(aStatus, aName) {
  for (let {name, state} of aStatus.state) {
    if (name == aName) {
      return state;
    }
  }
  return null;
}

/*
 * Make sure we report correctly when an add-on provider or AddonRepository block shutdown
 */
add_task(function* blockRepoShutdown() {
  // Reach into the AddonManager scope and inject our mock AddonRepository
  let realAddonRepo = AMscope.AddonRepository;
  // the mock provider behaves enough like AddonRepository for the purpose of this test
  let mockRepo = mockAddonProvider("Mock repo");
  AMscope.AddonRepository = mockRepo;

  let mockProvider = mockAddonProvider("Mock provider");

  startupManager();
  AddonManagerPrivate.registerProvider(mockProvider);

  // Start shutting the manager down
  let managerDown = promiseShutdownManager();

  // Wait for manager to call provider shutdown.
  yield mockProvider.shutdownPromise;
  // check AsyncShutdown state
  let status = MockAsyncShutdown.status();
  equal(findInStatus(status[0], "Mock provider"), "(none)");
  equal(status[1].name, "AddonRepository: async shutdown");
  equal(status[1].state, "pending");
  // let the provider finish
  mockProvider.doneResolve();

  // Wait for manager to call repo shutdown and start waiting for it
  yield mockRepo.shutdownPromise;
  // Check the shutdown state
  status = MockAsyncShutdown.status();
  equal(status[0].name, "AddonManager: Waiting for providers to shut down.");
  equal(status[0].state, "Complete");
  equal(status[1].name, "AddonRepository: async shutdown");
  equal(status[1].state, "in progress");

  // Now finish our shutdown, and wait for the manager to wrap up
  mockRepo.doneResolve();
  yield managerDown;

  // Check the shutdown state again
  status = MockAsyncShutdown.status();
  equal(status[0].name, "AddonRepository: async shutdown");
  equal(status[0].state, "done");
});
