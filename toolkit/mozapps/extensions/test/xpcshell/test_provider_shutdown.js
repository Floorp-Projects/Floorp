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

    get name() {
      return aName;
    },

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
}

// Helper to find a particular shutdown blocker's status in the JSON blob
function findInStatus(aStatus, aName) {
  for (let { name, state } of aStatus.state) {
    if (name == aName) {
      return state;
    }
  }
  return null;
}

/*
 * Make sure we report correctly when an add-on provider or AddonRepository block shutdown
 */
add_task(async function blockRepoShutdown() {
  // Reach into the AddonManager scope and inject our mock AddonRepository
  // the mock provider behaves enough like AddonRepository for the purpose of this test
  let mockRepo = mockAddonProvider("Mock repo");
  // Trigger the lazy getter so that we can assign a new value to it:
  void AMscope.AddonRepository;
  AMscope.AddonRepository = mockRepo;

  let mockProvider = mockAddonProvider("Mock provider");

  await promiseStartupManager();
  AddonManagerPrivate.registerProvider(mockProvider);

  let {
    fetchState,
  } = MockAsyncShutdown.profileBeforeChange.blockers[0].options;

  // Start shutting the manager down
  let managerDown = promiseShutdownManager();

  // Wait for manager to call provider shutdown.
  await mockProvider.shutdownPromise;
  // check AsyncShutdown state
  let status = fetchState();
  equal(findInStatus(status[1], "Mock provider"), "(none)");
  equal(status[2].name, "AddonRepository: async shutdown");
  equal(status[2].state, "pending");
  // let the provider finish
  mockProvider.doneResolve();

  // Wait for manager to call repo shutdown and start waiting for it
  await mockRepo.shutdownPromise;
  // Check the shutdown state
  status = fetchState();
  equal(status[1].name, "AddonManager: Waiting for providers to shut down.");
  equal(status[1].state, "Complete");
  equal(status[2].name, "AddonRepository: async shutdown");
  equal(status[2].state, "in progress");

  // Now finish our shutdown, and wait for the manager to wrap up
  mockRepo.doneResolve();
  await managerDown;

  // Check the shutdown state again
  status = fetchState();
  equal(status[0].name, "AddonRepository: async shutdown");
  equal(status[0].state, "done");
});
