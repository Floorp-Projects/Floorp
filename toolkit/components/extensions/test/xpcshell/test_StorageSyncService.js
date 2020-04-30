/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

function promisify(func, ...params) {
  return new Promise((resolve, reject) => {
    let changes = [];
    func(...params, {
      QueryInterface: ChromeUtils.generateQI([
        Ci.mozIExtensionStorageListener,
        Ci.mozIExtensionStorageCallback,
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

add_task(
  {
    skip_if: () => !AppConstants.MOZ_NEW_WEBEXT_STORAGE,
  },
  async function test_storage_sync_service() {
    // So that we can write to the profile directory.
    do_get_profile();

    const service = Cc["@mozilla.org/extensions/storage/sync;1"]
      .getService(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.mozIExtensionStorageArea);
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
  }
);
