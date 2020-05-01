"use strict";

const { ExperimentFakes } = ChromeUtils.import(
  "resource://testing-common/MSTestUtils.jsm"
);

const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const EVENT_CATEGORY = "messaging_experiments";
const EVENT_METHOD = "reach";
const EVENT_OBJECT = "cfr";
const COLLECTION_ID = "messaging-experiments";

const fakeRemoteSettingsClient = {
  get() {},
};

add_task(async function test_sendReachEvents() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("cfr_exp_01");
  const RECIPE_CFR = {
    arguments: ExperimentFakes.recipe("cfr_exp_01", {
      branches: [
        {
          slug: "control",
          value: {
            content: {
              targeting: "true",
            },
          },
        },
        {
          slug: "variant_1",
          value: {
            content: {
              targeting: "false",
            },
          },
        },
        {
          slug: "variant_2",
          value: {
            content: {
              targeting: "true",
            },
          },
        },
      ],
    }),
  };
  const RECIPE_FOO = {
    arguments: ExperimentFakes.recipe("foo", {
      branches: [
        {
          slug: "control",
          value: {
            content: {
              targeting: "true",
            },
          },
        },
        {
          slug: "variant_1",
          value: {
            content: {
              targeting: "true",
            },
          },
        },
      ],
    }),
  };
  sandbox.stub(manager.store, "getExperimentForGroup").returns(experiment);
  sandbox
    .stub(fakeRemoteSettingsClient, "get")
    .returns([RECIPE_CFR, RECIPE_FOO]);

  const extra = { branches: "control;variant_2" };
  const expectedEvents = [
    [EVENT_CATEGORY, EVENT_METHOD, EVENT_OBJECT, "cfr_exp_01", extra],
  ];

  Services.telemetry.setEventRecordingEnabled(EVENT_CATEGORY, true);
  Services.telemetry.clearEvents();

  await manager.sendReachEvents(fakeRemoteSettingsClient);

  TelemetryTestUtils.assertEvents(expectedEvents);

  Services.telemetry.setEventRecordingEnabled(EVENT_CATEGORY, false);
  sandbox.restore();
});

add_task(async function test_sendReachEvents_Failuure_RemoteSettings() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("cfr_exp_01");
  sandbox.stub(manager.store, "getExperimentForGroup").returns(experiment);
  sandbox.stub(fakeRemoteSettingsClient, "get").throws();

  Services.telemetry.setEventRecordingEnabled(EVENT_CATEGORY, true);
  Services.telemetry.clearEvents();

  await manager.sendReachEvents(fakeRemoteSettingsClient);

  TelemetryTestUtils.assertNumberOfEvents(0);

  Services.telemetry.setEventRecordingEnabled(EVENT_CATEGORY, false);
  sandbox.restore();
});

add_task(async function test_sendReachEvents_Failure_No_Active() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const RECIPE_CFR = {
    arguments: ExperimentFakes.recipe("cfr_exp_01", {
      branches: [
        {
          slug: "control",
          value: {
            content: {
              targeting: "true",
            },
          },
        },
        {
          slug: "variant_1",
          value: {
            content: {
              targeting: "true",
            },
          },
        },
      ],
    }),
  };
  sandbox.stub(manager.store, "getExperimentForGroup").returns(undefined);
  sandbox.stub(fakeRemoteSettingsClient, "get").resolves([RECIPE_CFR]);

  Services.telemetry.setEventRecordingEnabled(EVENT_CATEGORY, true);
  Services.telemetry.clearEvents();

  await manager.sendReachEvents(fakeRemoteSettingsClient);

  TelemetryTestUtils.assertNumberOfEvents(0);

  Services.telemetry.setEventRecordingEnabled(EVENT_CATEGORY, false);
  sandbox.restore();
});

add_task(async function test_sendReachEvents_Failure_No_Recipe() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("cfr_exp_01", {
    active: false,
  });
  const RECIPE_CFR = {
    arguments: ExperimentFakes.recipe("cfr_exp_02", {
      branches: [
        {
          slug: "control",
          value: {
            content: {
              targeting: "true",
            },
          },
        },
        {
          slug: "variant_1",
          value: {
            content: {
              targeting: "true",
            },
          },
        },
      ],
    }),
  };
  sandbox.stub(manager.store, "getExperimentForGroup").returns(experiment);
  sandbox.stub(fakeRemoteSettingsClient, "get").resolves([RECIPE_CFR]);

  Services.telemetry.setEventRecordingEnabled(EVENT_CATEGORY, true);
  Services.telemetry.clearEvents();

  await manager.sendReachEvents(fakeRemoteSettingsClient);

  TelemetryTestUtils.assertNumberOfEvents(0);

  Services.telemetry.setEventRecordingEnabled(EVENT_CATEGORY, false);
  sandbox.restore();
});

add_task(async function test_sendReachEvents_Failure_No_Qualified() {
  const sandbox = sinon.createSandbox();
  const manager = ExperimentFakes.manager();
  const experiment = ExperimentFakes.experiment("cfr_exp_01");
  const RECIPE_CFR = {
    arguments: ExperimentFakes.recipe("cfr_exp_01", {
      branches: [
        {
          slug: "control",
          value: {
            content: {
              targeting: "false",
            },
          },
        },
        {
          slug: "variant_1",
          value: {
            content: {
              targeting: "false",
            },
          },
        },
      ],
    }),
  };
  sandbox.stub(manager.store, "getExperimentForGroup").returns(experiment);
  sandbox.stub(fakeRemoteSettingsClient, "get").resolves([RECIPE_CFR]);

  Services.telemetry.setEventRecordingEnabled(EVENT_CATEGORY, true);
  Services.telemetry.clearEvents();

  await manager.sendReachEvents(fakeRemoteSettingsClient);

  TelemetryTestUtils.assertNumberOfEvents(0);

  Services.telemetry.setEventRecordingEnabled(EVENT_CATEGORY, false);
  sandbox.restore();
});
