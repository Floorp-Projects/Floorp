/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// names for cache devices
const kDiskDevice = "disk";
const kMemoryDevice = "memory";

const kCacheA = "http://cache/A";
const kCacheA2 = "http://cache/A2";
const kCacheB = "http://cache/B";
const kCacheC = "http://cache/C";
const kTestContent = "test content";

function make_input_stream_scriptable(input) {
  var wrapper = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
    Ci.nsIScriptableInputStream
  );
  wrapper.init(input);
  return wrapper;
}

const entries = [
  // key       content       device          should exist after leaving PB
  [kCacheA, kTestContent, kMemoryDevice, true],
  [kCacheA2, kTestContent, kDiskDevice, false],
  [kCacheB, kTestContent, kDiskDevice, true],
];

var store_idx;
var store_cb = null;

function store_entries(cb) {
  if (cb) {
    store_cb = cb;
    store_idx = 0;
  }

  if (store_idx == entries.length) {
    executeSoon(store_cb);
    return;
  }

  asyncOpenCacheEntry(
    entries[store_idx][0],
    entries[store_idx][2],
    Ci.nsICacheStorage.OPEN_TRUNCATE,
    Services.loadContextInfo.custom(false, {
      privateBrowsingId: entries[store_idx][3] ? 0 : 1,
    }),
    store_data
  );
}

var store_data = function(status, entry) {
  Assert.equal(status, Cr.NS_OK);
  var os = entry.openOutputStream(0, entries[store_idx][1].length);

  var written = os.write(entries[store_idx][1], entries[store_idx][1].length);
  if (written != entries[store_idx][1].length) {
    do_throw(
      "os.write has not written all data!\n" +
        "  Expected: " +
        entries[store_idx][1].length +
        "\n" +
        "  Actual: " +
        written +
        "\n"
    );
  }
  os.close();
  entry.close();
  store_idx++;
  executeSoon(store_entries);
};

var check_idx;
var check_cb = null;
var check_pb_exited;
function check_entries(cb, pbExited) {
  if (cb) {
    check_cb = cb;
    check_idx = 0;
    check_pb_exited = pbExited;
  }

  if (check_idx == entries.length) {
    executeSoon(check_cb);
    return;
  }

  asyncOpenCacheEntry(
    entries[check_idx][0],
    entries[check_idx][2],
    Ci.nsICacheStorage.OPEN_READONLY,
    Services.loadContextInfo.custom(false, {
      privateBrowsingId: entries[check_idx][3] ? 0 : 1,
    }),
    check_data
  );
}

var check_data = function(status, entry) {
  var cont = function() {
    check_idx++;
    executeSoon(check_entries);
  };

  if (!check_pb_exited || entries[check_idx][3]) {
    Assert.equal(status, Cr.NS_OK);
    var is = entry.openInputStream(0);
    pumpReadStream(is, function(read) {
      entry.close();
      Assert.equal(read, entries[check_idx][1]);
      cont();
    });
  } else {
    Assert.equal(status, Cr.NS_ERROR_CACHE_KEY_NOT_FOUND);
    cont();
  }
};

function run_test() {
  // Simulate a profile dir for xpcshell
  do_get_profile();

  // Start off with an empty cache
  evict_cache_entries();

  // Store cache-A, cache-A2, cache-B and cache-C
  store_entries(run_test2);

  do_test_pending();
}

function run_test2() {
  // Check if cache-A, cache-A2, cache-B and cache-C are available
  check_entries(run_test3, false);
}

function run_test3() {
  // Simulate all private browsing instances being closed
  var obsvc = Cc["@mozilla.org/observer-service;1"].getService(
    Ci.nsIObserverService
  );
  obsvc.notifyObservers(null, "last-pb-context-exited");

  // Make sure the memory device is not empty
  get_device_entry_count(kMemoryDevice, null, function(count) {
    Assert.equal(count, 1);
    // Check if cache-A is gone, and cache-B and cache-C are still available
    check_entries(do_test_finished, true);
  });
}
