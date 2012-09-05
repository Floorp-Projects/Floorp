/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;

// names for cache devices
const kDiskDevice = "disk";
const kMemoryDevice = "memory";
const kOfflineDevice = "offline";
const kPrivate = "private";

const kCacheA = "cache-A";
const kCacheA2 = "cache-A2";
const kCacheB = "cache-B";
const kCacheC = "cache-C";
const kTestContent = "test content";

// the name for our cache session
const kPrivateBrowsing = "PrivateBrowsing";

function check_devices_available(devices) {
  var cs = get_cache_service();
  var found_devices = [];

  var visitor = {
    visitDevice: function (deviceID, deviceInfo) {
      found_devices.push(deviceID);
      return false;
    },
    visitEntry: function (deviceID, entryInfo) {
      do_throw("nsICacheVisitor.visitEntry should not be called " +
        "when checking the availability of devices");
    }
  };

  // get the list of active devices
  cs.visitEntries(visitor);

  // see if any of the required devices was missing
  if (devices.sort().toString() != found_devices.sort().toString()) {
    do_throw("Expected to find these devices: \"" + devices.sort().toString() +
      "\", but found these instead: \"" + found_devices.sort().toString() + "\"");
  }

  // see if any extra devices have been found
  if (found_devices.length > devices.length) {
    do_throw("Expected to find these devices: [" + devices.join(", ") +
      "], but instead got: [" + found_devices.join(", ") + "]");
  }
}

function make_input_stream_scriptable(input) {
  var wrapper = Cc["@mozilla.org/scriptableinputstream;1"].
                createInstance(Ci.nsIScriptableInputStream);
  wrapper.init(input);
  return wrapper;
}

const entries = [
// key       content       device          should exist after leaving PB
  [kCacheA,  kTestContent, kMemoryDevice,  true],
  [kCacheA2, kTestContent, kPrivate,       false],
  [kCacheB,  kTestContent, kDiskDevice,    true],
  [kCacheC,  kTestContent, kOfflineDevice, true]
]

function get_storage_policy(device)
{
  switch (device) {
    case kDiskDevice:
      return Ci.nsICache.STORE_ON_DISK;
    case kOfflineDevice:
      return Ci.nsICache.STORE_OFFLINE;
    case kMemoryDevice:
    case kPrivate:
      return Ci.nsICache.STORE_IN_MEMORY;
  }
  do_throw("unknown device");
}

var store_idx;
var store_cb = null;
function store_entries(cb)
{
  if (cb) {
    store_cb = cb;
    store_idx = 0;
  }

  if (store_idx == entries.length) {
    do_execute_soon(store_cb);
    return;
  }

  var cache = get_cache_service();
  var session = cache.createSession(kPrivateBrowsing,
                                    get_storage_policy(entries[store_idx][2]),
                                    Ci.nsICache.STREAM_BASED);
  if (entries[store_idx][2] == kPrivate) {
    session.isPrivate = true;
  }

  session.asyncOpenCacheEntry(entries[store_idx][0],
                              Ci.nsICache.ACCESS_WRITE,
                              store_data);
}

var store_data = {
  onCacheEntryAvailable: function oCEA(entry, access, status) {
    do_check_eq(status, Cr.NS_OK);
    var os = entry.openOutputStream(0);

    var written = os.write(entries[store_idx][1], entries[store_idx][1].length);
    if (written != entries[store_idx][1].length) {
      do_throw("os.write has not written all data!\n" +
               "  Expected: " + entries[store_idx][1].length  + "\n" +
               "  Actual: " + written + "\n");
    }
    os.close();
    entry.close();
    store_idx++;
    do_execute_soon(store_entries);
  }
};

var check_idx;
var check_cb = null;
var check_pb_exited;
function check_entries(cb, pbExited)
{
  if (cb) {
    check_cb = cb;
    check_idx = 0;
    check_pb_exited = pbExited;
  }

  if (check_idx == entries.length) {
    do_execute_soon(check_cb);
    return;
  }

  var cache = get_cache_service();
  var session = cache.createSession(kPrivateBrowsing,
                                    get_storage_policy(entries[check_idx][2]),
                                    Ci.nsICache.STREAM_BASED);
  if (entries[check_idx][2] == kPrivate) {
    session.isPrivate = true;
  }

  session.asyncOpenCacheEntry(entries[check_idx][0],
                              Ci.nsICache.ACCESS_READ,
                              check_data);
}

var check_data = {
  onCacheEntryAvailable: function oCEA(entry, access, status) {
    if (!check_pb_exited || entries[check_idx][3]) {
      do_check_eq(status, Cr.NS_OK);
      var is = make_input_stream_scriptable(entry.openInputStream(0));
      var read = is.read(is.available());
      is.close();
      entry.close();
      do_check_eq(read, entries[check_idx][1]);
    } else {
      do_check_eq(status, Cr.NS_ERROR_CACHE_KEY_NOT_FOUND);
    }

    check_idx++;
    do_execute_soon(check_entries);
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
  // Make sure all three cache devices are available initially
  check_devices_available([kMemoryDevice, kDiskDevice, kOfflineDevice]);

  // Check if cache-A, cache-A2, cache-B and cache-C are available
  check_entries(run_test3, false);
}

function run_test3() {
  // Simulate all private browsing instances being closed
  var obsvc = Cc["@mozilla.org/observer-service;1"].
    getService(Ci.nsIObserverService);
  obsvc.notifyObservers(null, "last-pb-context-exited", null);

  // Make sure all three cache devices are still available
  check_devices_available([kMemoryDevice, kDiskDevice, kOfflineDevice]);

  // Make sure the memory device is not empty
  do_check_eq(get_device_entry_count(kMemoryDevice), 1);

  // Check if cache-A is gone, and cache-B and cache-C are still available
  check_entries(do_test_finished, true);
}
