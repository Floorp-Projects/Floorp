const BlocklistClients = ChromeUtils.import("resource://services-common/blocklist-clients.js", {});
const { RemoteSettings } = ChromeUtils.import("resource://services-settings/remote-settings.js", {});

const APP_ID = "xpcshell@tests.mozilla.org";
const TOOLKIT_ID = "toolkit@mozilla.org";

let client;

async function clear_state() {
  // Clear local DB.
  const collection = await client.openCollection();
  await collection.clear();
}

async function createRecords(records) {
  const collection = await client.openCollection();
  for (const record of records) {
    await collection.create(record);
  }
  collection.db.saveLastModified(42); // Simulate sync (and prevent load dump).
}


function run_test() {
  // This will initialize the remote settings clients for blocklists,
  // with their specific options etc.
  BlocklistClients.initialize();
  // Obtain one of the instantiated client for our tests.
  client = RemoteSettings("plugins", { bucketName: "blocklists" });

  run_next_test();
}

add_task(async function test_returns_all_without_target() {
  await createRecords([{
    matchName: "Adobe Flex"
  }, {
    versionRange: [],
  }, {
    matchName: "PDF reader",
    versionRange: [{
      severity: 0,
      vulnerabilityStatus: 1,
      targetApplication: []
    }]
  }, {
    matchName: "Java(\\(TM\\))? Plug-in 11\\.(7[6-9]|[8-9]\\d|1([0-6]\\d|70))(\\.\\d+)?([^\\d\\._]|$)",
    versionRange: [{
      severity: 0,
      vulnerabilityStatus: 1
    }],
    matchFilename: "libnpjp2\\.so",
  }, {
    versionRange: [{
      targetApplication: [],
      maxVersion: "1",
      minVersion: "0",
      severity: "1"
    }]
  }]);

  const list = await client.get();
  equal(list.length, 5);
});
add_task(clear_state);

add_task(async function test_returns_without_guid_or_with_matching_guid() {
  await createRecords([{
    willMatch: true,
    versionRange: [{
      targetApplication: [{
      }]
    }]
  }, {
    willMatch: false,
    versionRange: [{
      targetApplication: [{
        guid: "some-guid",
      }]
    }]
  }, {
    willMatch: true,
    versionRange: [{
      targetApplication: [{
        guid: APP_ID,
      }]
    }]
  }, {
    willMatch: true,
    versionRange: [{
      targetApplication: [{
        guid: TOOLKIT_ID,
      }]
    }]
  }]);

  const list = await client.get();
  equal(list.length, 3);
  ok(list.every(e => e.willMatch));
});
add_task(clear_state);

add_task(async function test_returns_without_app_version_or_with_matching_version() {
  await createRecords([{
    willMatch: true,
    versionRange: [{
      targetApplication: [{
        guid: APP_ID,
      }]
    }]
  }, {
    willMatch: true,
    versionRange: [{
      targetApplication: [{
        guid: APP_ID,
        minVersion: "0",
      }]
    }]
  }, {
    willMatch: true,
    versionRange: [{
      targetApplication: [{
        guid: APP_ID,
        minVersion: "0",
        maxVersion: "9999",
      }]
    }]
  }, {
    willMatch: false,
    versionRange: [{
      targetApplication: [{
        guid: APP_ID,
        minVersion: "0",
        maxVersion: "1",
      }]
    }]
  }, {
    willMatch: true,
    versionRange: [{
      targetApplication: [{
        guid: TOOLKIT_ID,
        minVersion: "0",
      }]
    }]
  }, {
    willMatch: true,
    versionRange: [{
      targetApplication: [{
        guid: TOOLKIT_ID,
        minVersion: "0",
        maxVersion: "9999",
      }]
    }]
    // We can't test the false case with maxVersion for toolkit, because the toolkit version
    // is 0 in xpcshell.
  }]);

  const list = await client.get();
  equal(list.length, 5);
  ok(list.every(e => e.willMatch));
});
add_task(clear_state);

add_task(async function test_multiple_version_and_target_applications() {
  await createRecords([{
    willMatch: true,
    versionRange: [{
      targetApplication: [{
        guid: "other-guid",
      }]
    }, {
      targetApplication: [{
        guid: APP_ID,
        minVersion: "0",
        maxVersion: "*",
      }]
    }]
  }, {
    willMatch: true,
    versionRange: [{
      targetApplication: [{
        guid: "other-guid",
      }]
    }, {
      targetApplication: [{
        guid: APP_ID,
        minVersion: "0",
      }]
    }]
  }, {
    willMatch: false,
    versionRange: [{
      targetApplication: [{
        guid: APP_ID,
        maxVersion: "57.*"
      }]
    }, {
      targetApplication: [{
        guid: APP_ID,
        maxVersion: "56.*"
      }, {
        guid: APP_ID,
        maxVersion: "57.*"
      }]
    }]
  }]);

  const list = await client.get();
  equal(list.length, 2);
  ok(list.every(e => e.willMatch));
});
add_task(clear_state);

add_task(async function test_complex_version() {
  await createRecords([{
    willMatch: false,
    versionRange: [{
      targetApplication: [{
        guid: APP_ID,
        maxVersion: "57.*"
      }]
    }]
  }, {
    willMatch: true,
    versionRange: [{
      targetApplication: [{
        guid: APP_ID,
        maxVersion: "9999.*"
      }]
    }]
  }, {
    willMatch: true,
    versionRange: [{
      targetApplication: [{
        guid: APP_ID,
        minVersion: "19.0a1"
      }]
    }]
  }]);

  const list = await client.get();
  equal(list.length, 2);
});
add_task(clear_state);
