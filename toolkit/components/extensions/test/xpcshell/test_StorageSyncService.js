/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

XPCOMUtils.defineLazyServiceGetter(
  this,
  "StorageSyncService",
  "@mozilla.org/extensions/storage/sync;1",
  "nsIInterfaceRequestor"
);

function promisify(func, ...params) {
  return new Promise((resolve, reject) => {
    let changes = [];
    func(...params, {
      QueryInterface: ChromeUtils.generateQI([
        Ci.mozIExtensionStorageListener,
        Ci.mozIExtensionStorageCallback,
        Ci.mozIBridgedSyncEngineCallback,
        Ci.mozIBridgedSyncEngineApplyCallback,
      ]),
      onChanged(json) {
        changes.push(JSON.parse(json));
      },
      handleSuccess(value) {
        resolve({
          changes,
          value: typeof value == "string" ? JSON.parse(value) : value,
        });
      },
      handleError(code, message) {
        reject(Components.Exception(message, code));
      },
    });
  });
}

add_task(async function setup_storage_sync() {
  // So that we can write to the profile directory.
  do_get_profile();
});

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_WEBEXT_STORAGE,
  },
  async function test_storage_sync_service() {
    const service = StorageSyncService.getInterface(
      Ci.mozIExtensionStorageArea
    );
    {
      let { changes, value } = await promisify(
        service.set,
        "ext-1",
        JSON.stringify({
          hi: "hello! 💖",
          bye: "adiós",
        })
      );
      deepEqual(
        changes,
        [
          {
            hi: {
              newValue: "hello! 💖",
            },
            bye: {
              newValue: "adiós",
            },
          },
        ],
        "`set` should notify listeners about changes"
      );
      ok(!value, "`set` should not return a value");
    }

    {
      let { changes, value } = await promisify(
        service.get,
        "ext-1",
        JSON.stringify(["hi"])
      );
      deepEqual(changes, [], "`get` should not notify listeners");
      deepEqual(
        value,
        {
          hi: "hello! 💖",
        },
        "`get` with key should return value"
      );

      let { value: allValues } = await promisify(service.get, "ext-1", "null");
      deepEqual(
        allValues,
        {
          hi: "hello! 💖",
          bye: "adiós",
        },
        "`get` without a key should return all values"
      );
    }

    {
      await promisify(
        service.set,
        "ext-2",
        JSON.stringify({
          hi: "hola! 👋",
        })
      );
      await promisify(service.clear, "ext-1");
      let { value: allValues } = await promisify(service.get, "ext-1", "null");
      deepEqual(allValues, {}, "clear removed ext-1");

      let { value: allValues2 } = await promisify(service.get, "ext-2", "null");
      deepEqual(allValues2, { hi: "hola! 👋" }, "clear didn't remove ext-2");
      // We need to clear data for ext-2 too, so later tests don't fail due to
      // this data.
      await promisify(service.clear, "ext-2");
    }
  }
);

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_WEBEXT_STORAGE,
  },
  async function test_storage_sync_bridged_engine() {
    const area = StorageSyncService.getInterface(Ci.mozIExtensionStorageArea);
    const engine = StorageSyncService.getInterface(Ci.mozIBridgedSyncEngine);

    info("Add some local items");
    await promisify(area.set, "ext-1", JSON.stringify({ a: "abc" }));
    await promisify(area.set, "ext-2", JSON.stringify({ b: "xyz" }));

    info("Start a sync");
    await promisify(engine.syncStarted);

    info("Store some incoming synced items");
    let incomingEnvelopesAsJSON = [
      {
        id: "guidAAA",
        modified: 0.1,
        cleartext: JSON.stringify({
          id: "guidAAA",
          extId: "ext-2",
          data: JSON.stringify({
            c: 1234,
          }),
        }),
      },
      {
        id: "guidBBB",
        modified: 0.1,
        cleartext: JSON.stringify({
          id: "guidBBB",
          extId: "ext-3",
          data: JSON.stringify({
            d: "new! ✨",
          }),
        }),
      },
    ].map(e => JSON.stringify(e));
    await promisify(area.storeIncoming, incomingEnvelopesAsJSON);

    info("Merge");
    // Three levels of JSON wrapping: each outgoing envelope, the cleartext in
    // each envelope, and the extension storage data in each cleartext.
    // TODO: Should we reduce to 2? Extension storage data could be a map...
    let { value: outgoingEnvelopesAsJSON } = await promisify(area.apply);
    let outgoingEnvelopes = outgoingEnvelopesAsJSON.map(json =>
      JSON.parse(json)
    );
    let parsedCleartexts = outgoingEnvelopes.map(e => JSON.parse(e.cleartext));
    let parsedData = parsedCleartexts.map(c => JSON.parse(c.data));

    // ext-1 doesn't exist remotely yet, so the Rust sync layer will generate
    // a GUID for it. We don't know what it is, so we find it by the extension
    // ID.
    let ext1Index = parsedCleartexts.findIndex(c => c.extId == "ext-1");
    greater(ext1Index, -1, "Should find envelope for ext-1");
    let ext1Guid = outgoingEnvelopes[ext1Index].id;

    // ext-2 has a remote GUID that we set in the test above.
    let ext2Index = outgoingEnvelopes.findIndex(c => c.id == "guidAAA");
    greater(ext2Index, -1, "Should find envelope for ext-2");

    equal(outgoingEnvelopes.length, 2, "Should upload ext-1 and ext-2");
    equal(
      ext1Guid,
      parsedCleartexts[ext1Index].id,
      "ext-1 ID in envelope should match cleartext"
    );
    deepEqual(
      parsedData[ext1Index],
      {
        a: "abc",
      },
      "Should upload new data for ext-1"
    );
    equal(
      outgoingEnvelopes[ext2Index].id,
      parsedCleartexts[ext2Index].id,
      "ext-2 ID in envelope should match cleartext"
    );
    deepEqual(
      parsedData[ext2Index],
      {
        b: "xyz",
        c: 1234,
      },
      "Should merge local and remote data for ext-2"
    );

    info("Mark all extensions as uploaded");
    await promisify(engine.setUploaded, 0, [ext1Guid, "guidAAA"]);

    info("Finish sync");
    await promisify(engine.syncFinished);

    // Try fetching values for the remote-only extension we just synced.
    let { value: ext3Value } = await promisify(area.get, "ext-3", "null");
    deepEqual(
      ext3Value,
      {
        d: "new! ✨",
      },
      "Should return new keys for ext-3"
    );

    info("Try applying a second time");
    let secondApply = await promisify(area.apply);
    deepEqual(
      secondApply.value,
      {},
      "Shouldn't merge anything on second apply"
    );

    info("Wipe all items");
    await promisify(engine.wipe);

    for (let extId of ["ext-1", "ext-2", "ext-3"]) {
      // `get` always returns an object, even if there are no keys for the
      // extension ID.
      let { value } = await promisify(area.get, extId, "null");
      deepEqual(value, {}, `Wipe should remove all values for ${extId}`);
    }
  }
);
