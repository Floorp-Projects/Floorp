/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const gKeyValueService =
  Cc["@mozilla.org/key-value-service;1"].getService(Ci.nsIKeyValueService);

const EXPORTED_SYMBOLS = [
  "KeyValueService",
];

function promisify(fn, ...args) {
  return new Promise((resolve, reject) => {
    fn({ resolve, reject }, ...args);
  });
}

/**
 * This module wraps the nsIKeyValue* interfaces in a Promise-based API.
 * To use it, import it, then call the KeyValueService.getOrCreate() method
 * with a database's path and (optionally) its name:
 *
 * ```
 *     ChromeUtils.import("resource://gre/modules/kvstore.jsm");
 *     let database = await KeyValueService.getOrCreate(path, name);
 * ```
 *
 * See the documentation in nsIKeyValue.idl for more information about the API
 * for key/value storage.
 */

class KeyValueService {
  static async getOrCreate(dir, name) {
    return new KeyValueDatabase(
      await promisify(gKeyValueService.getOrCreate, dir, name)
    );
  }
}

/**
 * A class that wraps an nsIKeyValueDatabase in a Promise-based API.
 *
 * This class isn't exported, so you can't instantiate it directly, but you
 * can retrieve an instance of this class via KeyValueService.getOrCreate():
 *
 * ```
 *     const database = await KeyValueService.getOrCreate(path, name);
 * ```
 *
 * You can then call its put(), get(), has(), and delete() methods to access
 * and manipulate key/value pairs:
 *
 * ```
 *     await database.put("foo", 1);
 *     await database.get("foo") === 1; // true
 *     await database.has("foo"); // true
 *     await database.delete("foo");
 *     await database.has("foo"); // false
 * ```
 *
 * You can also call writeMany() to put/delete multiple key/value pairs:
 *
 * ```
 *     await database.writeMany({
 *       key1: "value1",
 *       key2: "value2",
 *       key3: "value3",
 *       key4: null, // delete
 *     });
 * ```
 *
 * And you can call its enumerate() method to retrieve a KeyValueEnumerator,
 * which is described below.
 */
class KeyValueDatabase {
  constructor(database) {
    this.database = database;
  }

  put(key, value) {
    return promisify(this.database.put, key, value);
  }

  /**
   * Writes multiple key/value pairs to the database.
   *
   * Note:
   *   * Each write could be either put or delete.
   *   * Given multiple values with the same key, only the last value will be stored.
   *   * If the same key gets put and deleted for multiple times, the final state
   *     of that key is subject to the ordering of the put(s) and delete(s).
   *
   * @param pairs Pairs could be any of following types:
   *        * An Object, all its properties and the corresponding values will
   *          be used as key value pairs. A property with null or undefined indicating
   *          a deletion.
   *        * An Array or an iterable whose elements are key-value pairs. such as
   *          [["key1", "value1"], ["key2", "value2"]]. Use a pair with value null
   *          to delete a key-value pair, e.g. ["delete-key", null].
   *        * A Map. A key with null or undefined value indicating a deletion.
   * @return A promise that is fulfilled when all the key/value pairs are written
   *         to the database.
   */
  writeMany(pairs) {
    if (!pairs) {
      throw new Error("writeMany(): unexpected argument.");
    }

    let entries;

    if (pairs instanceof Map || pairs instanceof Array ||
        typeof(pairs[Symbol.iterator]) === "function") {
      try {
        // Let Map constructor validate the argument. Note that Map remembers
        // the original insertion order of the keys, which satisfies the ordering
        // premise of this function.
        const map = pairs instanceof Map ? pairs : new Map(pairs);
        entries = Array.from(map, ([key, value]) => ({key, value}));
      } catch (error) {
        throw new Error("writeMany(): unexpected argument.");
      }
    } else if (typeof(pairs) === "object") {
      entries = Array.from(Object.entries(pairs), ([key, value]) => ({key, value}));
    } else {
      throw new Error("writeMany(): unexpected argument.");
    }

    if (entries.length) {
      return promisify(this.database.writeMany, entries);
    }
    return Promise.resolve();
  }

  has(key) {
    return promisify(this.database.has, key);
  }

  get(key, defaultValue) {
    return promisify(this.database.get, key, defaultValue);
  }

  delete(key) {
    return promisify(this.database.delete, key);
  }

  clear() {
    return promisify(this.database.clear);
  }

  async enumerate(from_key, to_key) {
    return new KeyValueEnumerator(
      await promisify(this.database.enumerate, from_key, to_key)
    );
  }
}

/**
 * A class that wraps an nsIKeyValueEnumerator in a Promise-based API.
 *
 * This class isn't exported, so you can't instantiate it directly, but you
 * can retrieve an instance of this class by calling enumerate() on an instance
 * of KeyValueDatabase:
 *
 * ```
 *     const database = await KeyValueService.getOrCreate(path, name);
 *     const enumerator = await database.enumerate();
 * ```
 *
 * And then iterate pairs via its hasMoreElements() and getNext() methods:
 *
 * ```
 *     while (enumerator.hasMoreElements()) {
 *       const { key, value } = enumerator.getNext();
 *       …
 *     }
 * ```
 *
 * Or with a `for...of` statement:
 *
 * ```
 *     for (const { key, value } of enumerator) {
 *         …
 *     }
 * ```
 */
class KeyValueEnumerator {
  constructor(enumerator) {
    this.enumerator = enumerator;
  }

  hasMoreElements() {
    return this.enumerator.hasMoreElements();
  }

  getNext() {
    return this.enumerator.getNext();
  }

  * [Symbol.iterator]() {
    while (this.enumerator.hasMoreElements()) {
      yield (this.enumerator.getNext());
    }
  }
}
