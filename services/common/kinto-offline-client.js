/*
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * This file is generated from kinto.js - do not modify directly.
 */

this.EXPORTED_SYMBOLS = ["loadKinto"];

/*
 * Version 4.0.2 - ef8a96f
 */

(function(f){if(typeof exports==="object"&&typeof module!=="undefined"){module.exports=f()}else if(typeof define==="function"&&define.amd){define([],f)}else{var g;if(typeof window!=="undefined"){g=window}else if(typeof global!=="undefined"){g=global}else if(typeof self!=="undefined"){g=self}else{g=this}g.loadKinto = f()}})(function(){var define,module,exports;return (function e(t,n,r){function s(o,u){if(!n[o]){if(!t[o]){var a=typeof require=="function"&&require;if(!u&&a)return a(o,!0);if(i)return i(o,!0);var f=new Error("Cannot find module '"+o+"'");throw f.code="MODULE_NOT_FOUND",f}var l=n[o]={exports:{}};t[o][0].call(l.exports,function(e){var n=t[o][1][e];return s(n?n:e)},l,l.exports,e,t,n,r)}return n[o].exports}var i=typeof require=="function"&&require;for(var o=0;o<r.length;o++)s(r[o]);return s})({1:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _base = require("../src/adapters/base");

var _base2 = _interopRequireDefault(_base);

var _utils = require("../src/utils");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/*
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
Components.utils.import("resource://gre/modules/Sqlite.jsm");
Components.utils.import("resource://gre/modules/Task.jsm");

const SQLITE_PATH = "kinto.sqlite";

const statements = {
  "createCollectionData": `
    CREATE TABLE collection_data (
      collection_name TEXT,
      record_id TEXT,
      record TEXT
    );`,

  "createCollectionMetadata": `
    CREATE TABLE collection_metadata (
      collection_name TEXT PRIMARY KEY,
      last_modified INTEGER
    ) WITHOUT ROWID;`,

  "createCollectionDataRecordIdIndex": `
    CREATE UNIQUE INDEX unique_collection_record
      ON collection_data(collection_name, record_id);`,

  "clearData": `
    DELETE FROM collection_data
      WHERE collection_name = :collection_name;`,

  "createData": `
    INSERT INTO collection_data (collection_name, record_id, record)
      VALUES (:collection_name, :record_id, :record);`,

  "updateData": `
    INSERT OR REPLACE INTO collection_data (collection_name, record_id, record)
      VALUES (:collection_name, :record_id, :record);`,

  "deleteData": `
    DELETE FROM collection_data
      WHERE collection_name = :collection_name
      AND record_id = :record_id;`,

  "saveLastModified": `
    REPLACE INTO collection_metadata (collection_name, last_modified)
      VALUES (:collection_name, :last_modified);`,

  "getLastModified": `
    SELECT last_modified
      FROM collection_metadata
        WHERE collection_name = :collection_name;`,

  "getRecord": `
    SELECT record
      FROM collection_data
        WHERE collection_name = :collection_name
        AND record_id = :record_id;`,

  "listRecords": `
    SELECT record
      FROM collection_data
        WHERE collection_name = :collection_name;`,

  // N.B. we have to have a dynamic number of placeholders, which you
  // can't do without building your own statement. See `execute` for details
  "listRecordsById": `
    SELECT record_id, record
      FROM collection_data
        WHERE collection_name = ?
          AND record_id IN `,

  "importData": `
    REPLACE INTO collection_data (collection_name, record_id, record)
      VALUES (:collection_name, :record_id, :record);`

};

const createStatements = ["createCollectionData", "createCollectionMetadata", "createCollectionDataRecordIdIndex"];

const currentSchemaVersion = 1;

/**
 * Firefox adapter.
 *
 * Uses Sqlite as a backing store.
 *
 * Options:
 *  - path: the filename/path for the Sqlite database. If absent, use SQLITE_PATH.
 */
class FirefoxAdapter extends _base2.default {
  constructor(collection, options = {}) {
    super();
    this.collection = collection;
    this._connection = null;
    this._options = options;
  }

  _init(connection) {
    return Task.spawn(function* () {
      yield connection.executeTransaction(function* doSetup() {
        const schema = yield connection.getSchemaVersion();

        if (schema == 0) {

          for (let statementName of createStatements) {
            yield connection.execute(statements[statementName]);
          }

          yield connection.setSchemaVersion(currentSchemaVersion);
        } else if (schema != 1) {
          throw new Error("Unknown database schema: " + schema);
        }
      });
      return connection;
    });
  }

  _executeStatement(statement, params) {
    if (!this._connection) {
      throw new Error("The storage adapter is not open");
    }
    return this._connection.executeCached(statement, params);
  }

  open() {
    const self = this;
    return Task.spawn(function* () {
      const path = self._options.path || SQLITE_PATH;
      const opts = { path, sharedMemoryCache: false };
      if (!self._connection) {
        self._connection = yield Sqlite.openConnection(opts).then(self._init);
      }
    });
  }

  close() {
    if (this._connection) {
      const promise = this._connection.close();
      this._connection = null;
      return promise;
    }
    return Promise.resolve();
  }

  clear() {
    const params = { collection_name: this.collection };
    return this._executeStatement(statements.clearData, params);
  }

  execute(callback, options = { preload: [] }) {
    if (!this._connection) {
      throw new Error("The storage adapter is not open");
    }

    let result;
    const conn = this._connection;
    const collection = this.collection;

    return conn.executeTransaction(function* doExecuteTransaction() {
      // Preload specified records from DB, within transaction.
      const parameters = [collection, ...options.preload];
      const placeholders = options.preload.map(_ => "?");
      const stmt = statements.listRecordsById + "(" + placeholders.join(",") + ");";
      const rows = yield conn.execute(stmt, parameters);

      const preloaded = rows.reduce((acc, row) => {
        const record = JSON.parse(row.getResultByName("record"));
        acc[row.getResultByName("record_id")] = record;
        return acc;
      }, {});

      const proxy = transactionProxy(collection, preloaded);
      result = callback(proxy);

      for (let { statement, params } of proxy.operations) {
        yield conn.executeCached(statement, params);
      }
    }, conn.TRANSACTION_EXCLUSIVE).then(_ => result);
  }

  get(id) {
    const params = {
      collection_name: this.collection,
      record_id: id
    };
    return this._executeStatement(statements.getRecord, params).then(result => {
      if (result.length == 0) {
        return;
      }
      return JSON.parse(result[0].getResultByName("record"));
    });
  }

  list(params = { filters: {}, order: "" }) {
    const parameters = {
      collection_name: this.collection
    };
    return this._executeStatement(statements.listRecords, parameters).then(result => {
      const records = [];
      for (let k = 0; k < result.length; k++) {
        const row = result[k];
        records.push(JSON.parse(row.getResultByName("record")));
      }
      return records;
    }).then(results => {
      // The resulting list of records is filtered and sorted.
      // XXX: with some efforts, this could be implemented using SQL.
      return (0, _utils.reduceRecords)(params.filters, params.order, results);
    });
  }

  /**
   * Load a list of records into the local database.
   *
   * Note: The adapter is not in charge of filtering the already imported
   * records. This is done in `Collection#loadDump()`, as a common behaviour
   * between every adapters.
   *
   * @param  {Array} records.
   * @return {Array} imported records.
   */
  loadDump(records) {
    const connection = this._connection;
    const collection_name = this.collection;
    return Task.spawn(function* () {
      yield connection.executeTransaction(function* doImport() {
        for (let record of records) {
          const params = {
            collection_name: collection_name,
            record_id: record.id,
            record: JSON.stringify(record)
          };
          yield connection.execute(statements.importData, params);
        }
        const lastModified = Math.max(...records.map(record => record.last_modified));
        const params = {
          collection_name: collection_name
        };
        const previousLastModified = yield connection.execute(statements.getLastModified, params).then(result => {
          return result.length > 0 ? result[0].getResultByName("last_modified") : -1;
        });
        if (lastModified > previousLastModified) {
          const params = {
            collection_name: collection_name,
            last_modified: lastModified
          };
          yield connection.execute(statements.saveLastModified, params);
        }
      });
      return records;
    });
  }

  saveLastModified(lastModified) {
    const parsedLastModified = parseInt(lastModified, 10) || null;
    const params = {
      collection_name: this.collection,
      last_modified: parsedLastModified
    };
    return this._executeStatement(statements.saveLastModified, params).then(() => parsedLastModified);
  }

  getLastModified() {
    const params = {
      collection_name: this.collection
    };
    return this._executeStatement(statements.getLastModified, params).then(result => {
      if (result.length == 0) {
        return 0;
      }
      return result[0].getResultByName("last_modified");
    });
  }
}

exports.default = FirefoxAdapter;
function transactionProxy(collection, preloaded) {
  const _operations = [];

  return {
    get operations() {
      return _operations;
    },

    create(record) {
      _operations.push({
        statement: statements.createData,
        params: {
          collection_name: collection,
          record_id: record.id,
          record: JSON.stringify(record)
        }
      });
    },

    update(record) {
      _operations.push({
        statement: statements.updateData,
        params: {
          collection_name: collection,
          record_id: record.id,
          record: JSON.stringify(record)
        }
      });
    },

    delete(id) {
      _operations.push({
        statement: statements.deleteData,
        params: {
          collection_name: collection,
          record_id: id
        }
      });
    },

    get(id) {
      // Gecko JS engine outputs undesired warnings if id is not in preloaded.
      return id in preloaded ? preloaded[id] : undefined;
    }
  };
}

},{"../src/adapters/base":6,"../src/utils":8}],2:[function(require,module,exports){
/*
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

exports.default = loadKinto;

var _base = require("../src/adapters/base");

var _base2 = _interopRequireDefault(_base);

var _KintoBase = require("../src/KintoBase");

var _KintoBase2 = _interopRequireDefault(_KintoBase);

var _FirefoxStorage = require("./FirefoxStorage");

var _FirefoxStorage2 = _interopRequireDefault(_FirefoxStorage);

var _utils = require("../src/utils");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

function loadKinto() {
  const { EventEmitter } = Cu.import("resource://devtools/shared/event-emitter.js", {});
  const { generateUUID } = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

  // Use standalone kinto-http module landed in FFx.
  const { KintoHttpClient } = Cu.import("resource://services-common/kinto-http-client.js");

  Cu.import("resource://gre/modules/Timer.jsm");
  Cu.importGlobalProperties(['fetch']);

  // Leverage Gecko service to generate UUIDs.
  function makeIDSchema() {
    return {
      validate: _utils.RE_UUID.test.bind(_utils.RE_UUID),
      generate: function () {
        return generateUUID().toString().replace(/[{}]/g, "");
      }
    };
  }

  class KintoFX extends _KintoBase2.default {
    static get adapters() {
      return {
        BaseAdapter: _base2.default,
        FirefoxAdapter: _FirefoxStorage2.default
      };
    }

    constructor(options = {}) {
      const emitter = {};
      EventEmitter.decorate(emitter);

      const defaults = {
        events: emitter,
        ApiClass: KintoHttpClient,
        adapter: _FirefoxStorage2.default
      };

      const expandedOptions = _extends({}, defaults, options);
      super(expandedOptions);
    }

    collection(collName, options = {}) {
      const idSchema = makeIDSchema();
      const expandedOptions = _extends({ idSchema }, options);
      return super.collection(collName, expandedOptions);
    }
  }

  return KintoFX;
}

// This fixes compatibility with CommonJS required by browserify.
// See http://stackoverflow.com/questions/33505992/babel-6-changes-how-it-exports-default/33683495#33683495
if (typeof module === "object") {
  module.exports = loadKinto;
}

},{"../src/KintoBase":4,"../src/adapters/base":6,"../src/utils":8,"./FirefoxStorage":1}],3:[function(require,module,exports){

},{}],4:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

var _collection = require("./collection");

var _collection2 = _interopRequireDefault(_collection);

var _base = require("./adapters/base");

var _base2 = _interopRequireDefault(_base);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const DEFAULT_BUCKET_NAME = "default";
const DEFAULT_REMOTE = "http://localhost:8888/v1";

/**
 * KintoBase class.
 */
class KintoBase {
  /**
   * Provides a public access to the base adapter class. Users can create a
   * custom DB adapter by extending {@link BaseAdapter}.
   *
   * @type {Object}
   */
  static get adapters() {
    return {
      BaseAdapter: _base2.default
    };
  }

  /**
   * Synchronization strategies. Available strategies are:
   *
   * - `MANUAL`: Conflicts will be reported in a dedicated array.
   * - `SERVER_WINS`: Conflicts are resolved using remote data.
   * - `CLIENT_WINS`: Conflicts are resolved using local data.
   *
   * @type {Object}
   */
  static get syncStrategy() {
    return _collection2.default.strategy;
  }

  /**
   * Constructor.
   *
   * Options:
   * - `{String}`       `remote`         The server URL to use.
   * - `{String}`       `bucket`         The collection bucket name.
   * - `{EventEmitter}` `events`         Events handler.
   * - `{BaseAdapter}`  `adapter`        The base DB adapter class.
   * - `{Object}`       `adapterOptions` Options given to the adapter.
   * - `{String}`       `dbPrefix`       The DB name prefix.
   * - `{Object}`       `headers`        The HTTP headers to use.
   * - `{String}`       `requestMode`    The HTTP CORS mode to use.
   * - `{Number}`       `timeout`        The requests timeout in ms (default: `5000`).
   *
   * @param  {Object} options The options object.
   */
  constructor(options = {}) {
    const defaults = {
      bucket: DEFAULT_BUCKET_NAME,
      remote: DEFAULT_REMOTE
    };
    this._options = _extends({}, defaults, options);
    if (!this._options.adapter) {
      throw new Error("No adapter provided");
    }

    const { remote, events, headers, requestMode, timeout, ApiClass } = this._options;

    // public properties

    /**
     * The kinto HTTP client instance.
     * @type {KintoClient}
     */
    this.api = new ApiClass(remote, { events, headers, requestMode, timeout });
    /**
     * The event emitter instance.
     * @type {EventEmitter}
     */
    this.events = this._options.events;
  }

  /**
   * Creates a {@link Collection} instance. The second (optional) parameter
   * will set collection-level options like e.g. `remoteTransformers`.
   *
   * @param  {String} collName The collection name.
   * @param  {Object} options  May contain the following fields:
   *                           remoteTransformers: Array<RemoteTransformer>
   * @return {Collection}
   */
  collection(collName, options = {}) {
    if (!collName) {
      throw new Error("missing collection name");
    }

    const bucket = this._options.bucket;
    return new _collection2.default(bucket, collName, this.api, {
      events: this._options.events,
      adapter: this._options.adapter,
      adapterOptions: this._options.adapterOptions,
      dbPrefix: this._options.dbPrefix,
      idSchema: options.idSchema,
      remoteTransformers: options.remoteTransformers,
      hooks: options.hooks
    });
  }
}
exports.default = KintoBase;

},{"./adapters/base":6,"./collection":7}],5:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

var _base = require("./base.js");

var _base2 = _interopRequireDefault(_base);

var _utils = require("../utils");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const INDEXED_FIELDS = ["id", "_status", "last_modified"];

/**
 * IDB cursor handlers.
 * @type {Object}
 */
const cursorHandlers = {
  all(done) {
    const results = [];
    return function (event) {
      const cursor = event.target.result;
      if (cursor) {
        results.push(cursor.value);
        cursor.continue();
      } else {
        done(results);
      }
    };
  },

  in(values, done) {
    const sortedValues = [].slice.call(values).sort();
    const results = [];
    return function (event) {
      const cursor = event.target.result;
      if (!cursor) {
        done(results);
        return;
      }
      const { key, value } = cursor;
      let i = 0;
      while (key > sortedValues[i]) {
        // The cursor has passed beyond this key. Check next.
        ++i;
        if (i === sortedValues.length) {
          done(results); // There is no next. Stop searching.
          return;
        }
      }
      if (key === sortedValues[i]) {
        results.push(value);
        cursor.continue();
      } else {
        cursor.continue(sortedValues[i]);
      }
    };
  }
};

/**
 * Extract from filters definition the first indexed field. Since indexes were
 * created on single-columns, extracting a single one makes sense.
 *
 * @param  {Object} filters The filters object.
 * @return {String|undefined}
 */
function findIndexedField(filters) {
  const filteredFields = Object.keys(filters);
  const indexedFields = filteredFields.filter(field => {
    return INDEXED_FIELDS.indexOf(field) !== -1;
  });
  return indexedFields[0];
}

/**
 * Creates an IDB request and attach it the appropriate cursor event handler to
 * perform a list query.
 *
 * Multiple matching values are handled by passing an array.
 *
 * @param  {IDBStore}         store      The IDB store.
 * @param  {String|undefined} indexField The indexed field to query, if any.
 * @param  {Any}              value      The value to filter, if any.
 * @param  {Function}         done       The operation completion handler.
 * @return {IDBRequest}
 */
function createListRequest(store, indexField, value, done) {
  if (!indexField) {
    // Get all records.
    const request = store.openCursor();
    request.onsuccess = cursorHandlers.all(done);
    return request;
  }

  // WHERE IN equivalent clause
  if (Array.isArray(value)) {
    const request = store.index(indexField).openCursor();
    request.onsuccess = cursorHandlers.in(value, done);
    return request;
  }

  // WHERE field = value clause
  const request = store.index(indexField).openCursor(IDBKeyRange.only(value));
  request.onsuccess = cursorHandlers.all(done);
  return request;
}

/**
 * IndexedDB adapter.
 *
 * This adapter doesn't support any options.
 */
class IDB extends _base2.default {
  /**
   * Constructor.
   *
   * @param  {String} dbname The database nale.
   */
  constructor(dbname) {
    super();
    this._db = null;
    // public properties
    /**
     * The database name.
     * @type {String}
     */
    this.dbname = dbname;
  }

  _handleError(method) {
    return err => {
      const error = new Error(method + "() " + err.message);
      error.stack = err.stack;
      throw error;
    };
  }

  /**
   * Ensures a connection to the IndexedDB database has been opened.
   *
   * @override
   * @return {Promise}
   */
  open() {
    if (this._db) {
      return Promise.resolve(this);
    }
    return new Promise((resolve, reject) => {
      const request = indexedDB.open(this.dbname, 1);
      request.onupgradeneeded = event => {
        // DB object
        const db = event.target.result;
        // Main collection store
        const collStore = db.createObjectStore(this.dbname, {
          keyPath: "id"
        });
        // Primary key (generated by IdSchema, UUID by default)
        collStore.createIndex("id", "id", { unique: true });
        // Local record status ("synced", "created", "updated", "deleted")
        collStore.createIndex("_status", "_status");
        // Last modified field
        collStore.createIndex("last_modified", "last_modified");

        // Metadata store
        const metaStore = db.createObjectStore("__meta__", {
          keyPath: "name"
        });
        metaStore.createIndex("name", "name", { unique: true });
      };
      request.onerror = event => reject(event.target.error);
      request.onsuccess = event => {
        this._db = event.target.result;
        resolve(this);
      };
    });
  }

  /**
   * Closes current connection to the database.
   *
   * @override
   * @return {Promise}
   */
  close() {
    if (this._db) {
      this._db.close(); // indexedDB.close is synchronous
      this._db = null;
    }
    return super.close();
  }

  /**
   * Returns a transaction and a store objects for this collection.
   *
   * To determine if a transaction has completed successfully, we should rather
   * listen to the transaction’s complete event rather than the IDBObjectStore
   * request’s success event, because the transaction may still fail after the
   * success event fires.
   *
   * @param  {String}      mode  Transaction mode ("readwrite" or undefined)
   * @param  {String|null} name  Store name (defaults to coll name)
   * @return {Object}
   */
  prepare(mode = undefined, name = null) {
    const storeName = name || this.dbname;
    // On Safari, calling IDBDatabase.transaction with mode == undefined raises
    // a TypeError.
    const transaction = mode ? this._db.transaction([storeName], mode) : this._db.transaction([storeName]);
    const store = transaction.objectStore(storeName);
    return { transaction, store };
  }

  /**
   * Deletes every records in the current collection.
   *
   * @override
   * @return {Promise}
   */
  clear() {
    return this.open().then(() => {
      return new Promise((resolve, reject) => {
        const { transaction, store } = this.prepare("readwrite");
        store.clear();
        transaction.onerror = event => reject(new Error(event.target.error));
        transaction.oncomplete = () => resolve();
      });
    }).catch(this._handleError("clear"));
  }

  /**
   * Executes the set of synchronous CRUD operations described in the provided
   * callback within an IndexedDB transaction, for current db store.
   *
   * The callback will be provided an object exposing the following synchronous
   * CRUD operation methods: get, create, update, delete.
   *
   * Important note: because limitations in IndexedDB implementations, no
   * asynchronous code should be performed within the provided callback; the
   * promise will therefore be rejected if the callback returns a Promise.
   *
   * Options:
   * - {Array} preload: The list of record IDs to fetch and make available to
   *   the transaction object get() method (default: [])
   *
   * @example
   * const db = new IDB("example");
   * db.execute(transaction => {
   *   transaction.create({id: 1, title: "foo"});
   *   transaction.update({id: 2, title: "bar"});
   *   transaction.delete(3);
   *   return "foo";
   * })
   *   .catch(console.error.bind(console));
   *   .then(console.log.bind(console)); // => "foo"
   *
   * @param  {Function} callback The operation description callback.
   * @param  {Object}   options  The options object.
   * @return {Promise}
   */
  execute(callback, options = { preload: [] }) {
    // Transactions in IndexedDB are autocommited when a callback does not
    // perform any additional operation.
    // The way Promises are implemented in Firefox (see https://bugzilla.mozilla.org/show_bug.cgi?id=1193394)
    // prevents using within an opened transaction.
    // To avoid managing asynchronocity in the specified `callback`, we preload
    // a list of record in order to execute the `callback` synchronously.
    // See also:
    // - http://stackoverflow.com/a/28388805/330911
    // - http://stackoverflow.com/a/10405196
    // - https://jakearchibald.com/2015/tasks-microtasks-queues-and-schedules/
    return this.open().then(_ => new Promise((resolve, reject) => {
      // Start transaction.
      const { transaction, store } = this.prepare("readwrite");
      // Preload specified records using index.
      const ids = options.preload;
      store.index("id").openCursor().onsuccess = cursorHandlers.in(ids, records => {
        // Store obtained records by id.
        const preloaded = records.reduce((acc, record) => {
          acc[record.id] = record;
          return acc;
        }, {});
        // Expose a consistent API for every adapter instead of raw store methods.
        const proxy = transactionProxy(store, preloaded);
        // The callback is executed synchronously within the same transaction.
        let result;
        try {
          result = callback(proxy);
        } catch (e) {
          transaction.abort();
          reject(e);
        }
        if (result instanceof Promise) {
          // XXX: investigate how to provide documentation details in error.
          reject(new Error("execute() callback should not return a Promise."));
        }
        // XXX unsure if we should manually abort the transaction on error
        transaction.onerror = event => reject(new Error(event.target.error));
        transaction.oncomplete = event => resolve(result);
      });
    }));
  }

  /**
   * Retrieve a record by its primary key from the IndexedDB database.
   *
   * @override
   * @param  {String} id The record id.
   * @return {Promise}
   */
  get(id) {
    return this.open().then(() => {
      return new Promise((resolve, reject) => {
        const { transaction, store } = this.prepare();
        const request = store.get(id);
        transaction.onerror = event => reject(new Error(event.target.error));
        transaction.oncomplete = () => resolve(request.result);
      });
    }).catch(this._handleError("get"));
  }

  /**
   * Lists all records from the IndexedDB database.
   *
   * @override
   * @return {Promise}
   */
  list(params = { filters: {} }) {
    const { filters } = params;
    const indexField = findIndexedField(filters);
    const value = filters[indexField];
    return this.open().then(() => {
      return new Promise((resolve, reject) => {
        let results = [];
        const { transaction, store } = this.prepare();
        createListRequest(store, indexField, value, _results => {
          // we have received all requested records, parking them within
          // current scope
          results = _results;
        });
        transaction.onerror = event => reject(new Error(event.target.error));
        transaction.oncomplete = event => resolve(results);
      });
    }).then(results => {
      // The resulting list of records is filtered and sorted.
      const remainingFilters = _extends({}, filters);
      // If `indexField` was used already, don't filter again.
      delete remainingFilters[indexField];
      // XXX: with some efforts, this could be fully implemented using IDB API.
      return (0, _utils.reduceRecords)(remainingFilters, params.order, results);
    }).catch(this._handleError("list"));
  }

  /**
   * Store the lastModified value into metadata store.
   *
   * @override
   * @param  {Number}  lastModified
   * @return {Promise}
   */
  saveLastModified(lastModified) {
    const value = parseInt(lastModified, 10) || null;
    return this.open().then(() => {
      return new Promise((resolve, reject) => {
        const { transaction, store } = this.prepare("readwrite", "__meta__");
        store.put({ name: "lastModified", value: value });
        transaction.onerror = event => reject(event.target.error);
        transaction.oncomplete = event => resolve(value);
      });
    });
  }

  /**
   * Retrieve saved lastModified value.
   *
   * @override
   * @return {Promise}
   */
  getLastModified() {
    return this.open().then(() => {
      return new Promise((resolve, reject) => {
        const { transaction, store } = this.prepare(undefined, "__meta__");
        const request = store.get("lastModified");
        transaction.onerror = event => reject(event.target.error);
        transaction.oncomplete = event => {
          resolve(request.result && request.result.value || null);
        };
      });
    });
  }

  /**
   * Load a dump of records exported from a server.
   *
   * @abstract
   * @return {Promise}
   */
  loadDump(records) {
    return this.execute(transaction => {
      records.forEach(record => transaction.update(record));
    }).then(() => this.getLastModified()).then(previousLastModified => {
      const lastModified = Math.max(...records.map(record => record.last_modified));
      if (lastModified > previousLastModified) {
        return this.saveLastModified(lastModified);
      }
    }).then(() => records).catch(this._handleError("loadDump"));
  }
}

exports.default = IDB; /**
                        * IDB transaction proxy.
                        *
                        * @param  {IDBStore} store     The IndexedDB database store.
                        * @param  {Array}    preloaded The list of records to make available to
                        *                              get() (default: []).
                        * @return {Object}
                        */

function transactionProxy(store, preloaded = []) {
  return {
    create(record) {
      store.add(record);
    },

    update(record) {
      store.put(record);
    },

    delete(id) {
      store.delete(id);
    },

    get(id) {
      return preloaded[id];
    }
  };
}

},{"../utils":8,"./base.js":6}],6:[function(require,module,exports){
"use strict";

/**
 * Base db adapter.
 *
 * @abstract
 */

Object.defineProperty(exports, "__esModule", {
  value: true
});
class BaseAdapter {
  /**
   * Opens a connection to the database.
   *
   * @abstract
   * @return {Promise}
   */
  open() {
    return Promise.resolve();
  }

  /**
   * Closes current connection to the database.
   *
   * @abstract
   * @return {Promise}
   */
  close() {
    return Promise.resolve();
  }

  /**
   * Deletes every records present in the database.
   *
   * @abstract
   * @return {Promise}
   */
  clear() {
    throw new Error("Not Implemented.");
  }

  /**
   * Executes a batch of operations within a single transaction.
   *
   * @abstract
   * @param  {Function} callback The operation callback.
   * @param  {Object}   options  The options object.
   * @return {Promise}
   */
  execute(callback, options = { preload: [] }) {
    throw new Error("Not Implemented.");
  }

  /**
   * Retrieve a record by its primary key from the database.
   *
   * @abstract
   * @param  {String} id The record id.
   * @return {Promise}
   */
  get(id) {
    throw new Error("Not Implemented.");
  }

  /**
   * Lists all records from the database.
   *
   * @abstract
   * @param  {Object} params  The filters and order to apply to the results.
   * @return {Promise}
   */
  list(params = { filters: {}, order: "" }) {
    throw new Error("Not Implemented.");
  }

  /**
   * Store the lastModified value.
   *
   * @abstract
   * @param  {Number}  lastModified
   * @return {Promise}
   */
  saveLastModified(lastModified) {
    throw new Error("Not Implemented.");
  }

  /**
   * Retrieve saved lastModified value.
   *
   * @abstract
   * @return {Promise}
   */
  getLastModified() {
    throw new Error("Not Implemented.");
  }

  /**
   * Load a dump of records exported from a server.
   *
   * @abstract
   * @return {Promise}
   */
  loadDump(records) {
    throw new Error("Not Implemented.");
  }
}
exports.default = BaseAdapter;

},{}],7:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.CollectionTransaction = exports.SyncResultObject = undefined;

var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

exports.recordsEqual = recordsEqual;

var _base = require("./adapters/base");

var _base2 = _interopRequireDefault(_base);

var _IDB = require("./adapters/IDB");

var _IDB2 = _interopRequireDefault(_IDB);

var _utils = require("./utils");

var _uuid = require("uuid");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const RECORD_FIELDS_TO_CLEAN = ["_status"];
const AVAILABLE_HOOKS = ["incoming-changes"];

/**
 * Compare two records omitting local fields and synchronization
 * attributes (like _status and last_modified)
 * @param {Object} a    A record to compare.
 * @param {Object} b    A record to compare.
 * @return {boolean}
 */
function recordsEqual(a, b, localFields = []) {
  const fieldsToClean = RECORD_FIELDS_TO_CLEAN.concat(["last_modified"]).concat(localFields);
  const cleanLocal = r => (0, _utils.omitKeys)(r, fieldsToClean);
  return (0, _utils.deepEqual)(cleanLocal(a), cleanLocal(b));
}

/**
 * Synchronization result object.
 */
class SyncResultObject {
  /**
   * Object default values.
   * @type {Object}
   */
  static get defaults() {
    return {
      ok: true,
      lastModified: null,
      errors: [],
      created: [],
      updated: [],
      deleted: [],
      published: [],
      conflicts: [],
      skipped: [],
      resolved: []
    };
  }

  /**
   * Public constructor.
   */
  constructor() {
    /**
     * Current synchronization result status; becomes `false` when conflicts or
     * errors are registered.
     * @type {Boolean}
     */
    this.ok = true;
    Object.assign(this, SyncResultObject.defaults);
  }

  /**
   * Adds entries for a given result type.
   *
   * @param {String} type    The result type.
   * @param {Array}  entries The result entries.
   * @return {SyncResultObject}
   */
  add(type, entries) {
    if (!Array.isArray(this[type])) {
      return;
    }
    this[type] = this[type].concat(entries);
    this.ok = this.errors.length + this.conflicts.length === 0;
    return this;
  }

  /**
   * Reinitializes result entries for a given result type.
   *
   * @param  {String} type The result type.
   * @return {SyncResultObject}
   */
  reset(type) {
    this[type] = SyncResultObject.defaults[type];
    this.ok = this.errors.length + this.conflicts.length === 0;
    return this;
  }
}

exports.SyncResultObject = SyncResultObject;
function createUUIDSchema() {
  return {
    generate() {
      return (0, _uuid.v4)();
    },

    validate(id) {
      return (0, _utils.isUUID)(id);
    }
  };
}

function markStatus(record, status) {
  return _extends({}, record, { _status: status });
}

function markDeleted(record) {
  return markStatus(record, "deleted");
}

function markSynced(record) {
  return markStatus(record, "synced");
}

/**
 * Import a remote change into the local database.
 *
 * @param  {IDBTransactionProxy} transaction The transaction handler.
 * @param  {Object}              remote      The remote change object to import.
 * @param  {Array<String>}       localFields The list of fields that remain local.
 * @return {Object}
 */
function importChange(transaction, remote, localFields) {
  const local = transaction.get(remote.id);
  if (!local) {
    // Not found locally but remote change is marked as deleted; skip to
    // avoid recreation.
    if (remote.deleted) {
      return { type: "skipped", data: remote };
    }
    const synced = markSynced(remote);
    transaction.create(synced);
    return { type: "created", data: synced };
  }
  // Compare local and remote, ignoring local fields.
  const isIdentical = recordsEqual(local, remote, localFields);
  // Apply remote changes on local record.
  const synced = _extends({}, local, markSynced(remote));
  // Detect or ignore conflicts if record has also been modified locally.
  if (local._status !== "synced") {
    // Locally deleted, unsynced: scheduled for remote deletion.
    if (local._status === "deleted") {
      return { type: "skipped", data: local };
    }
    if (isIdentical) {
      // If records are identical, import anyway, so we bump the
      // local last_modified value from the server and set record
      // status to "synced".
      transaction.update(synced);
      return { type: "updated", data: { old: local, new: synced } };
    }
    if (local.last_modified !== undefined && local.last_modified === remote.last_modified) {
      // If our local version has the same last_modified as the remote
      // one, this represents an object that corresponds to a resolved
      // conflict. Our local version represents the final output, so
      // we keep that one. (No transaction operation to do.)
      // But if our last_modified is undefined,
      // that means we've created the same object locally as one on
      // the server, which *must* be a conflict.
      return { type: "void" };
    }
    return {
      type: "conflicts",
      data: { type: "incoming", local: local, remote: remote }
    };
  }
  // Local record was synced.
  if (remote.deleted) {
    transaction.delete(remote.id);
    return { type: "deleted", data: local };
  }
  // Import locally.
  transaction.update(synced);
  // if identical, simply exclude it from all SyncResultObject lists
  const type = isIdentical ? "void" : "updated";
  return { type, data: { old: local, new: synced } };
}

/**
 * Abstracts a collection of records stored in the local database, providing
 * CRUD operations and synchronization helpers.
 */
class Collection {
  /**
   * Constructor.
   *
   * Options:
   * - `{BaseAdapter} adapter` The DB adapter (default: `IDB`)
   * - `{String} dbPrefix`     The DB name prefix (default: `""`)
   *
   * @param  {String} bucket  The bucket identifier.
   * @param  {String} name    The collection name.
   * @param  {Api}    api     The Api instance.
   * @param  {Object} options The options object.
   */
  constructor(bucket, name, api, options = {}) {
    this._bucket = bucket;
    this._name = name;
    this._lastModified = null;

    const DBAdapter = options.adapter || _IDB2.default;
    if (!DBAdapter) {
      throw new Error("No adapter provided");
    }
    const dbPrefix = options.dbPrefix || "";
    const db = new DBAdapter(`${ dbPrefix }${ bucket }/${ name }`, options.adapterOptions);
    if (!(db instanceof _base2.default)) {
      throw new Error("Unsupported adapter.");
    }
    // public properties
    /**
     * The db adapter instance
     * @type {BaseAdapter}
     */
    this.db = db;
    /**
     * The Api instance.
     * @type {KintoClient}
     */
    this.api = api;
    /**
     * The event emitter instance.
     * @type {EventEmitter}
     */
    this.events = options.events;
    /**
     * The IdSchema instance.
     * @type {Object}
     */
    this.idSchema = this._validateIdSchema(options.idSchema);
    /**
     * The list of remote transformers.
     * @type {Array}
     */
    this.remoteTransformers = this._validateRemoteTransformers(options.remoteTransformers);
    /**
     * The list of hooks.
     * @type {Object}
     */
    this.hooks = this._validateHooks(options.hooks);
    /**
     * The list of fields names that will remain local.
     * @type {Array}
     */
    this.localFields = options.localFields || [];
  }

  /**
   * The collection name.
   * @type {String}
   */
  get name() {
    return this._name;
  }

  /**
   * The bucket name.
   * @type {String}
   */
  get bucket() {
    return this._bucket;
  }

  /**
   * The last modified timestamp.
   * @type {Number}
   */
  get lastModified() {
    return this._lastModified;
  }

  /**
   * Synchronization strategies. Available strategies are:
   *
   * - `MANUAL`: Conflicts will be reported in a dedicated array.
   * - `SERVER_WINS`: Conflicts are resolved using remote data.
   * - `CLIENT_WINS`: Conflicts are resolved using local data.
   *
   * @type {Object}
   */
  static get strategy() {
    return {
      CLIENT_WINS: "client_wins",
      SERVER_WINS: "server_wins",
      MANUAL: "manual"
    };
  }

  /**
   * Validates an idSchema.
   *
   * @param  {Object|undefined} idSchema
   * @return {Object}
   */
  _validateIdSchema(idSchema) {
    if (typeof idSchema === "undefined") {
      return createUUIDSchema();
    }
    if (typeof idSchema !== "object") {
      throw new Error("idSchema must be an object.");
    } else if (typeof idSchema.generate !== "function") {
      throw new Error("idSchema must provide a generate function.");
    } else if (typeof idSchema.validate !== "function") {
      throw new Error("idSchema must provide a validate function.");
    }
    return idSchema;
  }

  /**
   * Validates a list of remote transformers.
   *
   * @param  {Array|undefined} remoteTransformers
   * @return {Array}
   */
  _validateRemoteTransformers(remoteTransformers) {
    if (typeof remoteTransformers === "undefined") {
      return [];
    }
    if (!Array.isArray(remoteTransformers)) {
      throw new Error("remoteTransformers should be an array.");
    }
    return remoteTransformers.map(transformer => {
      if (typeof transformer !== "object") {
        throw new Error("A transformer must be an object.");
      } else if (typeof transformer.encode !== "function") {
        throw new Error("A transformer must provide an encode function.");
      } else if (typeof transformer.decode !== "function") {
        throw new Error("A transformer must provide a decode function.");
      }
      return transformer;
    });
  }

  /**
   * Validate the passed hook is correct.
   *
   * @param {Array|undefined} hook.
   * @return {Array}
   **/
  _validateHook(hook) {
    if (!Array.isArray(hook)) {
      throw new Error("A hook definition should be an array of functions.");
    }
    return hook.map(fn => {
      if (typeof fn !== "function") {
        throw new Error("A hook definition should be an array of functions.");
      }
      return fn;
    });
  }

  /**
   * Validates a list of hooks.
   *
   * @param  {Object|undefined} hooks
   * @return {Object}
   */
  _validateHooks(hooks) {
    if (typeof hooks === "undefined") {
      return {};
    }
    if (Array.isArray(hooks)) {
      throw new Error("hooks should be an object, not an array.");
    }
    if (typeof hooks !== "object") {
      throw new Error("hooks should be an object.");
    }

    const validatedHooks = {};

    for (let hook in hooks) {
      if (AVAILABLE_HOOKS.indexOf(hook) === -1) {
        throw new Error("The hook should be one of " + AVAILABLE_HOOKS.join(", "));
      }
      validatedHooks[hook] = this._validateHook(hooks[hook]);
    }
    return validatedHooks;
  }

  /**
   * Deletes every records in the current collection and marks the collection as
   * never synced.
   *
   * @return {Promise}
   */
  clear() {
    return this.db.clear().then(_ => this.db.saveLastModified(null)).then(_ => ({ data: [], permissions: {} }));
  }

  /**
   * Encodes a record.
   *
   * @param  {String} type   Either "remote" or "local".
   * @param  {Object} record The record object to encode.
   * @return {Promise}
   */
  _encodeRecord(type, record) {
    if (!this[`${ type }Transformers`].length) {
      return Promise.resolve(record);
    }
    return (0, _utils.waterfall)(this[`${ type }Transformers`].map(transformer => {
      return record => transformer.encode(record);
    }), record);
  }

  /**
   * Decodes a record.
   *
   * @param  {String} type   Either "remote" or "local".
   * @param  {Object} record The record object to decode.
   * @return {Promise}
   */
  _decodeRecord(type, record) {
    if (!this[`${ type }Transformers`].length) {
      return Promise.resolve(record);
    }
    return (0, _utils.waterfall)(this[`${ type }Transformers`].reverse().map(transformer => {
      return record => transformer.decode(record);
    }), record);
  }

  /**
   * Adds a record to the local database, asserting that none
   * already exist with this ID.
   *
   * Note: If either the `useRecordId` or `synced` options are true, then the
   * record object must contain the id field to be validated. If none of these
   * options are true, an id is generated using the current IdSchema; in this
   * case, the record passed must not have an id.
   *
   * Options:
   * - {Boolean} synced       Sets record status to "synced" (default: `false`).
   * - {Boolean} useRecordId  Forces the `id` field from the record to be used,
   *                          instead of one that is generated automatically
   *                          (default: `false`).
   *
   * @param  {Object} record
   * @param  {Object} options
   * @return {Promise}
   */
  create(record, options = { useRecordId: false, synced: false }) {
    // Validate the record and its ID (if any), even though this
    // validation is also done in the CollectionTransaction method,
    // because we need to pass the ID to preloadIds.
    const reject = msg => Promise.reject(new Error(msg));
    if (typeof record !== "object") {
      return reject("Record is not an object.");
    }
    if ((options.synced || options.useRecordId) && !record.hasOwnProperty("id")) {
      return reject("Missing required Id; synced and useRecordId options require one");
    }
    if (!options.synced && !options.useRecordId && record.hasOwnProperty("id")) {
      return reject("Extraneous Id; can't create a record having one set.");
    }
    const newRecord = _extends({}, record, {
      id: options.synced || options.useRecordId ? record.id : this.idSchema.generate(),
      _status: options.synced ? "synced" : "created"
    });
    if (!this.idSchema.validate(newRecord.id)) {
      return reject(`Invalid Id: ${ newRecord.id }`);
    }
    return this.execute(txn => txn.create(newRecord), { preloadIds: [newRecord.id] }).catch(err => {
      if (options.useRecordId) {
        throw new Error("Couldn't create record. It may have been virtually deleted.");
      }
      throw err;
    });
  }

  /**
   * Like {@link CollectionTransaction#update}, but wrapped in its own transaction.
   *
   * Options:
   * - {Boolean} synced: Sets record status to "synced" (default: false)
   * - {Boolean} patch:  Extends the existing record instead of overwriting it
   *   (default: false)
   *
   * @param  {Object} record
   * @param  {Object} options
   * @return {Promise}
   */
  update(record, options = { synced: false, patch: false }) {
    // Validate the record and its ID, even though this validation is
    // also done in the CollectionTransaction method, because we need
    // to pass the ID to preloadIds.
    if (typeof record !== "object") {
      return Promise.reject(new Error("Record is not an object."));
    }
    if (!record.hasOwnProperty("id")) {
      return Promise.reject(new Error("Cannot update a record missing id."));
    }
    if (!this.idSchema.validate(record.id)) {
      return Promise.reject(new Error(`Invalid Id: ${ record.id }`));
    }

    return this.execute(txn => txn.update(record, options), { preloadIds: [record.id] });
  }

  /**
   * Like {@link CollectionTransaction#upsert}, but wrapped in its own transaction.
   *
   * @param  {Object} record
   * @return {Promise}
   */
  upsert(record) {
    // Validate the record and its ID, even though this validation is
    // also done in the CollectionTransaction method, because we need
    // to pass the ID to preloadIds.
    if (typeof record !== "object") {
      return Promise.reject(new Error("Record is not an object."));
    }
    if (!record.hasOwnProperty("id")) {
      return Promise.reject(new Error("Cannot update a record missing id."));
    }
    if (!this.idSchema.validate(record.id)) {
      return Promise.reject(new Error(`Invalid Id: ${ record.id }`));
    }

    return this.execute(txn => txn.upsert(record), { preloadIds: [record.id] });
  }

  /**
   * Like {@link CollectionTransaction#get}, but wrapped in its own transaction.
   *
   * Options:
   * - {Boolean} includeDeleted: Include virtually deleted records.
   *
   * @param  {String} id
   * @param  {Object} options
   * @return {Promise}
   */
  get(id, options = { includeDeleted: false }) {
    return this.execute(txn => txn.get(id, options), { preloadIds: [id] });
  }

  /**
   * Like {@link CollectionTransaction#getAny}, but wrapped in its own transaction.
   *
   * @param  {String} id
   * @return {Promise}
   */
  getAny(id) {
    return this.execute(txn => txn.getAny(id), { preloadIds: [id] });
  }

  /**
   * Same as {@link Collection#delete}, but wrapped in its own transaction.
   *
   * Options:
   * - {Boolean} virtual: When set to `true`, doesn't actually delete the record,
   *   update its `_status` attribute to `deleted` instead (default: true)
   *
   * @param  {String} id       The record's Id.
   * @param  {Object} options  The options object.
   * @return {Promise}
   */
  delete(id, options = { virtual: true }) {
    return this.execute(transaction => {
      return transaction.delete(id, options);
    }, { preloadIds: [id] });
  }

  /**
   * The same as {@link CollectionTransaction#deleteAny}, but wrapped
   * in its own transaction.
   *
   * @param  {String} id       The record's Id.
   * @return {Promise}
   */
  deleteAny(id) {
    return this.execute(txn => txn.deleteAny(id), { preloadIds: [id] });
  }

  /**
   * Lists records from the local database.
   *
   * Params:
   * - {Object} filters Filter the results (default: `{}`).
   * - {String} order   The order to apply   (default: `-last_modified`).
   *
   * Options:
   * - {Boolean} includeDeleted: Include virtually deleted records.
   *
   * @param  {Object} params  The filters and order to apply to the results.
   * @param  {Object} options The options object.
   * @return {Promise}
   */
  list(params = {}, options = { includeDeleted: false }) {
    params = _extends({ order: "-last_modified", filters: {} }, params);
    return this.db.list(params).then(results => {
      let data = results;
      if (!options.includeDeleted) {
        data = results.filter(record => record._status !== "deleted");
      }
      return { data, permissions: {} };
    });
  }

  /**
   * Import changes into the local database.
   *
   * @param  {SyncResultObject} syncResultObject The sync result object.
   * @param  {Object}           changeObject     The change object.
   * @return {Promise}
   */
  importChanges(syncResultObject, changeObject) {
    return Promise.all(changeObject.changes.map(change => {
      return this._decodeRecord("remote", change);
    })).then(decodedChanges => {
      // No change, nothing to import.
      if (decodedChanges.length === 0) {
        return Promise.resolve(syncResultObject);
      }
      // Retrieve records matching change ids.
      return this.db.execute(transaction => {
        return decodedChanges.map(remote => {
          // Store remote change into local database.
          return importChange(transaction, remote, this.localFields);
        });
      }, { preload: decodedChanges.map(record => record.id) }).catch(err => {
        const data = {
          type: "incoming",
          message: err.message,
          stack: err.stack
        };
        // XXX one error of the whole transaction instead of per atomic op
        return [{ type: "errors", data }];
      }).then(imports => {
        for (let imported of imports) {
          if (imported.type !== "void") {
            syncResultObject.add(imported.type, imported.data);
          }
        }
        return syncResultObject;
      });
    }).then(syncResultObject => {
      syncResultObject.lastModified = changeObject.lastModified;
      // Don't persist lastModified value if any conflict or error occured
      if (!syncResultObject.ok) {
        return syncResultObject;
      }
      // No conflict occured, persist collection's lastModified value
      return this.db.saveLastModified(syncResultObject.lastModified).then(lastModified => {
        this._lastModified = lastModified;
        return syncResultObject;
      });
    });
  }

  /**
   * Execute a bunch of operations in a transaction.
   *
   * This transaction should be atomic -- either all of its operations
   * will succeed, or none will.
   *
   * The argument to this function is itself a function which will be
   * called with a {@link CollectionTransaction}. Collection methods
   * are available on this transaction, but instead of returning
   * promises, they are synchronous. execute() returns a Promise whose
   * value will be the return value of the provided function.
   *
   * Most operations will require access to the record itself, which
   * must be preloaded by passing its ID in the preloadIds option.
   *
   * Options:
   * - {Array} preloadIds: list of IDs to fetch at the beginning of
   *   the transaction
   *
   * @return {Promise} Resolves with the result of the given function
   *    when the transaction commits.
   */
  execute(doOperations, { preloadIds = [] } = {}) {
    for (let id of preloadIds) {
      if (!this.idSchema.validate(id)) {
        return Promise.reject(Error(`Invalid Id: ${ id }`));
      }
    }

    return this.db.execute(transaction => {
      const txn = new CollectionTransaction(this, transaction);
      const result = doOperations(txn);
      txn.emitEvents();
      return result;
    }, { preload: preloadIds });
  }

  /**
   * Resets the local records as if they were never synced; existing records are
   * marked as newly created, deleted records are dropped.
   *
   * A next call to {@link Collection.sync} will thus republish the whole
   * content of the local collection to the server.
   *
   * @return {Promise} Resolves with the number of processed records.
   */
  resetSyncStatus() {
    let _count;
    return this.list({ filters: { _status: ["deleted", "synced"] }, order: "" }, { includeDeleted: true }).then(unsynced => {
      return this.db.execute(transaction => {
        _count = unsynced.data.length;
        unsynced.data.forEach(record => {
          if (record._status === "deleted") {
            // Garbage collect deleted records.
            transaction.delete(record.id);
          } else {
            // Records that were synced become «created».
            transaction.update(_extends({}, record, {
              last_modified: undefined,
              _status: "created"
            }));
          }
        });
      });
    }).then(() => this.db.saveLastModified(null)).then(() => _count);
  }

  /**
   * Returns an object containing two lists:
   *
   * - `toDelete`: unsynced deleted records we can safely delete;
   * - `toSync`: local updates to send to the server.
   *
   * @return {Promise}
   */
  gatherLocalChanges() {
    return Promise.all([this.list({ filters: { _status: ["created", "updated"] }, order: "" }), this.list({ filters: { _status: "deleted" }, order: "" }, { includeDeleted: true })]).then(([unsynced, deleted]) => {
      return Promise.all([Promise.all(unsynced.data.map(this._encodeRecord.bind(this, "remote"))), Promise.all(deleted.data.map(this._encodeRecord.bind(this, "remote")))]);
    }).then(([toSync, toDelete]) => ({ toSync, toDelete }));
  }

  /**
   * Fetch remote changes, import them to the local database, and handle
   * conflicts according to `options.strategy`. Then, updates the passed
   * {@link SyncResultObject} with import results.
   *
   * Options:
   * - {String} strategy: The selected sync strategy.
   *
   * @param  {KintoClient.Collection} client           Kinto client Collection instance.
   * @param  {SyncResultObject}       syncResultObject The sync result object.
   * @param  {Object}                 options
   * @return {Promise}
   */
  pullChanges(client, syncResultObject, options = {}) {
    if (!syncResultObject.ok) {
      return Promise.resolve(syncResultObject);
    }
    options = _extends({ strategy: Collection.strategy.MANUAL,
      lastModified: this.lastModified,
      headers: {}
    }, options);

    // Optionally ignore some records when pulling for changes.
    // (avoid redownloading our own changes on last step of #sync())
    let filters;
    if (options.exclude) {
      // Limit the list of excluded records to the first 50 records in order
      // to remain under de-facto URL size limit (~2000 chars).
      // http://stackoverflow.com/questions/417142/what-is-the-maximum-length-of-a-url-in-different-browsers/417184#417184
      const exclude_id = options.exclude.slice(0, 50).map(r => r.id).join(",");
      filters = { exclude_id };
    }
    // First fetch remote changes from the server
    return client.listRecords({
      // Since should be ETag (see https://github.com/Kinto/kinto.js/issues/356)
      since: options.lastModified ? `${ options.lastModified }` : undefined,
      headers: options.headers,
      filters
    }).then(({ data, last_modified }) => {
      // last_modified is the ETag header value (string).
      // For retro-compatibility with first kinto.js versions
      // parse it to integer.
      const unquoted = last_modified ? parseInt(last_modified, 10) : undefined;

      // Check if server was flushed.
      // This is relevant for the Kinto demo server
      // (and thus for many new comers).
      const localSynced = options.lastModified;
      const serverChanged = unquoted > options.lastModified;
      const emptyCollection = data.length === 0;
      if (!options.exclude && localSynced && serverChanged && emptyCollection) {
        throw Error("Server has been flushed.");
      }

      const payload = { lastModified: unquoted, changes: data };
      return this.applyHook("incoming-changes", payload);
    })
    // Reflect these changes locally
    .then(changes => this.importChanges(syncResultObject, changes))
    // Handle conflicts, if any
    .then(result => this._handleConflicts(result, options.strategy));
  }

  applyHook(hookName, payload) {
    if (typeof this.hooks[hookName] == "undefined") {
      return Promise.resolve(payload);
    }
    return (0, _utils.waterfall)(this.hooks[hookName].map(hook => {
      return record => {
        const result = hook(payload, this);
        const resultThenable = result && typeof result.then === "function";
        const resultChanges = result && result.hasOwnProperty("changes");
        if (!(resultThenable || resultChanges)) {
          throw new Error(`Invalid return value for hook: ${ JSON.stringify(result) } has no 'then()' or 'changes' properties`);
        }
        return result;
      };
    }), payload);
  }

  /**
   * Publish local changes to the remote server and updates the passed
   * {@link SyncResultObject} with publication results.
   *
   * @param  {KintoClient.Collection} client           Kinto client Collection instance.
   * @param  {SyncResultObject}       syncResultObject The sync result object.
   * @param  {Object}                 options          The options object.
   * @return {Promise}
   */
  pushChanges(client, syncResultObject, options = {}) {
    if (!syncResultObject.ok) {
      return Promise.resolve(syncResultObject);
    }
    const safe = !options.strategy || options.strategy !== Collection.CLIENT_WINS;
    let synced;

    // Fetch local changes
    return this.gatherLocalChanges().then(({ toDelete, toSync }) => {
      // Send batch update requests
      return client.batch(batch => {
        toDelete.forEach(r => {
          // never published locally deleted records should not be pusblished
          if (r.last_modified) {
            batch.deleteRecord(r);
          }
        });
        toSync.forEach(r => {
          // Clean local fields (like _status) before sending to server.
          const published = this.cleanLocalFields(r);
          if (r._status === "created") {
            batch.createRecord(published);
          } else {
            batch.updateRecord(published);
          }
        });
      }, { headers: options.headers, safe, aggregate: true });
    })
    // Update published local records
    .then(batchResult => {
      synced = batchResult;
      // Merge outgoing errors into sync result object
      syncResultObject.add("errors", synced.errors.map(error => {
        error.type = "outgoing";
        return error;
      }));

      // The result of a batch returns data and permissions.
      // XXX: permissions are ignored currently.
      return Promise.all(synced.conflicts.map(({ type, local, remote }) => {
        // Note: we ensure that local data are actually available, as they may
        // be missing in the case of a published deletion.
        const safeLocal = local && local.data || {};
        return this._decodeRecord("remote", safeLocal).then(realLocal => {
          return this._decodeRecord("remote", remote).then(realRemote => {
            return { type, local: realLocal, remote: realRemote };
          });
        });
      }));
    }).then(conflicts => {
      // Merge outgoing conflicts into sync result object
      syncResultObject.add("conflicts", conflicts);

      // Reflect publication results locally using the response from
      // the batch request.
      // For created and updated records, the last_modified coming from server
      // will be stored locally.
      const published = synced.published.map(c => c.data);
      const skipped = synced.skipped.map(c => c.data);

      // Records that must be deleted are either deletions that were pushed
      // to server (published) or deleted records that were never pushed (skipped).
      const missingRemotely = skipped.map(r => {
        return _extends({}, r, { deleted: true });
      });
      const toApplyLocally = published.concat(missingRemotely);

      const toDeleteLocally = toApplyLocally.filter(r => r.deleted);
      const toUpdateLocally = toApplyLocally.filter(r => !r.deleted);
      // First, apply the decode transformers, if any
      return Promise.all(toUpdateLocally.map(record => {
        return this._decodeRecord("remote", record);
      }))
      // Process everything within a single transaction.
      .then(results => {
        return this.db.execute(transaction => {
          const updated = results.map(record => {
            const synced = markSynced(record);
            transaction.update(synced);
            return { data: synced };
          });
          const deleted = toDeleteLocally.map(record => {
            transaction.delete(record.id);
            // Amend result data with the deleted attribute set
            return { data: { id: record.id, deleted: true } };
          });
          return updated.concat(deleted);
        });
      }).then(published => {
        syncResultObject.add("published", published.map(res => res.data));
        return syncResultObject;
      });
    })
    // Handle conflicts, if any
    .then(result => this._handleConflicts(result, options.strategy)).then(result => {
      const resolvedUnsynced = result.resolved.filter(record => record._status !== "synced");
      // No resolved conflict to reflect anywhere
      if (resolvedUnsynced.length === 0 || options.resolved) {
        return result;
      } else if (options.strategy === Collection.strategy.CLIENT_WINS && !options.resolved) {
        // We need to push local versions of the records to the server
        return this.pushChanges(client, result, _extends({}, options, { resolved: true }));
      } else if (options.strategy === Collection.strategy.SERVER_WINS) {
        // If records have been automatically resolved according to strategy and
        // are in non-synced status, mark them as synced.
        return this.db.execute(transaction => {
          resolvedUnsynced.forEach(record => {
            transaction.update(markSynced(record));
          });
          return result;
        });
      }
    });
  }

  /**
   * Return a copy of the specified record without the local fields.
   *
   * @param  {Object} record  A record with potential local fields.
   * @return {Object}
   */
  cleanLocalFields(record) {
    const localKeys = RECORD_FIELDS_TO_CLEAN.concat(this.localFields);
    return (0, _utils.omitKeys)(record, localKeys);
  }

  /**
   * Resolves a conflict, updating local record according to proposed
   * resolution — keeping remote record `last_modified` value as a reference for
   * further batch sending.
   *
   * @param  {Object} conflict   The conflict object.
   * @param  {Object} resolution The proposed record.
   * @return {Promise}
   */
  resolve(conflict, resolution) {
    return this.db.execute(transaction => {
      const updated = this._resolveRaw(conflict, resolution);
      transaction.update(updated);
      return { data: updated, permissions: {} };
    });
  }

  /**
   * @private
   */
  _resolveRaw(conflict, resolution) {
    const resolved = _extends({}, resolution, {
      // Ensure local record has the latest authoritative timestamp
      last_modified: conflict.remote.last_modified
    });
    // If the resolution object is strictly equal to the
    // remote record, then we can mark it as synced locally.
    // Otherwise, mark it as updated (so that the resolution is pushed).
    const synced = (0, _utils.deepEqual)(resolved, conflict.remote);
    return markStatus(resolved, synced ? "synced" : "updated");
  }

  /**
   * Handles synchronization conflicts according to specified strategy.
   *
   * @param  {SyncResultObject} result    The sync result object.
   * @param  {String}           strategy  The {@link Collection.strategy}.
   * @return {Promise}
   */
  _handleConflicts(result, strategy = Collection.strategy.MANUAL) {
    if (strategy === Collection.strategy.MANUAL || result.conflicts.length === 0) {
      return Promise.resolve(result);
    }
    return this.db.execute(transaction => {
      return result.conflicts.map(conflict => {
        const resolution = strategy === Collection.strategy.CLIENT_WINS ? conflict.local : conflict.remote;
        const updated = this._resolveRaw(conflict, resolution);
        transaction.update(updated);
        return updated;
      });
    }).then(imports => {
      return result.reset("conflicts").add("resolved", imports);
    });
  }

  /**
   * Synchronize remote and local data. The promise will resolve with a
   * {@link SyncResultObject}, though will reject:
   *
   * - if the server is currently backed off;
   * - if the server has been detected flushed.
   *
   * Options:
   * - {Object} headers: HTTP headers to attach to outgoing requests.
   * - {Collection.strategy} strategy: See {@link Collection.strategy}.
   * - {Boolean} ignoreBackoff: Force synchronization even if server is currently
   *   backed off.
   * - {String} bucket: The remove bucket id to use (default: null)
   * - {String} collection: The remove collection id to use (default: null)
   * - {String} remote The remote Kinto server endpoint to use (default: null).
   *
   * @param  {Object} options Options.
   * @return {Promise}
   * @throws {Error} If an invalid remote option is passed.
   */
  sync(options = {
    strategy: Collection.strategy.MANUAL,
    headers: {},
    ignoreBackoff: false,
    bucket: null,
    collection: null,
    remote: null
  }) {
    const previousRemote = this.api.remote;
    if (options.remote) {
      // Note: setting the remote ensures it's valid, throws when invalid.
      this.api.remote = options.remote;
    }
    if (!options.ignoreBackoff && this.api.backoff > 0) {
      const seconds = Math.ceil(this.api.backoff / 1000);
      return Promise.reject(new Error(`Server is asking clients to back off; retry in ${ seconds }s or use the ignoreBackoff option.`));
    }

    const client = this.api.bucket(options.bucket || this.bucket).collection(options.collection || this.name);
    const result = new SyncResultObject();
    const syncPromise = this.db.getLastModified().then(lastModified => this._lastModified = lastModified).then(_ => this.pullChanges(client, result, options)).then(result => this.pushChanges(client, result, options)).then(result => {
      // Avoid performing a last pull if nothing has been published.
      if (result.published.length === 0) {
        return result;
      }
      // Avoid redownloading our own changes during the last pull.
      const pullOpts = _extends({}, options, { exclude: result.published });
      return this.pullChanges(client, result, pullOpts);
    });
    // Ensure API default remote is reverted if a custom one's been used
    return (0, _utils.pFinally)(syncPromise, () => this.api.remote = previousRemote);
  }

  /**
   * Load a list of records already synced with the remote server.
   *
   * The local records which are unsynced or whose timestamp is either missing
   * or superior to those being loaded will be ignored.
   *
   * @param  {Array} records The previously exported list of records to load.
   * @return {Promise} with the effectively imported records.
   */
  loadDump(records) {
    const reject = msg => Promise.reject(new Error(msg));
    if (!Array.isArray(records)) {
      return reject("Records is not an array.");
    }

    for (let record of records) {
      if (!record.hasOwnProperty("id") || !this.idSchema.validate(record.id)) {
        return reject("Record has invalid ID: " + JSON.stringify(record));
      }

      if (!record.last_modified) {
        return reject("Record has no last_modified value: " + JSON.stringify(record));
      }
    }

    // Fetch all existing records from local database,
    // and skip those who are newer or not marked as synced.

    // XXX filter by status / ids in records

    return this.list({}, { includeDeleted: true }).then(res => {
      return res.data.reduce((acc, record) => {
        acc[record.id] = record;
        return acc;
      }, {});
    }).then(existingById => {
      return records.filter(record => {
        const localRecord = existingById[record.id];
        const shouldKeep =
        // No local record with this id.
        localRecord === undefined ||
        // Or local record is synced
        localRecord._status === "synced" &&
        // And was synced from server
        localRecord.last_modified !== undefined &&
        // And is older than imported one.
        record.last_modified > localRecord.last_modified;
        return shouldKeep;
      });
    }).then(newRecords => newRecords.map(markSynced)).then(newRecords => this.db.loadDump(newRecords));
  }
}

exports.default = Collection; /**
                               * A Collection-oriented wrapper for an adapter's transaction.
                               *
                               * This defines the high-level functions available on a collection.
                               * The collection itself offers functions of the same name. These will
                               * perform just one operation in its own transaction.
                               */

class CollectionTransaction {
  constructor(collection, adapterTransaction) {
    this.collection = collection;
    this.adapterTransaction = adapterTransaction;

    this._events = [];
  }

  _queueEvent(action, payload) {
    this._events.push({ action, payload });
  }

  /**
   * Emit queued events, to be called once every transaction operations have
   * been executed successfully.
   */
  emitEvents() {
    for (let { action, payload } of this._events) {
      this.collection.events.emit(action, payload);
    }
    if (this._events.length > 0) {
      const targets = this._events.map(({ action, payload }) => _extends({ action }, payload));
      this.collection.events.emit("change", { targets });
    }
    this._events = [];
  }

  /**
   * Retrieve a record by its id from the local database, or
   * undefined if none exists.
   *
   * This will also return virtually deleted records.
   *
   * @param  {String} id
   * @return {Object}
   */
  getAny(id) {
    const record = this.adapterTransaction.get(id);
    return { data: record, permissions: {} };
  }

  /**
   * Retrieve a record by its id from the local database.
   *
   * Options:
   * - {Boolean} includeDeleted: Include virtually deleted records.
   *
   * @param  {String} id
   * @param  {Object} options
   * @return {Object}
   */
  get(id, options = { includeDeleted: false }) {
    const res = this.getAny(id);
    if (!res.data || !options.includeDeleted && res.data._status === "deleted") {
      throw new Error(`Record with id=${ id } not found.`);
    }

    return res;
  }

  /**
   * Deletes a record from the local database.
   *
   * Options:
   * - {Boolean} virtual: When set to `true`, doesn't actually delete the record,
   *   update its `_status` attribute to `deleted` instead (default: true)
   *
   * @param  {String} id       The record's Id.
   * @param  {Object} options  The options object.
   * @return {Object}
   */
  delete(id, options = { virtual: true }) {
    // Ensure the record actually exists.
    const existing = this.adapterTransaction.get(id);
    const alreadyDeleted = existing && existing._status == "deleted";
    if (!existing || alreadyDeleted && options.virtual) {
      throw new Error(`Record with id=${ id } not found.`);
    }
    // Virtual updates status.
    if (options.virtual) {
      this.adapterTransaction.update(markDeleted(existing));
    } else {
      // Delete for real.
      this.adapterTransaction.delete(id);
    }
    this._queueEvent("delete", { data: existing });
    return { data: existing, permissions: {} };
  }

  /**
   * Deletes a record from the local database, if any exists.
   * Otherwise, do nothing.
   *
   * @param  {String} id       The record's Id.
   * @return {Object}
   */
  deleteAny(id) {
    const existing = this.adapterTransaction.get(id);
    if (existing) {
      this.adapterTransaction.update(markDeleted(existing));
      this._queueEvent("delete", { data: existing });
    }
    return { data: _extends({ id }, existing), deleted: !!existing, permissions: {} };
  }

  /**
   * Adds a record to the local database, asserting that none
   * already exist with this ID.
   *
   * @param  {Object} record, which must contain an ID
   * @return {Object}
   */
  create(record) {
    if (typeof record !== "object") {
      throw new Error("Record is not an object.");
    }
    if (!record.hasOwnProperty("id")) {
      throw new Error("Cannot create a record missing id");
    }
    if (!this.collection.idSchema.validate(record.id)) {
      throw new Error(`Invalid Id: ${ record.id }`);
    }

    this.adapterTransaction.create(record);
    this._queueEvent("create", { data: record });
    return { data: record, permissions: {} };
  }

  /**
   * Updates a record from the local database.
   *
   * Options:
   * - {Boolean} synced: Sets record status to "synced" (default: false)
   * - {Boolean} patch:  Extends the existing record instead of overwriting it
   *   (default: false)
   *
   * @param  {Object} record
   * @param  {Object} options
   * @return {Object}
   */
  update(record, options = { synced: false, patch: false }) {
    if (typeof record !== "object") {
      throw new Error("Record is not an object.");
    }
    if (!record.hasOwnProperty("id")) {
      throw new Error("Cannot update a record missing id.");
    }
    if (!this.collection.idSchema.validate(record.id)) {
      throw new Error(`Invalid Id: ${ record.id }`);
    }

    const oldRecord = this.adapterTransaction.get(record.id);
    if (!oldRecord) {
      throw new Error(`Record with id=${ record.id } not found.`);
    }
    const newRecord = options.patch ? _extends({}, oldRecord, record) : record;
    const updated = this._updateRaw(oldRecord, newRecord, options);
    this.adapterTransaction.update(updated);
    this._queueEvent("update", { data: updated, oldRecord });
    return { data: updated, oldRecord, permissions: {} };
  }

  /**
   * Lower-level primitive for updating a record while respecting
   * _status and last_modified.
   *
   * @param  {Object} oldRecord: the record retrieved from the DB
   * @param  {Object} newRecord: the record to replace it with
   * @return {Object}
   */
  _updateRaw(oldRecord, newRecord, { synced = false } = {}) {
    const updated = _extends({}, newRecord);
    // Make sure to never loose the existing timestamp.
    if (oldRecord && oldRecord.last_modified && !updated.last_modified) {
      updated.last_modified = oldRecord.last_modified;
    }
    // If only local fields have changed, then keep record as synced.
    // If status is created, keep record as created.
    // If status is deleted, mark as updated.
    const isIdentical = oldRecord && recordsEqual(oldRecord, updated, this.localFields);
    const keepSynced = isIdentical && oldRecord._status == "synced";
    const neverSynced = !oldRecord || oldRecord && oldRecord._status == "created";
    const newStatus = keepSynced || synced ? "synced" : neverSynced ? "created" : "updated";
    return markStatus(updated, newStatus);
  }

  /**
   * Upsert a record into the local database.
   *
   * This record must have an ID.
   *
   * If a record with this ID already exists, it will be replaced.
   * Otherwise, this record will be inserted.
   *
   * @param  {Object} record
   * @return {Object}
   */
  upsert(record) {
    if (typeof record !== "object") {
      throw new Error("Record is not an object.");
    }
    if (!record.hasOwnProperty("id")) {
      throw new Error("Cannot update a record missing id.");
    }
    if (!this.collection.idSchema.validate(record.id)) {
      throw new Error(`Invalid Id: ${ record.id }`);
    }
    let oldRecord = this.adapterTransaction.get(record.id);
    const updated = this._updateRaw(oldRecord, record);
    this.adapterTransaction.update(updated);
    // Don't return deleted records -- pretend they are gone
    if (oldRecord && oldRecord._status == "deleted") {
      oldRecord = undefined;
    }
    if (oldRecord) {
      this._queueEvent("update", { data: updated, oldRecord });
    } else {
      this._queueEvent("create", { data: updated });
    }
    return { data: updated, oldRecord, permissions: {} };
  }
}
exports.CollectionTransaction = CollectionTransaction;

},{"./adapters/IDB":5,"./adapters/base":6,"./utils":8,"uuid":3}],8:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.sortObjects = sortObjects;
exports.filterObjects = filterObjects;
exports.reduceRecords = reduceRecords;
exports.isUUID = isUUID;
exports.waterfall = waterfall;
exports.pFinally = pFinally;
exports.deepEqual = deepEqual;
exports.omitKeys = omitKeys;
const RE_UUID = exports.RE_UUID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;

/**
 * Checks if a value is undefined.
 * @param  {Any}  value
 * @return {Boolean}
 */
function _isUndefined(value) {
  return typeof value === "undefined";
}

/**
 * Sorts records in a list according to a given ordering.
 *
 * @param  {String} order The ordering, eg. `-last_modified`.
 * @param  {Array}  list  The collection to order.
 * @return {Array}
 */
function sortObjects(order, list) {
  const hasDash = order[0] === "-";
  const field = hasDash ? order.slice(1) : order;
  const direction = hasDash ? -1 : 1;
  return list.slice().sort((a, b) => {
    if (a[field] && _isUndefined(b[field])) {
      return direction;
    }
    if (b[field] && _isUndefined(a[field])) {
      return -direction;
    }
    if (_isUndefined(a[field]) && _isUndefined(b[field])) {
      return 0;
    }
    return a[field] > b[field] ? direction : -direction;
  });
}

/**
 * Filters records in a list matching all given filters.
 *
 * @param  {String} filters  The filters object.
 * @param  {Array}  list     The collection to filter.
 * @return {Array}
 */
function filterObjects(filters, list) {
  return list.filter(entry => {
    return Object.keys(filters).every(filter => {
      const value = filters[filter];
      if (Array.isArray(value)) {
        return value.some(candidate => candidate === entry[filter]);
      }
      return entry[filter] === value;
    });
  });
}

/**
 * Filter and sort list against provided filters and order.
 *
 * @param  {Object} filters  The filters to apply.
 * @param  {String} order    The order to apply.
 * @param  {Array}  list     The list to reduce.
 * @return {Array}
 */
function reduceRecords(filters, order, list) {
  const filtered = filters ? filterObjects(filters, list) : list;
  return order ? sortObjects(order, filtered) : filtered;
}

/**
 * Checks if a string is an UUID.
 *
 * @param  {String} uuid The uuid to validate.
 * @return {Boolean}
 */
function isUUID(uuid) {
  return RE_UUID.test(uuid);
}

/**
 * Resolves a list of functions sequentially, which can be sync or async; in
 * case of async, functions must return a promise.
 *
 * @param  {Array} fns  The list of functions.
 * @param  {Any}   init The initial value.
 * @return {Promise}
 */
function waterfall(fns, init) {
  if (!fns.length) {
    return Promise.resolve(init);
  }
  return fns.reduce((promise, nextFn) => {
    return promise.then(nextFn);
  }, Promise.resolve(init));
}

/**
 * Ensure a callback is always executed at the end of the passed promise flow.
 *
 * @link   https://github.com/domenic/promises-unwrapping/issues/18
 * @param  {Promise}  promise  The promise.
 * @param  {Function} fn       The callback.
 * @return {Promise}
 */
function pFinally(promise, fn) {
  return promise.then(value => Promise.resolve(fn()).then(() => value), reason => Promise.resolve(fn()).then(() => {
    throw reason;
  }));
}

/**
 * Simple deep object comparison function. This only supports comparison of
 * serializable JavaScript objects.
 *
 * @param  {Object} a The source object.
 * @param  {Object} b The compared object.
 * @return {Boolean}
 */
function deepEqual(a, b) {
  if (a === b) {
    return true;
  }
  if (typeof a !== typeof b) {
    return false;
  }
  if (!(a instanceof Object) || !(b instanceof Object)) {
    return false;
  }
  if (Object.keys(a).length !== Object.keys(b).length) {
    return false;
  }
  for (let k in a) {
    if (!deepEqual(a[k], b[k])) {
      return false;
    }
  }
  return true;
}

/**
 * Return an object without the specified keys.
 *
 * @param  {Object} obj        The original object.
 * @param  {Array}  keys       The list of keys to exclude.
 * @return {Object}            A copy without the specified keys.
 */
function omitKeys(obj, keys = []) {
  return Object.keys(obj).reduce((acc, key) => {
    if (keys.indexOf(key) === -1) {
      acc[key] = obj[key];
    }
    return acc;
  }, {});
}

},{}]},{},[2])(2)
});