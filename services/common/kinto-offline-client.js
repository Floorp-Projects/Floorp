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
 * Version 2.0.3 - 0faf45b
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
    UPDATE collection_data
      SET record = :record
        WHERE collection_name = :collection_name
        AND record_id = :record_id;`,

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

  "importData": `
    REPLACE INTO collection_data (collection_name, record_id, record)
      VALUES (:collection_name, :record_id, :record);`

};

const createStatements = ["createCollectionData", "createCollectionMetadata", "createCollectionDataRecordIdIndex"];

const currentSchemaVersion = 1;

class FirefoxAdapter extends _base2.default {
  constructor(collection) {
    super();
    this.collection = collection;
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
      const opts = { path: SQLITE_PATH, sharedMemoryCache: false };
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
    const preloaded = options.preload.reduce((acc, record) => {
      acc[record.id] = record;
      return acc;
    }, {});

    const proxy = transactionProxy(this.collection, preloaded);
    let result;
    try {
      result = callback(proxy);
    } catch (e) {
      return Promise.reject(e);
    }
    const conn = this._connection;
    return conn.executeTransaction(function* doExecuteTransaction() {
      for (let { statement, params } of proxy.operations) {
        yield conn.executeCached(statement, params);
      }
    }).then(_ => result);
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
          return result.length > 0 ? result[0].getResultByName('last_modified') : -1;
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

},{"../src/adapters/base":5,"../src/utils":7}],2:[function(require,module,exports){
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

  // Use standalone kinto-client module landed in FFx.
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
        ApiClass: KintoHttpClient
      };

      const expandedOptions = Object.assign(defaults, options);
      super(expandedOptions);
    }

    collection(collName, options = {}) {
      const idSchema = makeIDSchema();
      const expandedOptions = Object.assign({ idSchema }, options);
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

},{"../src/KintoBase":4,"../src/adapters/base":5,"../src/utils":7,"./FirefoxStorage":1}],3:[function(require,module,exports){

},{}],4:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

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
   * - `{String}`       `remote`      The server URL to use.
   * - `{String}`       `bucket`      The collection bucket name.
   * - `{EventEmitter}` `events`      Events handler.
   * - `{BaseAdapter}`  `adapter`     The base DB adapter class.
   * - `{String}`       `dbPrefix`    The DB name prefix.
   * - `{Object}`       `headers`     The HTTP headers to use.
   * - `{String}`       `requestMode` The HTTP CORS mode to use.
   *
   * @param  {Object} options The options object.
   */
  constructor(options = {}) {
    const defaults = {
      bucket: DEFAULT_BUCKET_NAME,
      remote: DEFAULT_REMOTE
    };
    this._options = Object.assign(defaults, options);
    if (!this._options.adapter) {
      throw new Error("No adapter provided");
    }

    const { remote, events, headers, requestMode, ApiClass } = this._options;
    this._api = new ApiClass(remote, { events, headers, requestMode });

    // public properties
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
    return new _collection2.default(bucket, collName, this._api, {
      events: this._options.events,
      adapter: this._options.adapter,
      dbPrefix: this._options.dbPrefix,
      idSchema: options.idSchema,
      remoteTransformers: options.remoteTransformers,
      hooks: options.hooks
    });
  }
}
exports.default = KintoBase;

},{"./adapters/base":5,"./collection":6}],5:[function(require,module,exports){
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

},{}],6:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.SyncResultObject = undefined;
exports.cleanRecord = cleanRecord;

var _base = require("./adapters/base");

var _base2 = _interopRequireDefault(_base);

var _utils = require("./utils");

var _uuid = require("uuid");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const RECORD_FIELDS_TO_CLEAN = ["_status", "last_modified"];
const AVAILABLE_HOOKS = ["incoming-changes"];

/**
 * Cleans a record object, excluding passed keys.
 *
 * @param  {Object} record        The record object.
 * @param  {Array}  excludeFields The list of keys to exclude.
 * @return {Object}               A clean copy of source record object.
 */
function cleanRecord(record, excludeFields = RECORD_FIELDS_TO_CLEAN) {
  return Object.keys(record).reduce((acc, key) => {
    if (excludeFields.indexOf(key) === -1) {
      acc[key] = record[key];
    }
    return acc;
  }, {});
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
  return Object.assign({}, record, { _status: status });
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
 * @return {Object}
 */
function importChange(transaction, remote) {
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
  const identical = (0, _utils.deepEqual)(cleanRecord(local), cleanRecord(remote));
  if (local._status !== "synced") {
    // Locally deleted, unsynced: scheduled for remote deletion.
    if (local._status === "deleted") {
      return { type: "skipped", data: local };
    }
    if (identical) {
      // If records are identical, import anyway, so we bump the
      // local last_modified value from the server and set record
      // status to "synced".
      const synced = markSynced(remote);
      transaction.update(synced);
      return { type: "updated", data: synced, previous: local };
    }
    return {
      type: "conflicts",
      data: { type: "incoming", local: local, remote: remote }
    };
  }
  if (remote.deleted) {
    transaction.delete(remote.id);
    return { type: "deleted", data: { id: local.id } };
  }
  const synced = markSynced(remote);
  transaction.update(synced);
  // if identical, simply exclude it from all lists
  const type = identical ? "void" : "updated";
  return { type, data: synced };
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

    const DBAdapter = options.adapter;
    if (!DBAdapter) {
      throw new Error("No adapter provided");
    }
    const dbPrefix = options.dbPrefix || "";
    const db = new DBAdapter(`${ dbPrefix }${ bucket }/${ name }`);
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
    this._apiCollection = this.api.bucket(this.bucket).collection(this.name);
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
   * Adds a record to the local database.
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
    const reject = msg => Promise.reject(new Error(msg));
    if (typeof record !== "object") {
      return reject("Record is not an object.");
    }
    if ((options.synced || options.useRecordId) && !record.id) {
      return reject("Missing required Id; synced and useRecordId options require one");
    }
    if (!options.synced && !options.useRecordId && record.id) {
      return reject("Extraneous Id; can't create a record having one set.");
    }
    const newRecord = Object.assign({}, record, {
      id: options.synced || options.useRecordId ? record.id : this.idSchema.generate(),
      _status: options.synced ? "synced" : "created"
    });
    if (!this.idSchema.validate(newRecord.id)) {
      return reject(`Invalid Id: ${ newRecord.id }`);
    }
    return this.db.execute(transaction => {
      transaction.create(newRecord);
      return { data: newRecord, permissions: {} };
    }).catch(err => {
      if (options.useRecordId) {
        throw new Error("Couldn't create record. It may have been virtually deleted.");
      }
      throw err;
    });
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
   * @return {Promise}
   */
  update(record, options = { synced: false, patch: false }) {
    if (typeof record !== "object") {
      return Promise.reject(new Error("Record is not an object."));
    }
    if (!record.id) {
      return Promise.reject(new Error("Cannot update a record missing id."));
    }
    if (!this.idSchema.validate(record.id)) {
      return Promise.reject(new Error(`Invalid Id: ${ record.id }`));
    }
    return this.get(record.id).then(res => {
      const existing = res.data;
      const newStatus = options.synced ? "synced" : "updated";
      return this.db.execute(transaction => {
        const source = options.patch ? Object.assign({}, existing, record) : record;
        const updated = markStatus(source, newStatus);
        if (existing.last_modified && !updated.last_modified) {
          updated.last_modified = existing.last_modified;
        }
        transaction.update(updated);
        return { data: updated, permissions: {} };
      });
    });
  }

  /**
   * Retrieve a record by its id from the local database.
   *
   * @param  {String} id
   * @param  {Object} options
   * @return {Promise}
   */
  get(id, options = { includeDeleted: false }) {
    if (!this.idSchema.validate(id)) {
      return Promise.reject(Error(`Invalid Id: ${ id }`));
    }
    return this.db.get(id).then(record => {
      if (!record || !options.includeDeleted && record._status === "deleted") {
        throw new Error(`Record with id=${ id } not found.`);
      } else {
        return { data: record, permissions: {} };
      }
    });
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
   * @return {Promise}
   */
  delete(id, options = { virtual: true }) {
    if (!this.idSchema.validate(id)) {
      return Promise.reject(new Error(`Invalid Id: ${ id }`));
    }
    // Ensure the record actually exists.
    return this.get(id, { includeDeleted: true }).then(res => {
      const existing = res.data;
      return this.db.execute(transaction => {
        // Virtual updates status.
        if (options.virtual) {
          transaction.update(markDeleted(existing));
        } else {
          // Delete for real.
          transaction.delete(id);
        }
        return { data: { id: id }, permissions: {} };
      });
    });
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
    params = Object.assign({ order: "-last_modified", filters: {} }, params);
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
      if (change.deleted) {
        return Promise.resolve(change);
      }
      return this._decodeRecord("remote", change);
    })).then(decodedChanges => {
      // No change, nothing to import.
      if (decodedChanges.length === 0) {
        return Promise.resolve(syncResultObject);
      }
      // Retrieve records matching change ids.
      const remoteIds = decodedChanges.map(change => change.id);
      return this.list({ filters: { id: remoteIds }, order: "" }, { includeDeleted: true }).then(res => ({ decodedChanges, existingRecords: res.data })).then(({ decodedChanges, existingRecords }) => {
        return this.db.execute(transaction => {
          return decodedChanges.map(remote => {
            // Store remote change into local database.
            return importChange(transaction, remote);
          });
        }, { preload: existingRecords });
      }).catch(err => {
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
            transaction.update(Object.assign({}, record, {
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
   * @return {Object}
   */
  gatherLocalChanges() {
    let _toDelete;
    return Promise.all([this.list({ filters: { _status: ["created", "updated"] }, order: "" }), this.list({ filters: { _status: "deleted" }, order: "" }, { includeDeleted: true })]).then(([unsynced, deleted]) => {
      _toDelete = deleted.data;
      // Encode unsynced records.
      return Promise.all(unsynced.data.map(this._encodeRecord.bind(this, "remote")));
    }).then(toSync => ({ toDelete: _toDelete, toSync }));
  }

  /**
   * Fetch remote changes, import them to the local database, and handle
   * conflicts according to `options.strategy`. Then, updates the passed
   * {@link SyncResultObject} with import results.
   *
   * Options:
   * - {String} strategy: The selected sync strategy.
   *
   * @param  {SyncResultObject} syncResultObject
   * @param  {Object}           options
   * @return {Promise}
   */
  pullChanges(syncResultObject, options = {}) {
    if (!syncResultObject.ok) {
      return Promise.resolve(syncResultObject);
    }
    options = Object.assign({
      strategy: Collection.strategy.MANUAL,
      lastModified: this.lastModified,
      headers: {}
    }, options);
    // First fetch remote changes from the server
    return this._apiCollection.listRecords({
      since: options.lastModified || undefined,
      headers: options.headers
    }).then(({ data, last_modified }) => {
      // last_modified is the ETag header value (string).
      // For retro-compatibility with first kinto.js versions
      // parse it to integer.
      const unquoted = last_modified ? parseInt(last_modified.replace(/"/g, ""), 10) : undefined;

      // Check if server was flushed.
      // This is relevant for the Kinto demo server
      // (and thus for many new comers).
      const localSynced = options.lastModified;
      const serverChanged = unquoted > options.lastModified;
      const emptyCollection = data.length === 0;
      if (localSynced && serverChanged && emptyCollection) {
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
      return record => hook(payload, this);
    }), payload);
  }

  /**
   * Publish local changes to the remote server and updates the passed
   * {@link SyncResultObject} with publication results.
   *
   * @param  {SyncResultObject} syncResultObject The sync result object.
   * @param  {Object}           options          The options object.
   * @return {Promise}
   */
  pushChanges(syncResultObject, options = {}) {
    if (!syncResultObject.ok) {
      return Promise.resolve(syncResultObject);
    }
    const safe = options.strategy === Collection.SERVER_WINS;
    options = Object.assign({ safe }, options);

    // Fetch local changes
    return this.gatherLocalChanges().then(({ toDelete, toSync }) => {
      // Send batch update requests
      return this._apiCollection.batch(batch => {
        toDelete.forEach(r => {
          // never published locally deleted records should not be pusblished
          if (r.last_modified) {
            batch.deleteRecord(r);
          }
        });
        toSync.forEach(r => {
          const isCreated = r._status === "created";
          // Do not store status on server.
          // XXX: cleanRecord() removes last_modified, required by safe.
          delete r._status;
          if (isCreated) {
            batch.createRecord(r);
          } else {
            batch.updateRecord(r);
          }
        });
      }, { headers: options.headers, safe: true, aggregate: true });
    })
    // Update published local records
    .then(synced => {
      // Merge outgoing errors into sync result object
      syncResultObject.add("errors", synced.errors.map(error => {
        error.type = "outgoing";
        return error;
      }));

      // The result of a batch returns data and permissions.
      // XXX: permissions are ignored currently.
      const conflicts = synced.conflicts.map(c => {
        return { type: c.type, local: c.local.data, remote: c.remote };
      });
      const published = synced.published.map(c => c.data);
      const skipped = synced.skipped.map(c => c.data);

      // Merge outgoing conflicts into sync result object
      syncResultObject.add("conflicts", conflicts);
      // Reflect publication results locally
      const missingRemotely = skipped.map(r => Object.assign({}, r, { deleted: true }));
      const toApplyLocally = published.concat(missingRemotely);
      // Deleted records are distributed accross local and missing records
      // XXX: When tackling the issue to avoid downloading our own changes
      // from the server. `toDeleteLocally` should be obtained from local db.
      // See https://github.com/Kinto/kinto.js/issues/144
      const toDeleteLocally = toApplyLocally.filter(r => r.deleted);
      const toUpdateLocally = toApplyLocally.filter(r => !r.deleted);
      // First, apply the decode transformers, if any
      return Promise.all(toUpdateLocally.map(record => {
        return this._decodeRecord("remote", record);
      }))
      // Process everything within a single transaction
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
        return this.pushChanges(result, Object.assign({}, options, { resolved: true }));
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
   * Resolves a conflict, updating local record according to proposed
   * resolution — keeping remote record `last_modified` value as a reference for
   * further batch sending.
   *
   * @param  {Object} conflict   The conflict object.
   * @param  {Object} resolution The proposed record.
   * @return {Promise}
   */
  resolve(conflict, resolution) {
    return this.update(Object.assign({}, resolution, {
      // Ensure local record has the latest authoritative timestamp
      last_modified: conflict.remote.last_modified
    }));
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
    return Promise.all(result.conflicts.map(conflict => {
      const resolution = strategy === Collection.strategy.CLIENT_WINS ? conflict.local : conflict.remote;
      return this.resolve(conflict, resolution);
    })).then(imports => {
      return result.reset("conflicts").add("resolved", imports.map(res => res.data));
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
    const result = new SyncResultObject();
    const syncPromise = this.db.getLastModified().then(lastModified => this._lastModified = lastModified).then(_ => this.pullChanges(result, options)).then(result => this.pushChanges(result, options)).then(result => {
      // Avoid performing a last pull if nothing has been published.
      if (result.published.length === 0) {
        return result;
      }
      return this.pullChanges(result, options);
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
      if (!record.id || !this.idSchema.validate(record.id)) {
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
exports.default = Collection;

},{"./adapters/base":5,"./utils":7,"uuid":3}],7:[function(require,module,exports){
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

},{}]},{},[2])(2)
});