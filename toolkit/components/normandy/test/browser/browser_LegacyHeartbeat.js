/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BaseAction } = ChromeUtils.importESModule(
  "resource://normandy/actions/BaseAction.sys.mjs"
);
const { ClientEnvironment } = ChromeUtils.importESModule(
  "resource://normandy/lib/ClientEnvironment.sys.mjs"
);
const { EventEmitter } = ChromeUtils.importESModule(
  "resource://normandy/lib/EventEmitter.sys.mjs"
);
const { Heartbeat } = ChromeUtils.importESModule(
  "resource://normandy/lib/Heartbeat.sys.mjs"
);
const { Normandy } = ChromeUtils.importESModule(
  "resource://normandy/Normandy.sys.mjs"
);
const { ExperimentAPI } = ChromeUtils.importESModule(
  "resource://nimbus/ExperimentAPI.sys.mjs"
);
const { ExperimentFakes } = ChromeUtils.importESModule(
  "resource://testing-common/NimbusTestUtils.sys.mjs"
);
const { RecipeRunner } = ChromeUtils.importESModule(
  "resource://normandy/lib/RecipeRunner.sys.mjs"
);
const { RemoteSettings } = ChromeUtils.importESModule(
  "resource://services-settings/remote-settings.sys.mjs"
);
const { JsonSchema } = ChromeUtils.importESModule(
  "resource://gre/modules/JsonSchema.sys.mjs"
);

const SURVEY = {
  surveyId: "a survey",
  message: "test message",
  engagementButtonLabel: "",
  thanksMessage: "thanks!",
  postAnswerUrl: "https://example.com",
  learnMoreMessage: "Learn More",
  learnMoreUrl: "https://example.com",
  repeatOption: "once",
};

// See properties.payload in
// https://github.com/mozilla-services/mozilla-pipeline-schemas/blob/main/schemas/telemetry/heartbeat/heartbeat.4.schema.json

const PAYLOAD_SCHEMA = {
  additionalProperties: false,
  anyOf: [
    {
      required: ["closedTS"],
    },
    {
      required: ["windowClosedTS"],
    },
  ],
  properties: {
    closedTS: {
      minimum: 0,
      type: "integer",
    },
    engagedTS: {
      minimum: 0,
      type: "integer",
    },
    expiredTS: {
      minimum: 0,
      type: "integer",
    },
    flowId: {
      pattern:
        "^[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}$",
      type: "string",
    },
    learnMoreTS: {
      minimum: 0,
      type: "integer",
    },
    offeredTS: {
      minimum: 0,
      type: "integer",
    },
    score: {
      minimum: 1,
      type: "integer",
    },
    surveyId: {
      type: "string",
    },
    surveyVersion: {
      pattern: "^([0-9]+|[a-fA-F0-9]{64})$",
      type: "string",
    },
    testing: {
      type: "boolean",
    },
    version: {
      maximum: 1,
      minimum: 1,
      type: "number",
    },
    votedTS: {
      minimum: 0,
      type: "integer",
    },
    windowClosedTS: {
      minimum: 0,
      type: "integer",
    },
  },
  required: ["version", "flowId", "offeredTS", "surveyId", "surveyVersion"],
  type: "object",
};

function assertSurvey(actual, expected) {
  for (const key of Object.keys(actual)) {
    if (["flowId", "postAnswerUrl", "surveyVersion"].includes(key)) {
      continue;
    }

    Assert.equal(
      actual[key],
      expected[key],
      `Heartbeat should receive correct ${key} parameter`
    );
  }

  Assert.equal(actual.surveyVersion, "1");
  Assert.ok(actual.postAnswerUrl.startsWith(expected.postAnswerUrl));
}

decorate_task(
  withStubbedHeartbeat(),
  withClearStorage(),
  async function testLegacyHeartbeatTrigger({ heartbeatClassStub }) {
    const sandbox = sinon.createSandbox();

    const doEnrollmentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
      featureId: "legacyHeartbeat",
      value: {
        survey: SURVEY,
      },
    });

    const client = RemoteSettings("normandy-recipes-capabilities");
    sandbox.stub(client, "get").resolves([]);

    try {
      await RecipeRunner.run();
      Assert.equal(
        heartbeatClassStub.args.length,
        1,
        "Heartbeat should be instantiated once"
      );
      assertSurvey(heartbeatClassStub.args[0][1], SURVEY);

      doEnrollmentCleanup();
    } finally {
      sandbox.restore();
    }
  }
);

decorate_task(
  withClearStorage(),
  async function testLegacyHeartbeatPingPayload() {
    const sandbox = sinon.createSandbox();

    const doEnrollmentCleanup = await ExperimentFakes.enrollWithFeatureConfig({
      featureId: "legacyHeartbeat",
      value: {
        survey: SURVEY,
      },
    });

    const client = RemoteSettings("normandy-recipes-capabilities");
    sandbox.stub(client, "get").resolves([]);

    // Override Heartbeat so we can get the instance and manipulate it directly.
    const heartbeatDeferred = Promise.withResolvers();
    class TestHeartbeat extends Heartbeat {
      constructor(...args) {
        super(...args);
        heartbeatDeferred.resolve(this);
      }
    }
    ShowHeartbeatAction.overrideHeartbeatForTests(TestHeartbeat);

    try {
      await RecipeRunner.run();
      const heartbeat = await heartbeatDeferred.promise;
      // We are going to simulate the timer timing out, so we do not want it to
      // *actually* time out.
      heartbeat.endTimerIfPresent("surveyEndTimer");
      const notice = await heartbeat.noticePromise;
      await notice.updateComplete;

      const telemetrySentPromise = new Promise(resolve => {
        heartbeat.eventEmitter.once("TelemetrySent", payload =>
          resolve(payload)
        );
      });

      // This method would be triggered when the timer timed out. This will
      // trigger telemetry to be submitted.
      heartbeat.close();

      const payload = await telemetrySentPromise;

      const result = JsonSchema.validate(payload, PAYLOAD_SCHEMA);
      Assert.ok(result.valid);
      Assert.equal(payload.surveyVersion, "1");

      doEnrollmentCleanup();
    } finally {
      ShowHeartbeatAction.overrideHeartbeatForTests();
      sandbox.restore();
    }
  }
);
