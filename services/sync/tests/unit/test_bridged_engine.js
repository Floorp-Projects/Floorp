/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

const { BridgedEngine } = ChromeUtils.import(
  "resource://services-sync/bridged_engine.js"
);
const { Service } = ChromeUtils.import("resource://services-sync/service.js");

// Wraps an `object` in a proxy so that its methods are bound to it. This
// simulates how XPCOM class instances have all their methods bound.
function withBoundMethods(object) {
  return new Proxy(object, {
    get(target, key) {
      let value = target[key];
      return typeof value == "function" ? value.bind(target) : value;
    },
  });
}

add_task(async function test_interface() {
  class TestBridge {
    constructor() {
      this.storageVersion = 2;
      this.syncID = "syncID111111";
      this.wasInitialized = false;
      this.clear();
    }

    clear() {
      this.lastSyncMillis = 0;
      this.incomingRecords = [];
      this.uploadedIDs = [];
      this.wasSynced = false;
      this.wasReset = false;
      this.wasWiped = false;
    }

    // `mozIBridgedSyncEngine` methods.

    initialize(callback) {
      ok(
        !this.wasInitialized,
        "Shouldn't initialize a bridged engine more than once"
      );
      this.wasInitialized = true;
      CommonUtils.nextTick(() => callback.handleSuccess());
    }

    getLastSync(callback) {
      ok(
        this.wasInitialized,
        "Should initialize before getting last sync time"
      );
      CommonUtils.nextTick(() => callback.handleSuccess(this.lastSyncMillis));
    }

    setLastSync(millis, callback) {
      ok(
        this.wasInitialized,
        "Should initialize before setting last sync time"
      );
      this.lastSyncMillis = millis;
      CommonUtils.nextTick(() => callback.handleSuccess());
    }

    resetSyncId(callback) {
      ok(this.wasInitialized, "Should initialize before resetting sync ID");
      CommonUtils.nextTick(() => callback.handleSuccess(this.syncID));
    }

    ensureCurrentSyncId(newSyncId, callback) {
      ok(
        this.wasInitialized,
        "Should initialize before ensuring current sync ID"
      );
      equal(newSyncId, this.syncID, "Local and new sync IDs should match");
      CommonUtils.nextTick(() => callback.handleSuccess(this.syncID));
    }

    storeIncoming(records, callback) {
      ok(
        this.wasInitialized,
        "Should initialize before storing incoming records"
      );
      this.incomingRecords.push(...records.map(r => JSON.parse(r)));
      CommonUtils.nextTick(() => callback.handleSuccess());
    }

    apply(callback) {
      ok(this.wasInitialized, "Should initialize before applying records");
      let outgoingRecords = [
        {
          id: "hanson",
          data: {
            plants: ["seed", "flower 💐", "rose"],
            canYouTell: false,
          },
        },
        {
          id: "sheryl-crow",
          data: {
            today: "winding 🛣",
            tomorrow: "winding 🛣",
          },
        },
      ].map(r => JSON.stringify(r));
      CommonUtils.nextTick(() => callback.handleSuccess(outgoingRecords));
      return { cancel() {} };
    }

    setUploaded(millis, ids, callback) {
      ok(
        this.wasInitialized,
        "Should initialize before setting records as uploaded"
      );
      this.uploadedIDs.push(...ids);
      CommonUtils.nextTick(() => callback.handleSuccess());
      return { cancel() {} };
    }

    syncFinished(callback) {
      ok(
        this.wasInitialized,
        "Should initialize before flagging sync as finished"
      );
      this.wasSynced = true;
      CommonUtils.nextTick(() => callback.handleSuccess());
      return { cancel() {} };
    }

    reset(callback) {
      ok(this.wasInitialized, "Should initialize before resetting");
      this.clear();
      this.wasReset = true;
      CommonUtils.nextTick(() => callback.handleSuccess());
    }

    wipe(callback) {
      ok(this.wasInitialized, "Should initialize before wiping");
      this.clear();
      this.wasWiped = true;
      CommonUtils.nextTick(() => callback.handleSuccess());
    }
  }

  let bridge = new TestBridge();
  let engine = new BridgedEngine(withBoundMethods(bridge), "Nineties", Service);
  engine.enabled = true;

  let server = await serverForFoo(engine);
  try {
    await SyncTestingInfrastructure(server);

    info("Add server records");
    let foo = server.user("foo");
    let collection = foo.collection("nineties");
    let now = new_timestamp();
    collection.insert(
      "backstreet",
      encryptPayload({
        id: "backstreet",
        data: {
          say: "I want it that way",
          when: "never",
        },
      }),
      now
    );
    collection.insert(
      "tlc",
      encryptPayload({
        id: "tlc",
        data: {
          forbidden: ["scrubs 🚫"],
          numberAvailable: false,
        },
      }),
      now + 5
    );

    info("Sync the engine");
    // Advance the last sync time to skip the Backstreet Boys...
    bridge.lastSyncMillis = now + 2;
    await sync_engine_and_validate_telem(engine, false);

    let metaGlobal = foo
      .collection("meta")
      .wbo("global")
      .get();
    deepEqual(
      JSON.parse(metaGlobal.payload).engines.nineties,
      {
        version: 2,
        syncID: "syncID111111",
      },
      "Should write storage version and sync ID to m/g"
    );

    greater(bridge.lastSyncMillis, 0, "Should update last sync time");
    deepEqual(
      bridge.incomingRecords.sort((a, b) => a.id.localeCompare(b.id)),
      [
        {
          id: "tlc",
          data: {
            forbidden: ["scrubs 🚫"],
            numberAvailable: false,
          },
        },
      ],
      "Should stage incoming records from server"
    );
    deepEqual(
      bridge.uploadedIDs.sort(),
      ["hanson", "sheryl-crow"],
      "Should mark new local records as uploaded"
    );
    ok(bridge.wasSynced, "Should have finished sync after uploading");

    deepEqual(
      collection.keys().sort(),
      ["backstreet", "hanson", "sheryl-crow", "tlc"],
      "Should have all records on server"
    );
    let expectedRecords = [
      {
        id: "sheryl-crow",
        data: {
          today: "winding 🛣",
          tomorrow: "winding 🛣",
        },
      },
      {
        id: "hanson",
        data: {
          plants: ["seed", "flower 💐", "rose"],
          canYouTell: false,
        },
      },
    ];
    for (let expected of expectedRecords) {
      let actual = collection.cleartext(expected.id);
      deepEqual(
        actual,
        expected,
        `Should upload record ${expected.id} from bridged engine`
      );
    }

    await engine.resetClient();
    ok(bridge.wasReset, "Should reset local storage for bridge");

    await engine.wipeClient();
    ok(bridge.wasWiped, "Should wipe local storage for bridge");

    await engine.resetSyncID();
    ok(
      !foo.collection("nineties"),
      "Should delete server collection after resetting sync ID"
    );
  } finally {
    await promiseStopServer(server);
    await engine.finalize();
  }
});
