const BlocklistGlobal = ChromeUtils.import(
  "resource://gre/modules/Blocklist.jsm",
  null
);
const { Utils: RemoteSettingsUtils } = ChromeUtils.import(
  "resource://services-settings/Utils.jsm"
);

const IS_ANDROID = AppConstants.platform == "android";

let gBlocklistClients;

async function clear_state() {
  for (let { client } of gBlocklistClients) {
    // Remove last server times.
    Services.prefs.clearUserPref(client.lastCheckTimePref);

    // Clear local DB.
    await client.db.clear();
  }
}

add_task(async function setup() {
  AddonTestUtils.createAppInfo(
    "XPCShell",
    "xpcshell@tests.mozilla.org",
    "1",
    ""
  );

  // This will initialize the remote settings clients for blocklists.
  BlocklistGlobal.ExtensionBlocklistRS.ensureInitialized();
  BlocklistGlobal.GfxBlocklistRS._ensureInitialized();

  // ExtensionBlocklistMLBF is covered by test_blocklist_mlbf_dump.js.
  gBlocklistClients = [
    {
      client: BlocklistGlobal.ExtensionBlocklistRS._client,
      expectHasDump: IS_ANDROID,
    },
    {
      client: BlocklistGlobal.GfxBlocklistRS._client,
      expectHasDump: !IS_ANDROID,
    },
  ];

  await promiseStartupManager();
});

add_task(
  async function test_initial_dump_is_loaded_as_synced_when_collection_is_empty() {
    for (let { client, expectHasDump } of gBlocklistClients) {
      Assert.equal(
        await RemoteSettingsUtils.hasLocalDump(
          client.bucketName,
          client.collectionName
        ),
        expectHasDump,
        `Expected initial remote settings dump for ${client.collectionName}`
      );
    }
  }
);
add_task(clear_state);

add_task(async function test_data_is_filtered_for_target() {
  const initial = [
    {
      guid: "foo",
      matchName: "foo",
      versionRange: [
        {
          targetApplication: [],
          maxVersion: "*",
          minVersion: "0",
          severity: "1",
        },
      ],
    },
  ];
  const noMatchingTarget = [
    {
      guid: "foo",
      matchName: "foo",
      versionRange: [
        {
          targetApplication: [{ guid: "Foo" }],
          maxVersion: "*",
          minVersion: "0",
          severity: "3",
        },
      ],
    },
    {
      guid: "foo",
      matchName: "foo",
      versionRange: [
        {
          targetApplication: [{ guid: "XPCShell", maxVersion: "0.1" }],
          maxVersion: "*",
          minVersion: "0",
          severity: "1",
        },
      ],
    },
  ];
  const oneMatch = [
    {
      guid: "foo",
      matchName: "foo",
      versionRange: [
        {
          targetApplication: [
            {
              guid: "XPCShell",
            },
          ],
        },
      ],
    },
  ];

  const records = initial.concat(noMatchingTarget).concat(oneMatch);

  for (let { client } of gBlocklistClients) {
    // Initialize the collection with some data
    for (const record of records) {
      await client.db.create(record);
    }

    const internalData = await client.db.list();
    Assert.equal(internalData.length, records.length);
    let filtered = await client.get({ syncIfEmpty: false });
    Assert.equal(filtered.length, 2); // only two matches.
  }
});
add_task(clear_state);

add_task(
  async function test_entries_are_filtered_when_jexl_filter_expression_is_present() {
    const records = [
      {
        guid: "foo",
        matchName: "foo",
        willMatch: true,
      },
      {
        guid: "foo",
        matchName: "foo",
        willMatch: true,
        filter_expression: null,
      },
      {
        guid: "foo",
        matchName: "foo",
        willMatch: true,
        filter_expression: "1 == 1",
      },
      {
        guid: "foo",
        matchName: "foo",
        willMatch: false,
        filter_expression: "1 == 2",
      },
      {
        guid: "foo",
        matchName: "foo",
        willMatch: true,
        filter_expression: "1 == 1",
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
        guid: "foo",
        matchName: "foo",
        willMatch: false, // jexl prevails over versionRange.
        filter_expression: "1 == 2",
        versionRange: [
          {
            targetApplication: [
              {
                guid: "xpcshell@tests.mozilla.org",
                minVersion: "0",
                maxVersion: "*",
              },
            ],
          },
        ],
      },
    ];
    for (let { client } of gBlocklistClients) {
      for (const record of records) {
        await client.db.create(record);
      }
      await client.db.importChanges({}, 42); // Prevent from loading JSON dump.
      const list = await client.get({ syncIfEmpty: false });
      equal(list.length, 4);
      ok(list.every(e => e.willMatch));
    }
  }
);
add_task(clear_state);

add_task(async function test_bucketname_changes_when_bucket_pref_changes() {
  for (const { client } of gBlocklistClients) {
    equal(client.bucketName, "blocklists");
  }

  Services.prefs.setCharPref("services.blocklist.bucket", "blocklists-preview");

  for (const { client } of gBlocklistClients) {
    equal(client.bucketName, "blocklists-preview", client.identifier);
  }
  Services.prefs.clearUserPref("services.blocklist.bucket");
});
add_task(clear_state);
