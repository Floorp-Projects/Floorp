const BlocklistGlobal = ChromeUtils.import(
  "resource://gre/modules/Blocklist.jsm",
  null
);
const { RemoteSettings } = ChromeUtils.import(
  "resource://services-settings/remote-settings.js"
);

const APP_ID = "xpcshell@tests.mozilla.org";
const TOOLKIT_ID = "toolkit@mozilla.org";

let client;

async function clear_state() {
  // Clear local DB.
  await client.db.clear();
}

async function createRecords(records) {
  const withId = records.map((record, i) => ({
    id: `record-${i}`,
    ...record,
  }));
  return client.db.importChanges({}, 42, withId);
}

function run_test() {
  AddonTestUtils.createAppInfo(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "58",
    ""
  );
  // This will initialize the remote settings clients for blocklists,
  // with their specific options etc.
  BlocklistGlobal.ExtensionBlocklistRS.ensureInitialized();
  // Obtain one of the instantiated client for our tests.
  client = RemoteSettings("addons", { bucketName: "blocklists" });

  run_next_test();
}

add_task(async function test_supports_filter_expressions() {
  await createRecords([
    {
      name: "My Extension",
      filter_expression: 'env.appinfo.ID == "xpcshell@tests.mozilla.org"',
    },
    {
      name: "My Extension",
      filter_expression: "1 == 2",
    },
  ]);

  const list = await client.get();
  equal(list.length, 1);
});
add_task(clear_state);

add_task(async function test_returns_all_without_target() {
  await createRecords([
    {
      name: "My Extension",
    },
    {
      name: "foopydoo",
      versionRange: [],
    },
    {
      name: "My Other Extension",
      versionRange: [
        {
          severity: 0,
          targetApplication: [],
        },
      ],
    },
    {
      name:
        "Java(\\(TM\\))? Plug-in 11\\.(7[6-9]|[8-9]\\d|1([0-6]\\d|70))(\\.\\d+)?([^\\d\\._]|$)",
      versionRange: [
        {
          severity: 0,
        },
      ],
      matchFilename: "libnpjp2\\.so",
    },
    {
      name: "foopydoo",
      versionRange: [
        {
          targetApplication: [],
          maxVersion: "1",
          minVersion: "0",
          severity: "1",
        },
      ],
    },
  ]);

  const list = await client.get();
  equal(list.length, 5);
});
add_task(clear_state);

add_task(async function test_returns_without_guid_or_with_matching_guid() {
  await createRecords([
    {
      willMatch: true,
      name: "foopydoo",
      versionRange: [
        {
          targetApplication: [{}],
        },
      ],
    },
    {
      willMatch: false,
      name: "foopydoo",
      versionRange: [
        {
          targetApplication: [
            {
              guid: "some-guid",
            },
          ],
        },
      ],
    },
    {
      willMatch: true,
      name: "foopydoo",
      versionRange: [
        {
          targetApplication: [
            {
              guid: APP_ID,
            },
          ],
        },
      ],
    },
    {
      willMatch: true,
      name: "foopydoo",
      versionRange: [
        {
          targetApplication: [
            {
              guid: TOOLKIT_ID,
            },
          ],
        },
      ],
    },
  ]);

  const list = await client.get();
  info(JSON.stringify(list, null, 2));
  equal(list.length, 3);
  ok(list.every(e => e.willMatch));
});
add_task(clear_state);

add_task(
  async function test_returns_without_app_version_or_with_matching_version() {
    await createRecords([
      {
        willMatch: true,
        name: "foopydoo",
        versionRange: [
          {
            targetApplication: [
              {
                guid: APP_ID,
              },
            ],
          },
        ],
      },
      {
        willMatch: true,
        name: "foopydoo",
        versionRange: [
          {
            targetApplication: [
              {
                guid: APP_ID,
                minVersion: "0",
              },
            ],
          },
        ],
      },
      {
        willMatch: true,
        name: "foopydoo",
        versionRange: [
          {
            targetApplication: [
              {
                guid: APP_ID,
                minVersion: "0",
                maxVersion: "9999",
              },
            ],
          },
        ],
      },
      {
        willMatch: false,
        name: "foopydoo",
        versionRange: [
          {
            targetApplication: [
              {
                guid: APP_ID,
                minVersion: "0",
                maxVersion: "1",
              },
            ],
          },
        ],
      },
      {
        willMatch: true,
        name: "foopydoo",
        versionRange: [
          {
            targetApplication: [
              {
                guid: TOOLKIT_ID,
                minVersion: "0",
              },
            ],
          },
        ],
      },
      {
        willMatch: true,
        name: "foopydoo",
        versionRange: [
          {
            targetApplication: [
              {
                guid: TOOLKIT_ID,
                minVersion: "0",
                maxVersion: "9999",
              },
            ],
          },
        ],
        // We can't test the false case with maxVersion for toolkit, because the toolkit version
        // is 0 in xpcshell.
      },
    ]);

    const list = await client.get();
    equal(list.length, 5);
    ok(list.every(e => e.willMatch));
  }
);
add_task(clear_state);

add_task(async function test_multiple_version_and_target_applications() {
  await createRecords([
    {
      willMatch: true,
      name: "foopydoo",
      versionRange: [
        {
          targetApplication: [
            {
              guid: "other-guid",
            },
          ],
        },
        {
          targetApplication: [
            {
              guid: APP_ID,
              minVersion: "0",
              maxVersion: "*",
            },
          ],
        },
      ],
    },
    {
      willMatch: true,
      name: "foopydoo",
      versionRange: [
        {
          targetApplication: [
            {
              guid: "other-guid",
            },
          ],
        },
        {
          targetApplication: [
            {
              guid: APP_ID,
              minVersion: "0",
            },
          ],
        },
      ],
    },
    {
      willMatch: false,
      name: "foopydoo",
      versionRange: [
        {
          targetApplication: [
            {
              guid: APP_ID,
              maxVersion: "57.*",
            },
          ],
        },
        {
          targetApplication: [
            {
              guid: APP_ID,
              maxVersion: "56.*",
            },
            {
              guid: APP_ID,
              maxVersion: "57.*",
            },
          ],
        },
      ],
    },
  ]);

  const list = await client.get();
  equal(list.length, 2);
  ok(list.every(e => e.willMatch));
});
add_task(clear_state);

add_task(async function test_complex_version() {
  await createRecords([
    {
      willMatch: false,
      name: "foopydoo",
      versionRange: [
        {
          targetApplication: [
            {
              guid: APP_ID,
              maxVersion: "57.*",
            },
          ],
        },
      ],
    },
    {
      willMatch: true,
      name: "foopydoo",
      versionRange: [
        {
          targetApplication: [
            {
              guid: APP_ID,
              maxVersion: "9999.*",
            },
          ],
        },
      ],
    },
    {
      willMatch: true,
      name: "foopydoo",
      versionRange: [
        {
          targetApplication: [
            {
              guid: APP_ID,
              minVersion: "19.0a1",
            },
          ],
        },
      ],
    },
  ]);

  const list = await client.get();
  equal(list.length, 2);
});
add_task(clear_state);
