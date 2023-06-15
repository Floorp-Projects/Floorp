/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

do_get_profile(); // must be done before instantiating nsIDataStorageManager

let dataStorageManager = Cc[
  "@mozilla.org/security/datastoragemanager;1"
].getService(Ci.nsIDataStorageManager);
let dataStorage = dataStorageManager.get(
  Ci.nsIDataStorageManager.ClientAuthRememberList
);

add_task(function test_data_storage() {
  // Test putting a simple key/value pair.
  dataStorage.put("test", "value", Ci.nsIDataStorage.Persistent);
  Assert.equal(dataStorage.get("test", Ci.nsIDataStorage.Persistent), "value");

  // Test that getting a value with the same key but of a different type throws.
  Assert.throws(
    () => dataStorage.get("test", Ci.nsIDataStorage.Temporary),
    /NS_ERROR_NOT_AVAILABLE/,
    "getting a value of a type that hasn't been set yet should throw"
  );
  Assert.throws(
    () => dataStorage.get("test", Ci.nsIDataStorage.Private),
    /NS_ERROR_NOT_AVAILABLE/,
    "getting a value of a type that hasn't been set yet should throw"
  );

  // Put with Temporary/Private data shouldn't affect Persistent data
  dataStorage.put("test", "temporary", Ci.nsIDataStorage.Temporary);
  Assert.equal(
    dataStorage.get("test", Ci.nsIDataStorage.Temporary),
    "temporary"
  );
  dataStorage.put("test", "private", Ci.nsIDataStorage.Private);
  Assert.equal(dataStorage.get("test", Ci.nsIDataStorage.Private), "private");
  Assert.equal(dataStorage.get("test", Ci.nsIDataStorage.Persistent), "value");

  // Put of a previously-present key overwrites it (if of the same type)
  dataStorage.put("test", "new", Ci.nsIDataStorage.Persistent);
  Assert.equal(dataStorage.get("test", Ci.nsIDataStorage.Persistent), "new");

  // Removal should work
  dataStorage.remove("test", Ci.nsIDataStorage.Persistent);
  Assert.throws(
    () => dataStorage.get("test", Ci.nsIDataStorage.Persistent),
    /NS_ERROR_NOT_AVAILABLE/,
    "getting a removed value should throw"
  );
  // But removing one type shouldn't affect the others
  Assert.equal(
    dataStorage.get("test", Ci.nsIDataStorage.Temporary),
    "temporary"
  );
  Assert.equal(dataStorage.get("test", Ci.nsIDataStorage.Private), "private");
  // Test removing the other types as well
  dataStorage.remove("test", Ci.nsIDataStorage.Temporary);
  dataStorage.remove("test", Ci.nsIDataStorage.Private);
  Assert.throws(
    () => dataStorage.get("test", Ci.nsIDataStorage.Temporary),
    /NS_ERROR_NOT_AVAILABLE/,
    "getting a removed value should throw"
  );
  Assert.throws(
    () => dataStorage.get("test", Ci.nsIDataStorage.Private),
    /NS_ERROR_NOT_AVAILABLE/,
    "getting a removed value should throw"
  );
});
