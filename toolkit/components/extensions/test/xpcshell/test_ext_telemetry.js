"use strict";

const { TelemetryArchive } = ChromeUtils.import(
  "resource://gre/modules/TelemetryArchive.jsm"
);
const { TelemetryUtils } = ChromeUtils.import(
  "resource://gre/modules/TelemetryUtils.jsm"
);
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const { TelemetryStorage } = ChromeUtils.import(
  "resource://gre/modules/TelemetryStorage.jsm"
);

// All tests run privileged unless otherwise specified not to.
function createExtension(
  backgroundScript,
  permissions,
  isPrivileged = true,
  telemetry
) {
  let extensionData = {
    background: backgroundScript,
    manifest: { permissions, telemetry },
    isPrivileged,
  };

  return ExtensionTestUtils.loadExtension(extensionData);
}

async function run(test) {
  let extension = createExtension(
    test.backgroundScript,
    test.permissions || ["telemetry"],
    test.isPrivileged,
    test.telemetry
  );
  await extension.startup();
  await extension.awaitFinish(test.doneSignal);
  await extension.unload();
}

// Currently unsupported on Android: blocked on 1220177.
// See 1280234 c67 for discussion.
if (AppConstants.MOZ_BUILD_APP === "browser") {
  add_task(async function test_telemetry_without_telemetry_permission() {
    await run({
      backgroundScript: () => {
        browser.test.assertTrue(
          !browser.telemetry,
          "'telemetry' permission is required"
        );
        browser.test.notifyPass("telemetry_permission");
      },
      permissions: [],
      doneSignal: "telemetry_permission",
      isPrivileged: false,
    });
  });

  add_task(
    async function test_telemetry_without_telemetry_permission_privileged() {
      await run({
        backgroundScript: () => {
          browser.test.assertTrue(
            !browser.telemetry,
            "'telemetry' permission is required"
          );
          browser.test.notifyPass("telemetry_permission");
        },
        permissions: [],
        doneSignal: "telemetry_permission",
      });
    }
  );

  add_task(async function test_telemetry_scalar_add() {
    Services.telemetry.clearScalars();
    await run({
      backgroundScript: async () => {
        await browser.telemetry.scalarAdd(
          "telemetry.test.unsigned_int_kind",
          1
        );
        browser.test.notifyPass("scalar_add");
      },
      doneSignal: "scalar_add",
    });
    TelemetryTestUtils.assertScalar(
      TelemetryTestUtils.getProcessScalars("parent", false, true),
      "telemetry.test.unsigned_int_kind",
      1
    );
  });

  add_task(async function test_telemetry_scalar_add_unknown_name() {
    let { messages } = await promiseConsoleOutput(async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarAdd("telemetry.test.does_not_exist", 1);
          browser.test.notifyPass("scalar_add_unknown_name");
        },
        doneSignal: "scalar_add_unknown_name",
      });
    });
    Assert.ok(
      messages.find(({ message }) => message.includes("Unknown scalar")),
      "Telemetry should warn if an unknown scalar is incremented"
    );
  });

  add_task(async function test_telemetry_scalar_add_illegal_value() {
    await run({
      backgroundScript: () => {
        browser.test.assertThrows(
          () =>
            browser.telemetry.scalarAdd("telemetry.test.unsigned_int_kind", {}),
          /Incorrect argument types for telemetry.scalarAdd/,
          "The second 'value' argument to scalarAdd must be an integer, string, or boolean"
        );
        browser.test.notifyPass("scalar_add_illegal_value");
      },
      doneSignal: "scalar_add_illegal_value",
    });
  });

  add_task(async function test_telemetry_scalar_add_invalid_keyed_scalar() {
    let { messages } = await promiseConsoleOutput(async function() {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarAdd(
            "telemetry.test.keyed_unsigned_int",
            1
          );
          browser.test.notifyPass("scalar_add_invalid_keyed_scalar");
        },
        doneSignal: "scalar_add_invalid_keyed_scalar",
      });
    });
    Assert.ok(
      messages.find(({ message }) =>
        message.includes("Attempting to manage a keyed scalar as a scalar")
      ),
      "Telemetry should warn if a scalarAdd is called for a keyed scalar"
    );
  });

  add_task(async function test_telemetry_scalar_set() {
    Services.telemetry.clearScalars();
    await run({
      backgroundScript: async () => {
        await browser.telemetry.scalarSet("telemetry.test.boolean_kind", true);
        browser.test.notifyPass("scalar_set");
      },
      doneSignal: "scalar_set",
    });
    TelemetryTestUtils.assertScalar(
      TelemetryTestUtils.getProcessScalars("parent", false, true),
      "telemetry.test.boolean_kind",
      true
    );
  });

  add_task(async function test_telemetry_scalar_set_unknown_name() {
    let { messages } = await promiseConsoleOutput(async function() {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarSet(
            "telemetry.test.does_not_exist",
            true
          );
          browser.test.notifyPass("scalar_set_unknown_name");
        },
        doneSignal: "scalar_set_unknown_name",
      });
    });
    Assert.ok(
      messages.find(({ message }) => message.includes("Unknown scalar")),
      "Telemetry should warn if an unknown scalar is set"
    );
  });

  add_task(async function test_telemetry_scalar_set_maximum() {
    Services.telemetry.clearScalars();
    await run({
      backgroundScript: async () => {
        await browser.telemetry.scalarSetMaximum(
          "telemetry.test.unsigned_int_kind",
          123
        );
        browser.test.notifyPass("scalar_set_maximum");
      },
      doneSignal: "scalar_set_maximum",
    });
    TelemetryTestUtils.assertScalar(
      TelemetryTestUtils.getProcessScalars("parent", false, true),
      "telemetry.test.unsigned_int_kind",
      123
    );
  });

  add_task(async function test_telemetry_scalar_set_maximum_unknown_name() {
    let { messages } = await promiseConsoleOutput(async function() {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.scalarSetMaximum(
            "telemetry.test.does_not_exist",
            1
          );
          browser.test.notifyPass("scalar_set_maximum_unknown_name");
        },
        doneSignal: "scalar_set_maximum_unknown_name",
      });
    });
    Assert.ok(
      messages.find(({ message }) => message.includes("Unknown scalar")),
      "Telemetry should warn if an unknown scalar is set"
    );
  });

  add_task(async function test_telemetry_scalar_set_maximum_illegal_value() {
    await run({
      backgroundScript: () => {
        browser.test.assertThrows(
          () =>
            browser.telemetry.scalarSetMaximum(
              "telemetry.test.unsigned_int_kind",
              "string"
            ),
          /Incorrect argument types for telemetry.scalarSetMaximum/,
          "The second 'value' argument to scalarSetMaximum must be a scalar"
        );
        browser.test.notifyPass("scalar_set_maximum_illegal_value");
      },
      doneSignal: "scalar_set_maximum_illegal_value",
    });
  });

  add_task(async function test_telemetry_keyed_scalar_add() {
    Services.telemetry.clearScalars();
    await run({
      backgroundScript: async () => {
        await browser.telemetry.keyedScalarAdd(
          "telemetry.test.keyed_unsigned_int",
          "foo",
          1
        );
        browser.test.notifyPass("keyed_scalar_add");
      },
      doneSignal: "keyed_scalar_add",
    });
    TelemetryTestUtils.assertKeyedScalar(
      TelemetryTestUtils.getProcessScalars("parent", true, true),
      "telemetry.test.keyed_unsigned_int",
      "foo",
      1
    );
  });

  add_task(async function test_telemetry_keyed_scalar_add_unknown_name() {
    let { messages } = await promiseConsoleOutput(async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.keyedScalarAdd(
            "telemetry.test.does_not_exist",
            "foo",
            1
          );
          browser.test.notifyPass("keyed_scalar_add_unknown_name");
        },
        doneSignal: "keyed_scalar_add_unknown_name",
      });
    });
    Assert.ok(
      messages.find(({ message }) => message.includes("Unknown scalar")),
      "Telemetry should warn if an unknown keyed scalar is incremented"
    );
  });

  add_task(async function test_telemetry_keyed_scalar_add_illegal_value() {
    await run({
      backgroundScript: () => {
        browser.test.assertThrows(
          () =>
            browser.telemetry.keyedScalarAdd(
              "telemetry.test.keyed_unsigned_int",
              "foo",
              {}
            ),
          /Incorrect argument types for telemetry.keyedScalarAdd/,
          "The second 'value' argument to keyedScalarAdd must be an integer, string, or boolean"
        );
        browser.test.notifyPass("keyed_scalar_add_illegal_value");
      },
      doneSignal: "keyed_scalar_add_illegal_value",
    });
  });

  add_task(async function test_telemetry_keyed_scalar_add_invalid_scalar() {
    let { messages } = await promiseConsoleOutput(async function() {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.keyedScalarAdd(
            "telemetry.test.unsigned_int_kind",
            "foo",
            1
          );
          browser.test.notifyPass("keyed_scalar_add_invalid_scalar");
        },
        doneSignal: "keyed_scalar_add_invalid_scalar",
      });
    });
    Assert.ok(
      messages.find(({ message }) =>
        message.includes(
          "Attempting to manage a keyed scalar as a scalar (or vice-versa)"
        )
      ),
      "Telemetry should warn if a scalar is incremented as a keyed scalar"
    );
  });

  add_task(async function test_telemetry_keyed_scalar_add_long_key() {
    let { messages } = await promiseConsoleOutput(async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.keyedScalarAdd(
            "telemetry.test.keyed_unsigned_int",
            "X".repeat(73),
            1
          );
          browser.test.notifyPass("keyed_scalar_add_long_key");
        },
        doneSignal: "keyed_scalar_add_long_key",
      });
    });
    Assert.ok(
      messages.find(({ message }) =>
        message.includes("The key length must be limited to 72 characters.")
      ),
      "Telemetry should warn if keyed scalar's key is too long"
    );
  });

  add_task(async function test_telemetry_keyed_scalar_set() {
    Services.telemetry.clearScalars();
    await run({
      backgroundScript: async () => {
        await browser.telemetry.keyedScalarSet(
          "telemetry.test.keyed_boolean_kind",
          "foo",
          true
        );
        browser.test.notifyPass("keyed_scalar_set");
      },
      doneSignal: "keyed_scalar_set",
    });
    TelemetryTestUtils.assertKeyedScalar(
      TelemetryTestUtils.getProcessScalars("parent", true, true),
      "telemetry.test.keyed_boolean_kind",
      "foo",
      true
    );
  });

  add_task(async function test_telemetry_keyed_scalar_set_unknown_name() {
    let { messages } = await promiseConsoleOutput(async function() {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.keyedScalarSet(
            "telemetry.test.does_not_exist",
            "foo",
            true
          );
          browser.test.notifyPass("keyed_scalar_set_unknown_name");
        },
        doneSignal: "keyed_scalar_set_unknown_name",
      });
    });
    Assert.ok(
      messages.find(({ message }) => message.includes("Unknown scalar")),
      "Telemetry should warn if an unknown keyed scalar is incremented"
    );
  });

  add_task(async function test_telemetry_keyed_scalar_set_long_key() {
    let { messages } = await promiseConsoleOutput(async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.keyedScalarSet(
            "telemetry.test.keyed_unsigned_int",
            "X".repeat(73),
            1
          );
          browser.test.notifyPass("keyed_scalar_set_long_key");
        },
        doneSignal: "keyed_scalar_set_long_key",
      });
    });
    Assert.ok(
      messages.find(({ message }) =>
        message.includes("The key length must be limited to 72 characters")
      ),
      "Telemetry should warn if keyed scalar's key is too long"
    );
  });

  add_task(async function test_telemetry_keyed_scalar_set_maximum() {
    Services.telemetry.clearScalars();
    await run({
      backgroundScript: async () => {
        await browser.telemetry.keyedScalarSetMaximum(
          "telemetry.test.keyed_unsigned_int",
          "foo",
          123
        );
        browser.test.notifyPass("keyed_scalar_set_maximum");
      },
      doneSignal: "keyed_scalar_set_maximum",
    });
    TelemetryTestUtils.assertKeyedScalar(
      TelemetryTestUtils.getProcessScalars("parent", true, true),
      "telemetry.test.keyed_unsigned_int",
      "foo",
      123
    );
  });

  add_task(
    async function test_telemetry_keyed_scalar_set_maximum_unknown_name() {
      let { messages } = await promiseConsoleOutput(async function() {
        await run({
          backgroundScript: async () => {
            await browser.telemetry.keyedScalarSetMaximum(
              "telemetry.test.does_not_exist",
              "foo",
              1
            );
            browser.test.notifyPass("keyed_scalar_set_maximum_unknown_name");
          },
          doneSignal: "keyed_scalar_set_maximum_unknown_name",
        });
      });
      Assert.ok(
        messages.find(({ message }) => message.includes("Unknown scalar")),
        "Telemetry should warn if an unknown keyed scalar is set"
      );
    }
  );

  add_task(
    async function test_telemetry_keyed_scalar_set_maximum_illegal_value() {
      await run({
        backgroundScript: () => {
          browser.test.assertThrows(
            () =>
              browser.telemetry.keyedScalarSetMaximum(
                "telemetry.test.keyed_unsigned_int",
                "foo",
                "string"
              ),
            /Incorrect argument types for telemetry.keyedScalarSetMaximum/,
            "The third 'value' argument to keyedScalarSetMaximum must be a scalar"
          );
          browser.test.notifyPass("keyed_scalar_set_maximum_illegal_value");
        },
        doneSignal: "keyed_scalar_set_maximum_illegal_value",
      });
    }
  );

  add_task(async function test_telemetry_keyed_scalar_set_maximum_long_key() {
    let { messages } = await promiseConsoleOutput(async () => {
      await run({
        backgroundScript: async () => {
          await browser.telemetry.keyedScalarSetMaximum(
            "telemetry.test.keyed_unsigned_int",
            "X".repeat(73),
            1
          );
          browser.test.notifyPass("keyed_scalar_set_maximum_long_key");
        },
        doneSignal: "keyed_scalar_set_maximum_long_key",
      });
    });
    Assert.ok(
      messages.find(({ message }) =>
        message.includes("The key length must be limited to 72 characters")
      ),
      "Telemetry should warn if keyed scalar's key is too long"
    );
  });

  add_task(async function test_telemetry_record_event() {
    Services.telemetry.clearEvents();
    Services.telemetry.setEventRecordingEnabled("telemetry.test", true);

    await run({
      backgroundScript: async () => {
        await browser.telemetry.recordEvent(
          "telemetry.test",
          "test1",
          "object1"
        );
        browser.test.notifyPass("record_event_ok");
      },
      doneSignal: "record_event_ok",
    });

    TelemetryTestUtils.assertEvents(
      [
        {
          category: "telemetry.test",
          method: "test1",
          object: "object1",
        },
      ],
      { category: "telemetry.test" }
    );

    Services.telemetry.setEventRecordingEnabled("telemetry.test", false);
    Services.telemetry.clearEvents();
  });

  // Bug 1536877
  add_task(async function test_telemetry_record_event_value_must_be_string() {
    Services.telemetry.clearEvents();
    Services.telemetry.setEventRecordingEnabled("telemetry.test", true);

    await run({
      backgroundScript: async () => {
        try {
          await browser.telemetry.recordEvent(
            "telemetry.test",
            "test1",
            "object1",
            "value1"
          );
          browser.test.notifyPass("record_event_string_value");
        } catch (ex) {
          browser.test.fail(
            `Unexpected exception raised during record_event_value_must_be_string: ${ex}`
          );
          browser.test.notifyPass("record_event_string_value");
          throw ex;
        }
      },
      doneSignal: "record_event_string_value",
    });

    TelemetryTestUtils.assertEvents(
      [
        {
          category: "telemetry.test",
          method: "test1",
          object: "object1",
          value: "value1",
        },
      ],
      { category: "telemetry.test" }
    );

    Services.telemetry.setEventRecordingEnabled("telemetry.test", false);
    Services.telemetry.clearEvents();
  });

  add_task(async function test_telemetry_register_scalars_string() {
    Services.telemetry.clearScalars();
    await run({
      backgroundScript: async () => {
        await browser.telemetry.registerScalars("telemetry.test.dynamic", {
          webext_string: {
            kind: browser.telemetry.ScalarType.STRING,
            keyed: false,
            record_on_release: true,
          },
        });
        await browser.telemetry.scalarSet(
          "telemetry.test.dynamic.webext_string",
          "hello"
        );
        browser.test.notifyPass("register_scalars_string");
      },
      doneSignal: "register_scalars_string",
    });
    TelemetryTestUtils.assertScalar(
      TelemetryTestUtils.getProcessScalars("parent", false, true),
      "telemetry.test.dynamic.webext_string",
      "hello"
    );
  });

  add_task(async function test_telemetry_register_scalars_multiple() {
    Services.telemetry.clearScalars();
    await run({
      backgroundScript: async () => {
        await browser.telemetry.registerScalars("telemetry.test.dynamic", {
          webext_string: {
            kind: browser.telemetry.ScalarType.STRING,
            keyed: false,
            record_on_release: true,
          },
          webext_string_too: {
            kind: browser.telemetry.ScalarType.STRING,
            keyed: false,
            record_on_release: true,
          },
        });
        await browser.telemetry.scalarSet(
          "telemetry.test.dynamic.webext_string",
          "hello"
        );
        await browser.telemetry.scalarSet(
          "telemetry.test.dynamic.webext_string_too",
          "world"
        );
        browser.test.notifyPass("register_scalars_multiple");
      },
      doneSignal: "register_scalars_multiple",
    });
    const scalars = TelemetryTestUtils.getProcessScalars("parent", false, true);
    TelemetryTestUtils.assertScalar(
      scalars,
      "telemetry.test.dynamic.webext_string",
      "hello"
    );
    TelemetryTestUtils.assertScalar(
      scalars,
      "telemetry.test.dynamic.webext_string_too",
      "world"
    );
  });

  add_task(async function test_telemetry_register_scalars_boolean() {
    Services.telemetry.clearScalars();
    await run({
      backgroundScript: async () => {
        await browser.telemetry.registerScalars("telemetry.test.dynamic", {
          webext_boolean: {
            kind: browser.telemetry.ScalarType.BOOLEAN,
            keyed: false,
            record_on_release: true,
          },
        });
        await browser.telemetry.scalarSet(
          "telemetry.test.dynamic.webext_boolean",
          true
        );
        browser.test.notifyPass("register_scalars_boolean");
      },
      doneSignal: "register_scalars_boolean",
    });
    TelemetryTestUtils.assertScalar(
      TelemetryTestUtils.getProcessScalars("dynamic", false, true),
      "telemetry.test.dynamic.webext_boolean",
      true
    );
  });

  add_task(async function test_telemetry_register_scalars_count() {
    Services.telemetry.clearScalars();
    await run({
      backgroundScript: async () => {
        await browser.telemetry.registerScalars("telemetry.test.dynamic", {
          webext_count: {
            kind: browser.telemetry.ScalarType.COUNT,
            keyed: false,
            record_on_release: true,
          },
        });
        await browser.telemetry.scalarSet(
          "telemetry.test.dynamic.webext_count",
          123
        );
        browser.test.notifyPass("register_scalars_count");
      },
      doneSignal: "register_scalars_count",
    });
    TelemetryTestUtils.assertScalar(
      TelemetryTestUtils.getProcessScalars("dynamic", false, true),
      "telemetry.test.dynamic.webext_count",
      123
    );
  });

  add_task(async function test_telemetry_register_events() {
    Services.telemetry.clearEvents();

    await run({
      backgroundScript: async () => {
        await browser.telemetry.registerEvents("telemetry.test.dynamic", {
          test1: {
            methods: ["test1"],
            objects: ["object1"],
            extra_keys: [],
          },
        });
        await browser.telemetry.recordEvent(
          "telemetry.test.dynamic",
          "test1",
          "object1"
        );
        browser.test.notifyPass("register_events");
      },
      doneSignal: "register_events",
    });

    TelemetryTestUtils.assertEvents(
      [
        {
          category: "telemetry.test.dynamic",
          method: "test1",
          object: "object1",
        },
      ],
      { category: "telemetry.test.dynamic" },
      { process: "dynamic" }
    );
  });

  add_task(async function test_telemetry_submit_ping() {
    await run({
      backgroundScript: async () => {
        await browser.telemetry.submitPing("webext-test", {}, {});
        browser.test.notifyPass("submit_ping");
      },
      doneSignal: "submit_ping",
    });

    let pings = await TelemetryArchive.promiseArchivedPingList();
    equal(pings.length, 1);
    equal(pings[0].type, "webext-test");
  });

  add_task(async function test_telemetry_submit_encrypted_ping() {
    await run({
      backgroundScript: async () => {
        try {
          await browser.telemetry.submitEncryptedPing(
            { payload: "encrypted-webext-test" },
            {
              schemaName: "schema-name",
              schemaVersion: 123,
            }
          );
          browser.test.fail(
            "Expected exception without required manifest entries set."
          );
        } catch (e) {
          browser.test.assertTrue(
            e,
            /Encrypted telemetry pings require ping_type and public_key to be set in manifest./
          );
          browser.test.notifyPass("submit_encrypted_ping_fail");
        }
      },
      doneSignal: "submit_encrypted_ping_fail",
    });

    const telemetryManifestEntries = {
      ping_type: "encrypted-webext-ping",
      schemaNamespace: "schema-namespace",
      public_key: {
        id: "pioneer-dev-20200423",
        key: {
          crv: "P-256",
          kty: "EC",
          x: "Qqihp7EryDN2-qQ-zuDPDpy5mJD5soFBDZmzPWTmjwk",
          y: "PiEQVUlywi2bEsA3_5D0VFrCHClCyUlLW52ajYs-5uc",
        },
      },
    };

    await run({
      backgroundScript: async () => {
        await browser.telemetry.submitEncryptedPing(
          {
            payload: "encrypted-webext-test",
          },
          {
            schemaName: "schema-name",
            schemaVersion: 123,
          }
        );
        browser.test.notifyPass("submit_encrypted_ping_pass");
      },
      permissions: ["telemetry"],
      doneSignal: "submit_encrypted_ping_pass",
      isPrivileged: true,
      telemetry: telemetryManifestEntries,
    });

    telemetryManifestEntries.pioneer_id = true;
    telemetryManifestEntries.study_name = "test123";
    Services.prefs.setStringPref("toolkit.telemetry.pioneerId", "test123");

    await run({
      backgroundScript: async () => {
        await browser.telemetry.submitEncryptedPing(
          { payload: "encrypted-webext-test" },
          {
            schemaName: "schema-name",
            schemaVersion: 123,
          }
        );
        browser.test.notifyPass("submit_encrypted_ping_pass");
      },
      permissions: ["telemetry"],
      doneSignal: "submit_encrypted_ping_pass",
      isPrivileged: true,
      telemetry: telemetryManifestEntries,
    });

    // Wait for any pending pings to settle.
    await TelemetryStorage.testClearPendingPings();

    let pings = await TelemetryArchive.promiseArchivedPingList();
    equal(pings.length, 3);
    equal(pings[1].type, "encrypted-webext-ping");
    equal(pings[2].type, "encrypted-webext-ping");
  });

  add_task(async function test_telemetry_can_upload_enabled() {
    Services.prefs.setBoolPref(
      TelemetryUtils.Preferences.FhrUploadEnabled,
      true
    );

    await run({
      backgroundScript: async () => {
        const result = await browser.telemetry.canUpload();
        browser.test.assertTrue(result);
        browser.test.notifyPass("can_upload_enabled");
      },
      doneSignal: "can_upload_enabled",
    });

    Services.prefs.clearUserPref(TelemetryUtils.Preferences.FhrUploadEnabled);
  });

  add_task(async function test_telemetry_can_upload_disabled() {
    Services.prefs.setBoolPref(
      TelemetryUtils.Preferences.FhrUploadEnabled,
      false
    );

    await run({
      backgroundScript: async () => {
        const result = await browser.telemetry.canUpload();
        browser.test.assertFalse(result);
        browser.test.notifyPass("can_upload_disabled");
      },
      doneSignal: "can_upload_disabled",
    });

    Services.prefs.clearUserPref(TelemetryUtils.Preferences.FhrUploadEnabled);
  });
}
