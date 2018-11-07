"use strict";

ChromeUtils.import("resource://testing-common/TestUtils.jsm", this);
ChromeUtils.import("resource://normandy/lib/RecipeRunner.jsm", this);
ChromeUtils.import("resource://normandy/lib/ClientEnvironment.jsm", this);
ChromeUtils.import("resource://normandy/lib/CleanupManager.jsm", this);
ChromeUtils.import("resource://normandy/lib/NormandyApi.jsm", this);
ChromeUtils.import("resource://normandy/lib/ActionSandboxManager.jsm", this);
ChromeUtils.import("resource://normandy/lib/ActionsManager.jsm", this);
ChromeUtils.import("resource://normandy/lib/AddonStudies.jsm", this);
ChromeUtils.import("resource://normandy/lib/Uptake.jsm", this);

add_task(async function getFilterContext() {
  const recipe = {id: 17, arguments: {foo: "bar"}, unrelated: false};
  const context = RecipeRunner.getFilterContext(recipe);

  // Test for expected properties in the filter expression context.
  const expectedNormandyKeys = [
    "channel",
    "country",
    "distribution",
    "doNotTrack",
    "isDefaultBrowser",
    "locale",
    "plugins",
    "recipe",
    "request_time",
    "searchEngine",
    "syncDesktopDevices",
    "syncMobileDevices",
    "syncSetup",
    "syncTotalDevices",
    "telemetry",
    "userId",
    "version",
  ];
  for (const key of expectedNormandyKeys) {
    ok(key in context.normandy, `normandy.${key} is available`);
  }

  is(
    context.normandy.recipe.id,
    recipe.id,
    "normandy.recipe is the recipe passed to getFilterContext",
  );
  delete recipe.unrelated;
  Assert.deepEqual(
    context.normandy.recipe,
    recipe,
    "normandy.recipe drops unrecognized attributes from the recipe",
  );

  // Filter context attributes are cached.
  await SpecialPowers.pushPrefEnv({set: [["app.normandy.user_id", "some id"]]});
  is(context.normandy.userId, "some id", "User id is read from prefs when accessed");
  await SpecialPowers.pushPrefEnv({set: [["app.normandy.user_id", "real id"]]});
  is(context.normandy.userId, "some id", "userId was cached");

});

add_task(async function checkFilter() {
  const check = filter => RecipeRunner.checkFilter({filter_expression: filter});

  // Errors must result in a false return value.
  ok(!(await check("invalid ( + 5yntax")), "Invalid filter expressions return false");

  // Non-boolean filter results result in a true return value.
  ok(await check("[1, 2, 3]"), "Non-boolean filter expressions return true");

  // The given recipe must be available to the filter context.
  const recipe = {filter_expression: "normandy.recipe.id == 7", id: 7};
  ok(await RecipeRunner.checkFilter(recipe), "The recipe is available in the filter context");
  recipe.id = 4;
  ok(!(await RecipeRunner.checkFilter(recipe)), "The recipe is available in the filter context");
});

decorate_task(
  withMockNormandyApi,
  withStub(ClientEnvironment, "getClientClassification"),
  async function testClientClassificationCache(api, getStub) {
    getStub.returns(Promise.resolve(false));

    await SpecialPowers.pushPrefEnv({set: [
      ["app.normandy.api_url",
       "https://example.com/selfsupport-dummy"],
    ]});

    // When the experiment pref is false, eagerly call getClientClassification.
    await SpecialPowers.pushPrefEnv({set: [
      ["app.normandy.experiments.lazy_classify", false],
    ]});
    ok(!getStub.called, "getClientClassification hasn't been called");
    await RecipeRunner.run();
    ok(getStub.called, "getClientClassification was called eagerly");

    // When the experiment pref is true, do not eagerly call getClientClassification.
    await SpecialPowers.pushPrefEnv({set: [
      ["app.normandy.experiments.lazy_classify", true],
    ]});
    getStub.reset();
    ok(!getStub.called, "getClientClassification hasn't been called");
    await RecipeRunner.run();
    ok(!getStub.called, "getClientClassification was not called eagerly");
  }
);

/**
 * Mocks RecipeRunner.loadActionSandboxManagers for testing run.
 */
async function withMockActionSandboxManagers(actions, testFunction) {
  const managers = {};
  for (const action of actions) {
    const manager = new ActionSandboxManager("");
    manager.addHold("testing");
    managers[action.name] = manager;
    sinon.stub(managers[action.name], "runAsyncCallback");
  }

  const loadActionSandboxManagers = sinon.stub(RecipeRunner, "loadActionSandboxManagers")
    .resolves(managers);
  await testFunction(managers);
  loadActionSandboxManagers.restore();

  for (const manager of Object.values(managers)) {
    manager.removeHold("testing");
    await manager.isNuked();
  }
}

decorate_task(
  withSpy(AddonStudies, "close"),
  withStub(Uptake, "reportRunner"),
  withStub(NormandyApi, "fetchRecipes"),
  withStub(ActionsManager.prototype, "fetchRemoteActions"),
  withStub(ActionsManager.prototype, "preExecution"),
  withStub(ActionsManager.prototype, "runRecipe"),
  withStub(ActionsManager.prototype, "finalize"),
  async function testRun(
    closeSpy,
    reportRunnerStub,
    fetchRecipesStub,
    fetchRemoteActionsStub,
    preExecutionStub,
    runRecipeStub,
    finalizeStub
  ) {
    const runRecipeReturn = Promise.resolve();
    const runRecipeReturnThen = sinon.spy(runRecipeReturn, "then");
    runRecipeStub.returns(runRecipeReturn);

    const matchRecipe = {id: "match", action: "matchAction", filter_expression: "true"};
    const noMatchRecipe = {id: "noMatch", action: "noMatchAction", filter_expression: "false"};
    const missingRecipe = {id: "missing", action: "missingAction", filter_expression: "true"};
    fetchRecipesStub.callsFake(async () => [matchRecipe, noMatchRecipe, missingRecipe]);

    await RecipeRunner.run();

    ok(fetchRemoteActionsStub.calledOnce, "remote actions should be fetched");
    ok(preExecutionStub.calledOnce, "pre-execution hooks should be run");
    Assert.deepEqual(
      runRecipeStub.args,
      [[matchRecipe], [missingRecipe]],
      "recipe with matching filters should be executed",
    );
    ok(runRecipeReturnThen.called, "the run method should be used asyncronously");

    // Test uptake reporting
    Assert.deepEqual(
      reportRunnerStub.args,
      [[Uptake.RUNNER_SUCCESS]],
      "RecipeRunner should report uptake telemetry",
    );

    // Ensure storage is closed after the run.
    ok(closeSpy.calledOnce, "Storage should be closed after the run");
  }
);

decorate_task(
  withMockNormandyApi,
  async function testRunFetchFail(mockApi) {
    const closeSpy = sinon.spy(AddonStudies, "close");
    const reportRunner = sinon.stub(Uptake, "reportRunner");

    const action = {name: "action"};
    mockApi.actions = [action];
    mockApi.fetchRecipes.rejects(new Error("Signature not valid"));

    await withMockActionSandboxManagers(mockApi.actions, async managers => {
      const manager = managers.action;
      await RecipeRunner.run();

      // If the recipe fetch failed, do not run anything.
      sinon.assert.notCalled(manager.runAsyncCallback);
      sinon.assert.calledWith(reportRunner, Uptake.RUNNER_SERVER_ERROR);

      // Test that network errors report a specific uptake error
      reportRunner.reset();
      mockApi.fetchRecipes.rejects(new Error("NetworkError: The system was down"));
      await RecipeRunner.run();
      sinon.assert.calledWith(reportRunner, Uptake.RUNNER_NETWORK_ERROR);

      // Test that signature issues report a specific uptake error
      reportRunner.reset();
      mockApi.fetchRecipes.rejects(new NormandyApi.InvalidSignatureError("Signature fail"));
      await RecipeRunner.run();
      sinon.assert.calledWith(reportRunner, Uptake.RUNNER_INVALID_SIGNATURE);
    });

    // If the recipe fetch failed, we don't need to call close since nothing
    // opened a connection in the first place.
    sinon.assert.notCalled(closeSpy);

    closeSpy.restore();
    reportRunner.restore();
  }
);

// test init() in dev mode
decorate_task(
  withPrefEnv({
    set: [
      ["datareporting.healthreport.uploadEnabled", true],  // telemetry enabled
      ["app.normandy.dev_mode", true],
      ["app.normandy.first_run", false],
    ],
  }),
  withStub(RecipeRunner, "run"),
  withStub(RecipeRunner, "registerTimer"),
  async function testInitDevMode(runStub, registerTimerStub, updateRunIntervalStub) {
    await RecipeRunner.init();
    ok(runStub.called, "RecipeRunner.run is called immediately when in dev mode");
    ok(registerTimerStub.called, "RecipeRunner.init registers a timer");
  }
);

// Test init() during normal operation
decorate_task(
  withPrefEnv({
    set: [
      ["datareporting.healthreport.uploadEnabled", true],  // telemetry enabled
      ["app.normandy.dev_mode", false],
      ["app.normandy.first_run", false],
    ],
  }),
  withStub(RecipeRunner, "run"),
  withStub(RecipeRunner, "registerTimer"),
  async function testInit(runStub, registerTimerStub) {
    await RecipeRunner.init();
    ok(!runStub.called, "RecipeRunner.run is called immediately when not in dev mode or first run");
    ok(registerTimerStub.called, "RecipeRunner.init registers a timer");
  }
);

// Test init() first run
decorate_task(
  withPrefEnv({
    set: [
      ["datareporting.healthreport.uploadEnabled", true],  // telemetry enabled
      ["app.normandy.dev_mode", false],
      ["app.normandy.first_run", true],
      ["app.normandy.api_url", "https://example.com"],
    ],
  }),
  withStub(RecipeRunner, "run"),
  withStub(RecipeRunner, "registerTimer"),
  withStub(RecipeRunner, "watchPrefs"),
  async function testInitFirstRun(runStub, registerTimerStub, watchPrefsStub) {
    await RecipeRunner.init();
    ok(runStub.called, "RecipeRunner.run is called immediately on first run");
    ok(
      !Services.prefs.getBoolPref("app.normandy.first_run"),
      "On first run, the first run pref is set to false"
    );
    ok(registerTimerStub.called, "RecipeRunner.registerTimer registers a timer");

    // RecipeRunner.init() sets this pref to false, but SpecialPowers
    // relies on the preferences it manages to actually change when it
    // tries to change them. Settings this back to true here allows
    // that to happen. Not doing this causes popPrefEnv to hang forever.
    Services.prefs.setBoolPref("app.normandy.first_run", true);
  }
);

// Test that prefs are watched correctly
decorate_task(
  withPrefEnv({
    set: [
      ["datareporting.healthreport.uploadEnabled", true],  // telemetry enabled
      ["app.normandy.dev_mode", false],
      ["app.normandy.first_run", false],
      ["app.normandy.enabled", true],
      ["app.normandy.api_url", "https://example.com"], // starts with "https://"
    ],
  }),
  withStub(RecipeRunner, "run"),
  withStub(RecipeRunner, "enable"),
  withStub(RecipeRunner, "disable"),
  withStub(CleanupManager, "addCleanupHandler"),
  withStub(AddonStudies, "stop"),

  async function testPrefWatching(runStub, enableStub, disableStub, addCleanupHandlerStub, stopStub) {
    await RecipeRunner.init();
    is(enableStub.callCount, 1, "Enable should be called initially");
    is(disableStub.callCount, 0, "Disable should not be called initially");

    await SpecialPowers.pushPrefEnv({ set: [["app.normandy.enabled", false]] });
    is(enableStub.callCount, 1, "Enable should not be called again");
    is(disableStub.callCount, 1, "RecipeRunner should disable when Shield is disabled");

    await SpecialPowers.pushPrefEnv({ set: [["app.normandy.enabled", true]] });
    is(enableStub.callCount, 2, "RecipeRunner should re-enable when Shield is enabled");
    is(disableStub.callCount, 1, "Disable should not be called again");

    await SpecialPowers.pushPrefEnv({ set: [["app.normandy.api_url", "http://example.com"]] }); // does not start with https://
    is(enableStub.callCount, 2, "Enable should not be called again");
    is(disableStub.callCount, 2, "RecipeRunner should disable when an invalid api url is given");

    await SpecialPowers.pushPrefEnv({ set: [["app.normandy.api_url", "https://example.com"]] }); // ends with https://
    is(enableStub.callCount, 3, "RecipeRunner should re-enable when a valid api url is given");
    is(disableStub.callCount, 2, "Disable should not be called again");

    await SpecialPowers.pushPrefEnv({ set: [["datareporting.healthreport.uploadEnabled", false]] });
    is(enableStub.callCount, 3, "Enable should not be called again");
    is(disableStub.callCount, 3, "RecipeRunner should disable when telemetry is disabled");

    await SpecialPowers.pushPrefEnv({ set: [["datareporting.healthreport.uploadEnabled", true]] });
    is(enableStub.callCount, 4, "RecipeRunner should re-enable when telemetry is enabled");
    is(disableStub.callCount, 3, "Disable should not be called again");

    is(runStub.callCount, 0, "RecipeRunner.run should not be called during this test");
  }
);

// Test that enable and disable are idempotent
decorate_task(
  withStub(RecipeRunner, "registerTimer"),
  withStub(RecipeRunner, "unregisterTimer"),

  async function testPrefWatching(registerTimerStub, unregisterTimerStub) {
    const originalEnabled = RecipeRunner.enabled;

    try {
      RecipeRunner.enabled = false;
      RecipeRunner.enable();
      RecipeRunner.enable();
      RecipeRunner.enable();
      is(registerTimerStub.callCount, 1, "Enable should be idempotent");

      RecipeRunner.enabled = true;
      RecipeRunner.disable();
      RecipeRunner.disable();
      RecipeRunner.disable();
      is(registerTimerStub.callCount, 1, "Disable should be idempotent");

    } finally {
      RecipeRunner.enabled = originalEnabled;
    }
  }
);
