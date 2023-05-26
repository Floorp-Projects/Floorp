/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { BaseAction } = ChromeUtils.importESModule(
  "resource://normandy/actions/BaseAction.sys.mjs"
);
const { ClientEnvironment } = ChromeUtils.importESModule(
  "resource://normandy/lib/ClientEnvironment.sys.mjs"
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

function assertSurvey(actual, expected) {
  for (const key of Object.keys(actual)) {
    if (["postAnswerUrl", "flowId"].includes(key)) {
      continue;
    }

    Assert.equal(
      actual[key],
      expected[key],
      `Heartbeat should receive correct ${key} parameter`
    );
  }

  Assert.ok(actual.postAnswerUrl.startsWith(expected.postAnswerUrl));
}

decorate_task(
  withStubbedHeartbeat(),
  withClearStorage(),
  async function testLegacyHeartbeatTrigger({ heartbeatClassStub }) {
    const sandbox = sinon.createSandbox();

    const cleanupEnrollment = await ExperimentFakes.enrollWithFeatureConfig({
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

      await cleanupEnrollment();
    } finally {
      sandbox.restore();
    }
  }
);
