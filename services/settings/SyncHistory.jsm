/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  KeyValueService: "resource://gre/modules/kvstore.jsm",
});

var EXPORTED_SYMBOLS = ["SyncHistory"];

/**
 * A helper to keep track of synchronization statuses.
 *
 * We rely on a different storage backend than for storing Remote Settings data,
 * because the eventual goal is to be able to detect `IndexedDB` issues and act
 * accordingly.
 */
class SyncHistory {
  // Internal reference to underlying rkv store.
  #store;

  /**
   * @param {String} source the synchronization source (eg. `"settings-sync"`)
   * @param {Object} options
   * @param {int} options.size Maximum number of entries per source.
   */
  constructor(source, { size } = { size: 100 }) {
    this.source = source;
    this.size = size;
  }

  /**
   * Store the synchronization status. The ETag is converted and stored as
   * a millisecond epoch timestamp.
   * The entries with the oldest timestamps will be deleted to maintain the
   * history size under the configured maximum.
   *
   * @param {String} etag the ETag value from the server (eg. `"1647961052593"`)
   * @param {String} status the synchronization status (eg. `"success"`)
   * @param {Object} infos optional additional information to keep track of
   */
  async store(etag, status, infos = {}) {
    const rkv = await this.#init();
    const timestamp = parseInt(etag.replace('"', ""), 10);
    if (Number.isNaN(timestamp)) {
      throw new Error(`Invalid ETag value ${etag}`);
    }
    const key = `v1-${this.source}\t${timestamp}`;
    const value = { timestamp, status, infos };
    await rkv.put(key, JSON.stringify(value));
    // Trim old entries.
    const allEntries = await this.list();
    for (let i = this.size; i < allEntries.length; i++) {
      let { timestamp } = allEntries[i];
      await rkv.delete(`v1-${this.source}\t${timestamp}`);
    }
  }

  /**
   * Retrieve the stored history entries for a certain source, sorted by
   * timestamp descending.
   *
   * @returns {Array<Object>} a list of objects
   */
  async list() {
    const rkv = await this.#init();
    const entries = [];
    // The "from" and "to" key parameters to nsIKeyValueStore.enumerate()
    // are inclusive and exclusive, respectively, and keys are tuples
    // of source and datetime joined by a tab (\t), which is character code 9;
    // so enumerating ["source", "source\n"), where the line feed (\n)
    // is character code 10, enumerates all pairs with the given source.
    for (const { value } of await rkv.enumerate(
      `v1-${this.source}`,
      `v1-${this.source}\n`
    )) {
      try {
        const stored = JSON.parse(value);
        entries.push({ ...stored, datetime: new Date(stored.timestamp) });
      } catch (e) {
        // Ignore malformed entries.
        Cu.reportError(e);
      }
    }
    // Sort entries by `timestamp` descending.
    entries.sort((a, b) => (a.timestamp > b.timestamp ? -1 : 1));
    return entries;
  }

  /**
   * Return the most recent entry.
   */
  async last() {
    // List is sorted from newer to older.
    return (await this.list())[0];
  }

  /**
   * Wipe out the **whole** store.
   */
  async clear() {
    const rkv = await this.#init();
    await rkv.clear();
  }

  /**
   * Initialize the rkv store in the user profile.
   *
   * @returns {Object} the underlying `KeyValueService` instance.
   */
  async #init() {
    if (!this.#store) {
      // Get and cache a handle to the kvstore.
      const dir = PathUtils.join(PathUtils.profileDir, "settings");
      await IOUtils.makeDirectory(dir);
      this.#store = await lazy.KeyValueService.getOrCreate(dir, "synchistory");
    }
    return this.#store;
  }
}
