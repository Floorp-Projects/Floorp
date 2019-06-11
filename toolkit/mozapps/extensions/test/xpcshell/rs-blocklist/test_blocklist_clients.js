const BlocklistGlobal = ChromeUtils.import("resource://gre/modules/Blocklist.jsm", null);
const { Utils: RemoteSettingsUtils } = ChromeUtils.import("resource://services-settings/Utils.jsm");

const IS_ANDROID = AppConstants.platform == "android";

let gBlocklistClients;

async function clear_state() {
  for (let {client} of gBlocklistClients) {
    // Remove last server times.
    Services.prefs.clearUserPref(client.lastCheckTimePref);

    // Clear local DB.
    const collection = await client.openCollection();
    await collection.clear();
  }
}


function run_test() {
  AddonTestUtils.createAppInfo("XPCShell", "xpcshell@tests.mozilla.org", "1", "");

  // This will initialize the remote settings clients for blocklists.
  BlocklistGlobal.ExtensionBlocklistRS.ensureInitialized();
  BlocklistGlobal.PluginBlocklistRS.ensureInitialized();
  BlocklistGlobal.GfxBlocklistRS._ensureInitialized();

  gBlocklistClients = [
    {client: BlocklistGlobal.ExtensionBlocklistRS._client, testData: ["i808", "i720", "i539"]},
    {client: BlocklistGlobal.PluginBlocklistRS._client, testData: ["p1044", "p32", "p28"]},
    {client: BlocklistGlobal.GfxBlocklistRS._client, testData: ["g204", "g200", "g36"]},
  ];

  promiseStartupManager().then(run_next_test);
}

add_task(async function test_initial_dump_is_loaded_as_synced_when_collection_is_empty() {
  for (let {client} of gBlocklistClients) {
    if (IS_ANDROID && client.collectionName != BlocklistGlobal.ExtensionBlocklistRS._client.collectionName) {
      // On Android we don't ship the dumps of plugins and gfx.
      continue;
    }
    Assert.ok(await RemoteSettingsUtils.hasLocalDump(client.bucketName, client.collectionName));
  }
});
add_task(clear_state);

add_task(async function test_data_is_filtered_for_target() {
  const initial = [{
    "versionRange": [{
      "targetApplication": [],
      "maxVersion": "*",
      "minVersion": "0",
      "severity": "1",
    }],
  }];
  const noMatchingTarget = [{
    "versionRange": [{
      "targetApplication": [],
      "maxVersion": "*",
      "minVersion": "0",
      "severity": "3",
    }],
  }, {
    "versionRange": [{
      "targetApplication": [],
      "maxVersion": "*",
      "minVersion": "0",
      "severity": "1",
    }],
  }];
  const oneMatch = [{
    "versionRange": [{
      "targetApplication": [{
        "guid": "some-guid",
      }],
    }],
  }];

  const records = initial.concat(noMatchingTarget).concat(oneMatch);

  for (let {client} of gBlocklistClients) {
    // Initialize the collection with some data
    const collection = await client.openCollection();
    for (const record of records) {
      await collection.create(record);
    }

    const { data: internalData } = await collection.list();
    Assert.equal(internalData.length, records.length);
    Assert.equal((await client.get()).length, 2); // only two matches.
  }
});
add_task(clear_state);

add_task(async function test_entries_are_filtered_when_jexl_filter_expression_is_present() {
  const records = [{
      willMatch: true,
    }, {
      willMatch: true,
      filter_expression: null,
    }, {
      willMatch: true,
      filter_expression: "1 == 1",
    }, {
      willMatch: false,
      filter_expression: "1 == 2",
    }, {
      willMatch: true,
      filter_expression: "1 == 1",
      versionRange: [{
        targetApplication: [{
          guid: "some-guid",
        }],
      }],
    }, {
      willMatch: false,  // jexl prevails over versionRange.
      filter_expression: "1 == 2",
      versionRange: [{
        targetApplication: [{
          guid: "xpcshell@tests.mozilla.org",
          minVersion: "0",
          maxVersion: "*",
        }],
      }],
    },
  ];
  for (let {client} of gBlocklistClients) {
    const collection = await client.openCollection();
    for (const record of records) {
      await collection.create(record);
    }
    await collection.db.saveLastModified(42); // Prevent from loading JSON dump.
    const list = await client.get();
    equal(list.length, 4);
    ok(list.every(e => e.willMatch));
  }
});
add_task(clear_state);

add_task(async function test_bucketname_changes_when_bucket_pref_changes() {
  for (const { client } of gBlocklistClients) {
    equal(client.bucketName, "blocklists");
  }
  equal(BlocklistGlobal.ExtensionBlocklistRS._client.bucketName, "addons");

  Services.prefs.setCharPref("services.blocklist.bucket", "blocklists-preview");
  Services.prefs.setCharPref("services.blocklist.addons.bucket", "addons-preview");

  for (const { client } of gBlocklistClients) {
    equal(client.bucketName, "blocklists-preview", client.identifier);
  }
  equal(BlocklistGlobal.ExtensionBlocklistRS._client.bucketName, "addons-preview");
  Services.prefs.clearUserPref("services.blocklist.bucket");
  Services.prefs.clearUserPref("services.blocklist.addons.bucket");
});
add_task(clear_state);
