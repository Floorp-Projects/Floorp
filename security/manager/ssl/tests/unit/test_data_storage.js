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
    () => dataStorage.get("test", Ci.nsIDataStorage.Private),
    /NS_ERROR_NOT_AVAILABLE/,
    "getting a value of a type that hasn't been set yet should throw"
  );

  // Put with Private data shouldn't affect Persistent data
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
  // But removing one type shouldn't affect the other
  Assert.equal(dataStorage.get("test", Ci.nsIDataStorage.Private), "private");
  // Test removing the other type as well
  dataStorage.remove("test", Ci.nsIDataStorage.Private);
  Assert.throws(
    () => dataStorage.get("test", Ci.nsIDataStorage.Private),
    /NS_ERROR_NOT_AVAILABLE/,
    "getting a removed value should throw"
  );

  // Saturate the storage tables (there is a maximum of 2048 entries for each
  // type of data).
  for (let i = 0; i < 2048; i++) {
    let padded = i.toString().padStart(4, "0");
    dataStorage.put(
      `key${padded}`,
      `value${padded}`,
      Ci.nsIDataStorage.Persistent
    );
    dataStorage.put(
      `key${padded}`,
      `value${padded}`,
      Ci.nsIDataStorage.Private
    );
  }
  // Ensure the data can be read back.
  for (let i = 0; i < 2048; i++) {
    let padded = i.toString().padStart(4, "0");
    let val = dataStorage.get(`key${padded}`, Ci.nsIDataStorage.Persistent);
    Assert.equal(val, `value${padded}`);
    val = dataStorage.get(`key${padded}`, Ci.nsIDataStorage.Private);
    Assert.equal(val, `value${padded}`);
  }
  // Remove each entry.
  for (let i = 0; i < 2048; i++) {
    let padded = i.toString().padStart(4, "0");
    dataStorage.remove(`key${padded}`, Ci.nsIDataStorage.Persistent);
    dataStorage.remove(`key${padded}`, Ci.nsIDataStorage.Private);
  }
  // Ensure the entries are not present.
  for (let i = 0; i < 2048; i++) {
    let padded = i.toString().padStart(4, "0");
    Assert.throws(
      () => dataStorage.get(`key${padded}`, Ci.nsIDataStorage.Persistent),
      /NS_ERROR_NOT_AVAILABLE/,
      "getting a removed value should throw"
    );
    Assert.throws(
      () => dataStorage.get(`key${padded}`, Ci.nsIDataStorage.Private),
      /NS_ERROR_NOT_AVAILABLE/,
      "getting a removed value should throw"
    );
  }
  // Add new entries.
  for (let i = 0; i < 2048; i++) {
    let padded = i.toString().padStart(5, "1");
    dataStorage.put(
      `key${padded}`,
      `value${padded}`,
      Ci.nsIDataStorage.Persistent
    );
    dataStorage.put(
      `key${padded}`,
      `value${padded}`,
      Ci.nsIDataStorage.Private
    );
  }
  // Ensure each new entry was added.
  for (let i = 0; i < 2048; i++) {
    let padded = i.toString().padStart(5, "1");
    let val = dataStorage.get(`key${padded}`, Ci.nsIDataStorage.Persistent);
    Assert.equal(val, `value${padded}`);
    val = dataStorage.get(`key${padded}`, Ci.nsIDataStorage.Private);
    Assert.equal(val, `value${padded}`);
  }
});
