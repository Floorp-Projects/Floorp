const BlocklistGlobal = ChromeUtils.import("resource://gre/modules/Blocklist.jsm", null);

const IS_ANDROID = AppConstants.platform == "android";

let gBlocklistClients;
let server;

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

  // Set up an HTTP Server
  server = new HttpServer();
  server.start(-1);

  // Point the blocklist clients to use this local HTTP server.
  Services.prefs.setCharPref("services.settings.server",
                             `http://localhost:${server.identity.primaryPort}/v1`);

  // Unfortunately security settings are coupled with blocklists clients,
  // this will be fixed in Bug 1526018
  // We disable intermediate preloading because it runs when polling ends, and
  // interferes with `clear_state()` from this test suite.
  Services.prefs.setBoolPref("security.remote_settings.intermediates.enabled", false);

  // This will initialize the remote settings clients for blocklists.
  BlocklistGlobal.ExtensionBlocklistRS.ensureInitialized();
  BlocklistGlobal.PluginBlocklistRS.ensureInitialized();
  BlocklistGlobal.GfxBlocklistRS._ensureInitialized();

  gBlocklistClients = [
    {client: BlocklistGlobal.ExtensionBlocklistRS._client, testData: ["i808", "i720", "i539"]},
    {client: BlocklistGlobal.PluginBlocklistRS._client, testData: ["p1044", "p32", "p28"]},
    {client: BlocklistGlobal.GfxBlocklistRS._client, testData: ["g204", "g200", "g36"]},
  ];
  // Disable signature verification in these tests.
  for (const c of gBlocklistClients) {
    c.verifySignature = false;
  }

  // Setup server fake responses.
  function handleResponse(request, response) {
    try {
      const sample = getSampleResponse(request, server.identity.primaryPort);
      if (!sample) {
        do_throw(`unexpected ${request.method} request for ${request.path}?${request.queryString}`);
      }
      const { status: { status, statusText }, sampleHeaders, responseBody } = sample;

      response.setStatusLine(null, status, statusText);
      // send the headers
      for (const headerLine of sampleHeaders) {
        const headerElements = headerLine.split(":");
        response.setHeader(headerElements[0], headerElements[1].trimLeft());
      }
      response.setHeader("Date", (new Date()).toUTCString());

      response.write(responseBody);
      response.finish();
    } catch (e) {
      info(e);
    }
  }
  const configPath = "/v1/";
  const monitorChangesPath = "/v1/buckets/monitor/collections/changes/records";
  const addonsRecordsPath  = "/v1/buckets/blocklists/collections/addons/records";
  const gfxRecordsPath     = "/v1/buckets/blocklists/collections/gfx/records";
  const pluginsRecordsPath = "/v1/buckets/blocklists/collections/plugins/records";
  server.registerPathHandler(configPath, handleResponse);
  server.registerPathHandler(monitorChangesPath, handleResponse);
  server.registerPathHandler(addonsRecordsPath, handleResponse);
  server.registerPathHandler(gfxRecordsPath, handleResponse);
  server.registerPathHandler(pluginsRecordsPath, handleResponse);


  registerCleanupFunction(function() {
    server.stop(() => { });
  });
  promiseStartupManager().then(run_next_test);
}

add_task(async function test_initial_dump_is_loaded_as_synced_when_collection_is_empty() {
  const november2016 = 1480000000000;

  for (let {client} of gBlocklistClients) {
    if (IS_ANDROID && client.collectionName != BlocklistGlobal.ExtensionBlocklistRS._client.collectionName) {
      // On Android we don't ship the dumps of plugins and gfx.
      continue;
    }

    // Test an empty db populates, but don't reach server (specified timestamp <= dump).
    await client.maybeSync(1);

    // Verify the loaded data has status to synced:
    const collection = await client.openCollection();
    const { data: list } = await collection.list();
    equal(list[0]._status, "synced");

    // Verify that the internal timestamp was updated.
    const timestamp = await collection.db.getLastModified();
    ok(timestamp > november2016, `Loaded dump of ${client.collectionName} has timestamp ${timestamp}`);
  }
});
add_task(clear_state);

add_task(async function test_initial_dump_is_loaded_when_using_get_on_empty_collection() {
  for (let {client} of gBlocklistClients) {
    if (IS_ANDROID && client.collectionName != BlocklistGlobal.ExtensionBlocklistRS._client.collectionName) {
      // On Android we don't ship the dumps of plugins and gfx.
      continue;
    }
    // Internal database is empty.
    const collection = await client.openCollection();
    const { data: list } = await collection.list();
    equal(list.length, 0);

    // Calling .get() will load the dump.
    const afterLoaded = await client.get();
    ok(afterLoaded.length > 0, `Loaded dump of ${client.collectionName} has ${afterLoaded.length} records`);
  }
});
add_task(clear_state);

add_task(async function test_sync_event_data_is_filtered_for_target() {
  // Here we will synchronize 4 times, the first two to initialize the local DB and
  // the last two about event filtered data.
  const timestamp1 = 3000;
  const timestamp2 = 3001;
  const timestamp3 = 4001;
  const timestamp4 = 5001;

  for (let {client} of gBlocklistClients) {
    // Initialize the collection with some data (local is empty, thus no ?_since)
    await client.maybeSync(timestamp1, {loadDump: false});
    // This will pick the data with ?_since=3000.
    await client.maybeSync(timestamp2);

    // In ?_since=4000 entries, no target matches. The sync event is not called.
    let called = false;
    client.on("sync", e => called = true);
    await client.maybeSync(timestamp3);
    equal(called, false, `shouldn't have sync event for ${client.collectionName}`);

    // In ?_since=5000 entries, only one entry matches.
    let syncEventData;
    client.on("sync", e => syncEventData = e.data);
    await client.maybeSync(timestamp4);
    const { current, created, updated, deleted } = syncEventData;
    equal(created.length + updated.length + deleted.length, 1, `event filtered data for ${client.collectionName}`);

    // Since we had entries whose target does not match, the internal storage list
    // and the event current data should differ.
    const collection = await client.openCollection();
    const { data: internalData } = await collection.list();
    ok(internalData.length > current.length, `event current data for ${client.collectionName}`);
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

add_task(async function test_inspect_with_blocklist_clients() {
  const rsSigner = "remote-settings.content-signature.mozilla.org";
  const {
    serverTimestamp,
    localTimestamp,
    lastCheck,
    collections,
  } = await RemoteSettings.inspect();

  equal(serverTimestamp, '"3000"');
  equal(localTimestamp, null); // not yet synchronized.
  equal(lastCheck, 0); // not yet synchronized.

  const defaults = {
    bucket: "blocklists",
    localTimestamp: null,
    lastCheck: 0,
    signerName: rsSigner,
  };
  equal(collections.length, 3);
  deepEqual(collections[0], { ...defaults, collection: "gfx", serverTimestamp: 3000 });
  deepEqual(collections[1], { ...defaults, collection: "addons", serverTimestamp: 3000 });
  deepEqual(collections[2], { ...defaults, collection: "plugins", serverTimestamp: 3000 });

  // Now synchronize, and check that values were updated.
  Services.prefs.setBoolPref("services.settings.load_dump", false);
  const currentTime = Math.floor(Date.now() / 1000);
  await RemoteSettings.pollChanges();

  const inspected = await RemoteSettings.inspect();

  equal(inspected.localTimestamp, '"3000"');
  ok(inspected.lastCheck >= currentTime, `last check ${inspected.lastCheck}`);
  for (const c of inspected.collections) {
    equal(c.localTimestamp, 3000);
    ok(c.lastCheck >= currentTime, `${c.collectionName} last check ${c.lastCheck}`);
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


// get a response for a given request from sample data
function getSampleResponse(req, port) {
  const responses = {
    "OPTIONS": {
      "sampleHeaders": [
        "Access-Control-Allow-Headers: Content-Length,Expires,Backoff,Retry-After,Last-Modified,Total-Records,ETag,Pragma,Cache-Control,authorization,content-type,if-none-match,Alert,Next-Page",
        "Access-Control-Allow-Methods: GET,HEAD,OPTIONS,POST,DELETE,OPTIONS",
        "Access-Control-Allow-Origin: *",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": "null",
    },
    "GET:/v1/": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({
        "settings": {
          "batch_max_requests": 25,
        },
        "url": `http://localhost:${port}/v1/`,
        "documentation": "https://kinto.readthedocs.org/",
        "version": "1.5.1",
        "commit": "cbc6f58",
        "hello": "kinto",
      }),
    },
    "GET:/v1/buckets/monitor/collections/changes/records": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        `Date: ${new Date().toUTCString()}`,
        "Etag: \"3000\"",
      ],
      "status": { status: 200, statusText: "OK" },
      "responseBody": JSON.stringify({
        "data": [{
          "id": "4676f0c7-9757-4796-a0e8-b40a5a37a9c9",
          "bucket": "blocklists",
          "collection": "gfx",
          "last_modified": 3000,  // will become ?_expected=3000
        }, {
          "id": "36b2ebab-d691-4796-b36b-f7a06df38b26",
          "bucket": "blocklists",
          "collection": "addons",
          "last_modified": 3000,  // will become ?_expected=3000
        }, {
          "id": "42aea14b-4b35-4308-94d9-8562412a2fef",
          "bucket": "blocklists",
          "collection": "plugins",
          "last_modified": 3000,  // will become ?_expected=3000
        }, {
          "id": "50f4ef31-7788-4be8-b073-114440be4d8d",
          "bucket": "main",
          "collection": "passwords",
          "last_modified": 2900,
        }, {
          "id": "d2f08123-b815-4bbf-a0b7-a948a65ecafa",
          "bucket": "pinning-preview",
          "collection": "pins",
          "last_modified": 2800,
        }],
      }),
    },
    "GET:/v1/buckets/blocklists/collections/addons/records?_expected=3000&_sort=-last_modified": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"3000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "prefs": [],
        "blockID": "i539",
        "last_modified": 3000,
        "versionRange": [{
          "targetApplication": [],
          "maxVersion": "*",
          "minVersion": "0",
          "severity": "1",
        }],
        "guid": "ScorpionSaver@jetpack",
        "id": "9d500963-d80e-3a91-6e74-66f3811b99cc",
      }]}),
    },
    "GET:/v1/buckets/blocklists/collections/plugins/records?_expected=3000&_sort=-last_modified": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"3000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "matchFilename": "NPFFAddOn.dll",
        "blockID": "p28",
        "id": "7b1e0b3c-e390-a817-11b6-a6887f65f56e",
        "last_modified": 3000,
        "versionRange": [],
      }]}),
    },
    "GET:/v1/buckets/blocklists/collections/gfx/records?_expected=3000&_sort=-last_modified": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"3000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "driverVersionComparator": "LESS_THAN_OR_EQUAL",
        "driverVersion": "8.17.12.5896",
        "vendor": "0x10de",
        "blockID": "g36",
        "feature": "DIRECT3D_9_LAYERS",
        "devices": ["0x0a6c"],
        "featureStatus": "BLOCKED_DRIVER_VERSION",
        "last_modified": 3000,
        "os": "WINNT 6.1",
        "id": "3f947f16-37c2-4e96-d356-78b26363729b",
      }]}),
    },
    "GET:/v1/buckets/blocklists/collections/addons/records?_expected=3001&_sort=-last_modified&_since=3000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"4000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "prefs": [],
        "blockID": "i808",
        "last_modified": 4000,
        "versionRange": [{
          "targetApplication": [],
          "maxVersion": "*",
          "minVersion": "0",
          "severity": "3",
        }],
        "guid": "{c96d1ae6-c4cf-4984-b110-f5f561b33b5a}",
        "id": "9ccfac91-e463-c30c-f0bd-14143794a8dd",
      }, {
        "prefs": ["browser.startup.homepage"],
        "blockID": "i720",
        "last_modified": 3500,
        "versionRange": [{
          "targetApplication": [],
          "maxVersion": "*",
          "minVersion": "0",
          "severity": "1",
        }],
        "guid": "FXqG@xeeR.net",
        "id": "cf9b3129-a97e-dbd7-9525-a8575ac03c25",
      }]}),
    },
    "GET:/v1/buckets/blocklists/collections/plugins/records?_expected=3001&_sort=-last_modified&_since=3000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"4000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "infoURL": "https://get.adobe.com/flashplayer/",
        "blockID": "p1044",
        "matchFilename": "libflashplayer\\.so",
        "last_modified": 4000,
        "versionRange": [{
          "targetApplication": [],
          "minVersion": "11.2.202.509",
          "maxVersion": "*",
          "severity": "0",
          "vulnerabilityStatus": "1",
        }],
        "os": "Linux",
        "id": "aabad965-e556-ffe7-4191-074f5dee3df3",
      }, {
        "matchFilename": "npViewpoint.dll",
        "blockID": "p32",
        "id": "1f48af42-c508-b8ef-b8d5-609d48e4f6c9",
        "last_modified": 3500,
        "versionRange": [{
          "targetApplication": [{
            "minVersion": "3.0",
            "maxVersion": "*",
          }],
        }],
      }]}),
    },
    "GET:/v1/buckets/blocklists/collections/gfx/records?_expected=3001&_sort=-last_modified&_since=3000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"4000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "vendor": "0x8086",
        "blockID": "g204",
        "feature": "WEBGL_MSAA",
        "devices": [],
        "id": "c96bca82-e6bd-044d-14c4-9c1d67e9283a",
        "last_modified": 4000,
        "os": "Darwin 10",
        "featureStatus": "BLOCKED_DEVICE",
      }, {
        "vendor": "0x10de",
        "blockID": "g200",
        "feature": "WEBGL_MSAA",
        "devices": [],
        "id": "c3a15ba9-e0e2-421f-e399-c995e5b8d14e",
        "last_modified": 3500,
        "os": "Darwin 11",
        "featureStatus": "BLOCKED_DEVICE",
      }]}),
    },
    "GET:/v1/buckets/blocklists/collections/addons/records?_expected=4001&_sort=-last_modified&_since=4000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"5000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "last_modified": 4001,
        "versionRange": [{
          "targetApplication": [{
            "guid": "some-guid",
          }],
        }],
        "id": "8f03b264-57b7-4263-9b15-ad91b033a034",
      }]}),
    },
    "GET:/v1/buckets/blocklists/collections/plugins/records?_expected=4001&_sort=-last_modified&_since=4000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"5000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "last_modified": 4001,
        "versionRange": [{
          "targetApplication": [{
            "guid": "xpcshell@tests.mozilla.org",
            "minVersion": "0",
            "maxVersion": "57.*",
          }],
        }],
        "id": "cd3ea0b2-1ba8-4fb6-b242-976a87626116",
      }]}),
    },
    "GET:/v1/buckets/blocklists/collections/gfx/records?_expected=4001&_sort=-last_modified&_since=4000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"5000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "last_modified": 4001,
        "versionRange": [{
          "targetApplication": [{
            "guid": "xpcshell@tests.mozilla.org",
            "maxVersion": "20",
          }],
        }],
        "id": "86771771-e803-4006-95e9-c9275d58b3d1",
      }]}),
    },
    "GET:/v1/buckets/blocklists/collections/addons/records?_expected=5001&_sort=-last_modified&_since=5000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"6000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        // delete an entry with non matching target (see above)
        "last_modified": 5001,
        "deleted": true,
        "id": "8f03b264-57b7-4263-9b15-ad91b033a034",
      }, {
        // delete entry with matching target (see above)
        "last_modified": 5002,
        "deleted": true,
        "id": "9ccfac91-e463-c30c-f0bd-14143794a8dd",
      }, {
        // create an extra non matching
        "last_modified": 5003,
        "id": "75b36589-435a-48d4-8ee4-bacee3fb6119",
        "versionRange": [{
          "targetApplication": [{
            "guid": "xpcshell@tests.mozilla.org",
            "minVersion": "0",
            "maxVersion": "57.*",
          }],
        }],
      }]}),
    },
    "GET:/v1/buckets/blocklists/collections/plugins/records?_expected=5001&_sort=-last_modified&_since=5000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"6000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        // entry with non matching target (see above)
        "newAttribute": 42,
        "versionRange": [{
          "targetApplication": [{
            "guid": "xpcshell@tests.mozilla.org",
            "minVersion": "0",
            "maxVersion": "57.*",
          }],
        }],
        "id": "cd3ea0b2-1ba8-4fb6-b242-976a87626116",
      }, {
        // entry with matching target (see above)
        "newAttribute": 42,
        "matchFilename": "npViewpoint.dll",
        "blockID": "p32",
        "id": "1f48af42-c508-b8ef-b8d5-609d48e4f6c9",
        "last_modified": 3500,
        "versionRange": [{
          "targetApplication": [{
            "guid": "xpcshell@tests.mozilla.org",
            "minVersion": "3.0",
            "maxVersion": "*",
          }],
        }],
      }]}),
    },
    "GET:/v1/buckets/blocklists/collections/gfx/records?_expected=5001&_sort=-last_modified&_since=5000": {
      "sampleHeaders": [
        "Access-Control-Allow-Origin: *",
        "Access-Control-Expose-Headers: Retry-After, Content-Length, Alert, Backoff",
        "Content-Type: application/json; charset=UTF-8",
        "Server: waitress",
        "Etag: \"6000\"",
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "versionRange": [{
          "targetApplication": [{
            "guid": "xpcshell@tests.mozilla.org",
            "minVersion": "0",
            "maxVersion": "*",
          }],
        }],
        "id": "43031a81-5f36-4eef-9b35-52f0bbeba363",
      }, {
        "versionRange": [{
          "targetApplication": [{
            "guid": "xpcshell@tests.mozilla.org",
            "minVersion": "0",
            "maxVersion": "3",
          }],
        }],
        "id": "75a06bd3-f906-427d-a448-02092ee589fc",
      }]}),
    },
  };
  return responses[`${req.method}:${req.path}?${req.queryString}`] ||
         responses[`${req.method}:${req.path}`] ||
         responses[req.method];
}
