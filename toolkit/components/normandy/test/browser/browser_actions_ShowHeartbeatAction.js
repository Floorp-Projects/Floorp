"use strict";

ChromeUtils.import("resource://normandy/actions/BaseAction.jsm", this);
ChromeUtils.import("resource://normandy/actions/ShowHeartbeatAction.jsm", this);
ChromeUtils.import("resource://normandy/lib/ClientEnvironment.jsm", this);
ChromeUtils.import("resource://normandy/lib/Heartbeat.jsm", this);
ChromeUtils.import("resource://normandy/lib/Storage.jsm", this);
ChromeUtils.import("resource://normandy/lib/Uptake.jsm", this);
ChromeUtils.import("resource://testing-common/NormandyTestUtils.jsm", this);

const HOUR_IN_MS = 60 * 60 * 1000;

function heartbeatRecipeFactory(overrides = {}) {
  const defaults = {
    revision_id: 1,
    name: "Test Recipe",
    action: "show-heartbeat",
    arguments: {
      surveyId: "a survey",
      message: "test message",
      engagementButtonLabel: "",
      thanksMessage: "thanks!",
      postAnswerUrl: "http://example.com",
      learnMoreMessage: "Learn More",
      learnMoreUrl: "http://example.com",
      repeatOption: "once",
    },
  };

  if (overrides.arguments) {
    defaults.arguments = Object.assign(defaults.arguments, overrides.arguments);
    delete overrides.arguments;
  }

  return recipeFactory(Object.assign(defaults, overrides));
}

class MockHeartbeat {
  constructor() {
    this.eventEmitter = new MockEventEmitter();
  }
}

class MockEventEmitter {
  constructor() {
    this.once = sinon.stub();
  }
}

function withStubbedHeartbeat(testFunction) {
  return async function wrappedTestFunction(...args) {
    const backstage = ChromeUtils.import(
      "resource://normandy/actions/ShowHeartbeatAction.jsm",
      null
    );
    const originalHeartbeat = backstage.Heartbeat;
    const heartbeatInstanceStub = new MockHeartbeat();
    const heartbeatClassStub = sinon.stub();
    heartbeatClassStub.returns(heartbeatInstanceStub);
    backstage.Heartbeat = heartbeatClassStub;

    try {
      await testFunction(
        { heartbeatClassStub, heartbeatInstanceStub },
        ...args
      );
    } finally {
      backstage.Heartbeat = originalHeartbeat;
    }
  };
}

function withClearStorage(testFunction) {
  return async function wrappedTestFunction(...args) {
    Storage.clearAllStorage();
    try {
      await testFunction(...args);
    } finally {
      Storage.clearAllStorage();
    }
  };
}

// Test that a normal heartbeat works as expected
decorate_task(
  withStubbedHeartbeat,
  withClearStorage,
  async function testHappyPath({ heartbeatClassStub, heartbeatInstanceStub }) {
    const recipe = heartbeatRecipeFactory();
    const action = new ShowHeartbeatAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    await action.finalize();
    is(
      action.state,
      ShowHeartbeatAction.STATE_FINALIZED,
      "Action should be finalized"
    );
    is(action.lastError, null, "No errors should have been thrown");

    const options = heartbeatClassStub.args[0][1];
    Assert.deepEqual(
      heartbeatClassStub.args,
      [
        [
          heartbeatClassStub.args[0][0], // target window
          {
            surveyId: options.surveyId,
            message: recipe.arguments.message,
            engagementButtonLabel: recipe.arguments.engagementButtonLabel,
            thanksMessage: recipe.arguments.thanksMessage,
            learnMoreMessage: recipe.arguments.learnMoreMessage,
            learnMoreUrl: recipe.arguments.learnMoreUrl,
            postAnswerUrl: options.postAnswerUrl,
            flowId: options.flowId,
            surveyVersion: recipe.revision_id,
          },
        ],
      ],
      "expected arguments were passed"
    );

    ok(NormandyTestUtils.isUuid(options.flowId, "flowId should be a uuid"));

    // postAnswerUrl gains several query string parameters. Check that the prefix is right
    ok(options.postAnswerUrl.startsWith(recipe.arguments.postAnswerUrl));

    ok(
      heartbeatInstanceStub.eventEmitter.once.calledWith("Voted"),
      "Voted event handler should be registered"
    );
    ok(
      heartbeatInstanceStub.eventEmitter.once.calledWith("Engaged"),
      "Engaged event handler should be registered"
    );
  }
);

/* Test that heartbeat doesn't show if an unrelated heartbeat has shown recently. */
decorate_task(
  withStubbedHeartbeat,
  withClearStorage,
  async function testRepeatGeneral({ heartbeatClassStub }) {
    const allHeartbeatStorage = new Storage("normandy-heartbeat");
    await allHeartbeatStorage.setItem("lastShown", Date.now());
    const recipe = heartbeatRecipeFactory();

    const action = new ShowHeartbeatAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "No errors should have been thrown");

    is(
      heartbeatClassStub.args.length,
      0,
      "Heartbeat should not be called once"
    );
  }
);

/* Test that a heartbeat shows if an unrelated heartbeat showed more than 24 hours ago. */
decorate_task(
  withStubbedHeartbeat,
  withClearStorage,
  async function testRepeatUnrelated({ heartbeatClassStub }) {
    const allHeartbeatStorage = new Storage("normandy-heartbeat");
    await allHeartbeatStorage.setItem(
      "lastShown",
      Date.now() - 25 * HOUR_IN_MS
    );
    const recipe = heartbeatRecipeFactory();

    const action = new ShowHeartbeatAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "No errors should have been thrown");

    is(heartbeatClassStub.args.length, 1, "Heartbeat should be called once");
  }
);

/* Test that a repeat=once recipe is not shown again, even more than 24 hours ago. */
decorate_task(
  withStubbedHeartbeat,
  withClearStorage,
  async function testRepeatTypeOnce({ heartbeatClassStub }) {
    const recipe = heartbeatRecipeFactory({
      arguments: { repeatOption: "once" },
    });
    const recipeStorage = new Storage(recipe.id);
    await recipeStorage.setItem("lastShown", Date.now() - 25 * HOUR_IN_MS);

    const action = new ShowHeartbeatAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "No errors should have been thrown");

    is(heartbeatClassStub.args.length, 0, "Heartbeat should not be called");
  }
);

/* Test that a repeat=xdays recipe is shown again, only after the expected number of days. */
decorate_task(
  withStubbedHeartbeat,
  withClearStorage,
  async function testRepeatTypeXdays({ heartbeatClassStub }) {
    const recipe = heartbeatRecipeFactory({
      arguments: {
        repeatOption: "xdays",
        repeatEvery: 2,
      },
    });
    const recipeStorage = new Storage(recipe.id);
    const allHeartbeatStorage = new Storage("normandy-heartbeat");

    await recipeStorage.setItem("lastShown", Date.now() - 25 * HOUR_IN_MS);
    await allHeartbeatStorage.setItem(
      "lastShown",
      Date.now() - 25 * HOUR_IN_MS
    );
    const action = new ShowHeartbeatAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "No errors should have been thrown");
    is(heartbeatClassStub.args.length, 0, "Heartbeat should not be called");

    await recipeStorage.setItem("lastShown", Date.now() - 50 * HOUR_IN_MS);
    await allHeartbeatStorage.setItem(
      "lastShown",
      Date.now() - 50 * HOUR_IN_MS
    );
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "No errors should have been thrown");
    is(
      heartbeatClassStub.args.length,
      1,
      "Heartbeat should have been called once"
    );
  }
);

/* Test that a repeat=nag recipe is shown again until lastInteraction is set */
decorate_task(
  withStubbedHeartbeat,
  withClearStorage,
  async function testRepeatTypeNag({ heartbeatClassStub }) {
    const recipe = heartbeatRecipeFactory({
      arguments: { repeatOption: "nag" },
    });
    const recipeStorage = new Storage(recipe.id);
    const allHeartbeatStorage = new Storage("normandy-heartbeat");

    await allHeartbeatStorage.setItem(
      "lastShown",
      Date.now() - 25 * HOUR_IN_MS
    );
    await recipeStorage.setItem("lastShown", Date.now() - 25 * HOUR_IN_MS);
    const action = new ShowHeartbeatAction();
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "No errors should have been thrown");
    is(heartbeatClassStub.args.length, 1, "Heartbeat should be called");

    await allHeartbeatStorage.setItem(
      "lastShown",
      Date.now() - 50 * HOUR_IN_MS
    );
    await recipeStorage.setItem("lastShown", Date.now() - 50 * HOUR_IN_MS);
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "No errors should have been thrown");
    is(heartbeatClassStub.args.length, 2, "Heartbeat should be called again");

    await allHeartbeatStorage.setItem(
      "lastShown",
      Date.now() - 75 * HOUR_IN_MS
    );
    await recipeStorage.setItem("lastShown", Date.now() - 75 * HOUR_IN_MS);
    await recipeStorage.setItem(
      "lastInteraction",
      Date.now() - 50 * HOUR_IN_MS
    );
    await action.processRecipe(recipe, BaseAction.suitability.FILTER_MATCH);
    is(action.lastError, null, "No errors should have been thrown");
    is(
      heartbeatClassStub.args.length,
      2,
      "Heartbeat should not be called again"
    );
  }
);

/* generatePostAnswerURL shouldn't annotate empty strings */
add_task(async function postAnswerEmptyString() {
  const recipe = heartbeatRecipeFactory({ arguments: { postAnswerUrl: "" } });
  const action = new ShowHeartbeatAction();
  is(
    await action.generatePostAnswerURL(recipe),
    "",
    "an empty string should not be annotated"
  );
});

/* generatePostAnswerURL should include the right details */
add_task(async function postAnswerUrl() {
  const recipe = heartbeatRecipeFactory({
    arguments: {
      postAnswerUrl: "https://example.com/survey?survey_id=42",
      includeTelemetryUUID: false,
      message: "Hello, World!",
    },
  });
  const action = new ShowHeartbeatAction();
  const url = new URL(await action.generatePostAnswerURL(recipe));

  is(
    url.searchParams.get("survey_id"),
    "42",
    "Pre-existing search parameters should be preserved"
  );
  is(
    url.searchParams.get("fxVersion"),
    Services.appinfo.version,
    "Firefox version should be included"
  );
  is(
    url.searchParams.get("surveyversion"),
    Services.appinfo.version,
    "Survey version should also be the Firefox version"
  );
  ok(
    ["0", "1"].includes(url.searchParams.get("syncSetup")),
    `syncSetup should be 0 or 1, got ${url.searchParams.get("syncSetup")}`
  );
  is(
    url.searchParams.get("updateChannel"),
    UpdateUtils.getUpdateChannel("false"),
    "Update channel should be included"
  );
  ok(!url.searchParams.has("userId"), "no user id should be included");
  is(
    url.searchParams.get("utm_campaign"),
    "Hello%2CWorld!",
    "utm_campaign should be an encoded version of the message"
  );
  is(
    url.searchParams.get("utm_medium"),
    "show-heartbeat",
    "utm_medium should be the action name"
  );
  is(
    url.searchParams.get("utm_source"),
    "firefox",
    "utm_source should be firefox"
  );
});

/* generatePostAnswerURL shouldn't override existing values in the url */
add_task(async function postAnswerUrlNoOverwite() {
  const recipe = heartbeatRecipeFactory({
    arguments: {
      postAnswerUrl:
        "https://example.com/survey?utm_source=shady_tims_firey_fox",
    },
  });
  const action = new ShowHeartbeatAction();
  const url = new URL(await action.generatePostAnswerURL(recipe));
  is(
    url.searchParams.get("utm_source"),
    "shady_tims_firey_fox",
    "utm_source should not be overwritten"
  );
});

/* generatePostAnswerURL should only include userId if requested */
add_task(async function postAnswerUrlUserIdIfRequested() {
  const recipeWithId = heartbeatRecipeFactory({
    arguments: { includeTelemetryUUID: true },
  });
  const recipeWithoutId = heartbeatRecipeFactory({
    arguments: { includeTelemetryUUID: false },
  });
  const action = new ShowHeartbeatAction();

  const urlWithId = new URL(await action.generatePostAnswerURL(recipeWithId));
  is(
    urlWithId.searchParams.get("userId"),
    ClientEnvironment.userId,
    "clientId should be included"
  );

  const urlWithoutId = new URL(
    await action.generatePostAnswerURL(recipeWithoutId)
  );
  ok(!urlWithoutId.searchParams.has("userId"), "userId should not be included");
});

/* generateSurveyId should include userId only if requested */
decorate_task(
  withStubbedHeartbeat,
  withClearStorage,
  async function testGenerateSurveyId({ heartbeatClassStub }) {
    const recipeWithoutId = heartbeatRecipeFactory({
      arguments: { surveyId: "test-id", includeTelemetryUUID: false },
    });
    const recipeWithId = heartbeatRecipeFactory({
      arguments: { surveyId: "test-id", includeTelemetryUUID: true },
    });
    const action = new ShowHeartbeatAction();
    is(
      action.generateSurveyId(recipeWithoutId),
      "test-id",
      "userId should not be included if not requested"
    );
    is(
      action.generateSurveyId(recipeWithId),
      `test-id::${ClientEnvironment.userId}`,
      "userId should be included if requested"
    );
  }
);
