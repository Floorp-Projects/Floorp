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

this.EXPORTED_SYMBOLS = ["Kinto"];

/*
 * Version 9.0.2 - b025c7b
 */

(function(f){if(typeof exports==="object"&&typeof module!=="undefined"){module.exports=f()}else if(typeof define==="function"&&define.amd){define([],f)}else{var g;if(typeof window!=="undefined"){g=window}else if(typeof global!=="undefined"){g=global}else if(typeof self!=="undefined"){g=self}else{g=this}g.Kinto = f()}})(function(){var define,module,exports;return (function e(t,n,r){function s(o,u){if(!n[o]){if(!t[o]){var a=typeof require=="function"&&require;if(!u&&a)return a(o,!0);if(i)return i(o,!0);var f=new Error("Cannot find module '"+o+"'");throw f.code="MODULE_NOT_FOUND",f}var l=n[o]={exports:{}};t[o][0].call(l.exports,function(e){var n=t[o][1][e];return s(n?n:e)},l,l.exports,e,t,n,r)}return n[o].exports}var i=typeof require=="function"&&require;for(var o=0;o<r.length;o++)s(r[o]);return s})({1:[function(require,module,exports){
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

var _KintoBase = require("../src/KintoBase");

var _KintoBase2 = _interopRequireDefault(_KintoBase);

var _utils = require("../src/utils");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Timer.jsm");
Cu.importGlobalProperties(["fetch"]);
const { EventEmitter } = Cu.import("resource://gre/modules/EventEmitter.jsm", {});
const { generateUUID } = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);

// Use standalone kinto-http module landed in FFx.
const { KintoHttpClient } = Cu.import("resource://services-common/kinto-http-client.js");

class Kinto extends _KintoBase2.default {
  constructor(options = {}) {
    const events = {};
    EventEmitter.decorate(events);

    const defaults = {
      events,
      ApiClass: KintoHttpClient
    };
    super(_extends({}, defaults, options));
  }

  collection(collName, options = {}) {
    const idSchema = {
      validate: _utils.RE_UUID.test.bind(_utils.RE_UUID),
      generate: function () {
        return generateUUID().toString().replace(/[{}]/g, "");
      }
    };
    return super.collection(collName, _extends({ idSchema }, options));
  }
}

exports.default = Kinto; // This fixes compatibility with CommonJS required by browserify.
// See http://stackoverflow.com/questions/33505992/babel-6-changes-how-it-exports-default/33683495#33683495

if (typeof module === "object") {
  module.exports = Kinto;
}

},{"../src/KintoBase":3,"../src/utils":7}],2:[function(require,module,exports){

},{}],3:[function(require,module,exports){
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
const DEFAULT_RETRY = 1;

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
   * - `{Object}`       `retry`          Number of retries when the server fails to process the request (default: `1`)
   * - `{String}`       `requestMode`    The HTTP CORS mode to use.
   * - `{Number}`       `timeout`        The requests timeout in ms (default: `5000`).
   *
   * @param  {Object} options The options object.
   */
  constructor(options = {}) {
    const defaults = {
      bucket: DEFAULT_BUCKET_NAME,
      remote: DEFAULT_REMOTE,
      retry: DEFAULT_RETRY
    };
    this._options = _extends({}, defaults, options);
    if (!this._options.adapter) {
      throw new Error("No adapter provided");
    }

    const {
      remote,
      events,
      headers,
      retry,
      requestMode,
      timeout,
      ApiClass
    } = this._options;

    // public properties

    /**
     * The kinto HTTP client instance.
     * @type {KintoClient}
     */
    this.api = new ApiClass(remote, {
      events,
      headers,
      retry,
      requestMode,
      timeout
    });
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
   * @param  {Object} [options={}]                 Extra options or override client's options.
   * @param  {Object} [options.idSchema]           IdSchema instance (default: UUID)
   * @param  {Object} [options.remoteTransformers] Array<RemoteTransformer> (default: `[]`])
   * @param  {Object} [options.hooks]              Array<Hook> (default: `[]`])
   * @return {Collection}
   */
  collection(collName, options = {}) {
    if (!collName) {
      throw new Error("missing collection name");
    }
    const { bucket, events, adapter, adapterOptions, dbPrefix } = _extends({}, this._options, options);
    const { idSchema, remoteTransformers, hooks } = options;

    return new _collection2.default(bucket, collName, this.api, {
      events,
      adapter,
      adapterOptions,
      dbPrefix,
      idSchema,
      remoteTransformers,
      hooks
    });
  }
}
exports.default = KintoBase;

},{"./adapters/base":5,"./collection":6}],4:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _base = require("./base.js");

var _base2 = _interopRequireDefault(_base);

var _utils = require("../utils");

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

const INDEXED_FIELDS = ["id", "_status", "last_modified"];

/**
 * IDB cursor handlers.
 * @type {Object}
 */
const cursorHandlers = {
  all(filters, done) {
    const results = [];
    return function (event) {
      const cursor = event.target.result;
      if (cursor) {
        if ((0, _utils.filterObject)(filters, cursor.value)) {
          results.push(cursor.value);
        }
        cursor.continue();
      } else {
        done(results);
      }
    };
  },

  in(values, done) {
    if (values.length === 0) {
      return done([]);
    }
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
 * @param  {Object}           filters    More filters.
 * @param  {Function}         done       The operation completion handler.
 * @return {IDBRequest}
 */
function createListRequest(store, indexField, value, filters, done) {
  if (!indexField) {
    // Get all records.
    const request = store.openCursor();
    request.onsuccess = cursorHandlers.all(filters, done);
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
  request.onsuccess = cursorHandlers.all(filters, done);
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

  _handleError(method, err) {
    const error = new Error(method + "() " + err.message);
    error.stack = err.stack;
    throw error;
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
    return Promise.resolve();
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
    var _this = this;

    return _asyncToGenerator(function* () {
      try {
        yield _this.open();
        return new Promise(function (resolve, reject) {
          const { transaction, store } = _this.prepare("readwrite");
          store.clear();
          transaction.onerror = function (event) {
            return reject(new Error(event.target.error));
          };
          transaction.oncomplete = function () {
            return resolve();
          };
        });
      } catch (e) {
        _this._handleError("clear", e);
      }
    })();
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
    var _this2 = this;

    return _asyncToGenerator(function* () {
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
      yield _this2.open();
      return new Promise(function (resolve, reject) {
        // Start transaction.
        const { transaction, store } = _this2.prepare("readwrite");
        // Preload specified records using index.
        const ids = options.preload;
        store.index("id").openCursor().onsuccess = cursorHandlers.in(ids, function (records) {
          // Store obtained records by id.
          const preloaded = records.reduce(function (acc, record) {
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
          transaction.onerror = function (event) {
            return reject(new Error(event.target.error));
          };
          transaction.oncomplete = function (event) {
            return resolve(result);
          };
        });
      });
    })();
  }

  /**
   * Retrieve a record by its primary key from the IndexedDB database.
   *
   * @override
   * @param  {String} id The record id.
   * @return {Promise}
   */
  get(id) {
    var _this3 = this;

    return _asyncToGenerator(function* () {
      try {
        yield _this3.open();
        return new Promise(function (resolve, reject) {
          const { transaction, store } = _this3.prepare();
          const request = store.get(id);
          transaction.onerror = function (event) {
            return reject(new Error(event.target.error));
          };
          transaction.oncomplete = function () {
            return resolve(request.result);
          };
        });
      } catch (e) {
        _this3._handleError("get", e);
      }
    })();
  }

  /**
   * Lists all records from the IndexedDB database.
   *
   * @override
   * @return {Promise}
   */
  list(params = { filters: {} }) {
    var _this4 = this;

    return _asyncToGenerator(function* () {
      const { filters } = params;
      const indexField = findIndexedField(filters);
      const value = filters[indexField];
      try {
        yield _this4.open();
        const results = yield new Promise(function (resolve, reject) {
          let results = [];
          // If `indexField` was used already, don't filter again.
          const remainingFilters = (0, _utils.omitKeys)(filters, indexField);

          const { transaction, store } = _this4.prepare();
          createListRequest(store, indexField, value, remainingFilters, function (_results) {
            // we have received all requested records, parking them within
            // current scope
            results = _results;
          });
          transaction.onerror = function (event) {
            return reject(new Error(event.target.error));
          };
          transaction.oncomplete = function (event) {
            return resolve(results);
          };
        });

        // The resulting list of records is sorted.
        // XXX: with some efforts, this could be fully implemented using IDB API.
        return params.order ? (0, _utils.sortObjects)(params.order, results) : results;
      } catch (e) {
        _this4._handleError("list", e);
      }
    })();
  }

  /**
   * Store the lastModified value into metadata store.
   *
   * @override
   * @param  {Number}  lastModified
   * @return {Promise}
   */
  saveLastModified(lastModified) {
    var _this5 = this;

    return _asyncToGenerator(function* () {
      const value = parseInt(lastModified, 10) || null;
      yield _this5.open();
      return new Promise(function (resolve, reject) {
        const { transaction, store } = _this5.prepare("readwrite", "__meta__");
        store.put({ name: "lastModified", value: value });
        transaction.onerror = function (event) {
          return reject(event.target.error);
        };
        transaction.oncomplete = function (event) {
          return resolve(value);
        };
      });
    })();
  }

  /**
   * Retrieve saved lastModified value.
   *
   * @override
   * @return {Promise}
   */
  getLastModified() {
    var _this6 = this;

    return _asyncToGenerator(function* () {
      yield _this6.open();
      return new Promise(function (resolve, reject) {
        const { transaction, store } = _this6.prepare(undefined, "__meta__");
        const request = store.get("lastModified");
        transaction.onerror = function (event) {
          return reject(event.target.error);
        };
        transaction.oncomplete = function (event) {
          resolve(request.result && request.result.value || null);
        };
      });
    })();
  }

  /**
   * Load a dump of records exported from a server.
   *
   * @abstract
   * @return {Promise}
   */
  loadDump(records) {
    var _this7 = this;

    return _asyncToGenerator(function* () {
      try {
        yield _this7.execute(function (transaction) {
          records.forEach(function (record) {
            return transaction.update(record);
          });
        });
        const previousLastModified = yield _this7.getLastModified();
        const lastModified = Math.max(...records.map(function (record) {
          return record.last_modified;
        }));
        if (lastModified > previousLastModified) {
          yield _this7.saveLastModified(lastModified);
        }
        return records;
      } catch (e) {
        _this7._handleError("loadDump", e);
      }
    })();
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

},{"../utils":7,"./base.js":5}],5:[function(require,module,exports){
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

function _asyncToGenerator(fn) { return function () { var gen = fn.apply(this, arguments); return new Promise(function (resolve, reject) { function step(key, arg) { try { var info = gen[key](arg); var value = info.value; } catch (error) { reject(error); return; } if (info.done) { resolve(value); } else { return Promise.resolve(value).then(function (value) { step("next", value); }, function (err) { step("throw", err); }); } } return step("next"); }); }; }

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
    // Deduplicate entries by id. If the values don't have `id` attribute, just
    // keep all.
    const deduplicated = this[type].concat(entries).reduce((acc, cur) => {
      const existing = acc.filter(r => cur.id && r.id ? cur.id != r.id : true);
      return existing.concat(cur);
    }, []);
    this[type] = deduplicated;
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
    const db = new DBAdapter(`${dbPrefix}${bucket}/${name}`, options.adapterOptions);
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

    for (const hook in hooks) {
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
    var _this = this;

    return _asyncToGenerator(function* () {
      yield _this.db.clear();
      yield _this.db.saveLastModified(null);
      return { data: [], permissions: {} };
    })();
  }

  /**
   * Encodes a record.
   *
   * @param  {String} type   Either "remote" or "local".
   * @param  {Object} record The record object to encode.
   * @return {Promise}
   */
  _encodeRecord(type, record) {
    if (!this[`${type}Transformers`].length) {
      return Promise.resolve(record);
    }
    return (0, _utils.waterfall)(this[`${type}Transformers`].map(transformer => {
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
    if (!this[`${type}Transformers`].length) {
      return Promise.resolve(record);
    }
    return (0, _utils.waterfall)(this[`${type}Transformers`].reverse().map(transformer => {
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
      return reject(`Invalid Id: ${newRecord.id}`);
    }
    return this.execute(txn => txn.create(newRecord), {
      preloadIds: [newRecord.id]
    }).catch(err => {
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
      return Promise.reject(new Error(`Invalid Id: ${record.id}`));
    }

    return this.execute(txn => txn.update(record, options), {
      preloadIds: [record.id]
    });
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
      return Promise.reject(new Error(`Invalid Id: ${record.id}`));
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
    var _this2 = this;

    return _asyncToGenerator(function* () {
      params = _extends({ order: "-last_modified", filters: {} }, params);
      const results = yield _this2.db.list(params);
      let data = results;
      if (!options.includeDeleted) {
        data = results.filter(function (record) {
          return record._status !== "deleted";
        });
      }
      return { data, permissions: {} };
    })();
  }

  /**
   * Imports remote changes into the local database.
   * This method is in charge of detecting the conflicts, and resolve them
   * according to the specified strategy.
   * @param  {SyncResultObject} syncResultObject The sync result object.
   * @param  {Array}            decodedChanges   The list of changes to import in the local database.
   * @param  {String}           strategy         The {@link Collection.strategy} (default: MANUAL)
   * @return {Promise}
   */
  importChanges(syncResultObject, decodedChanges, strategy = Collection.strategy.MANUAL) {
    var _this3 = this;

    return _asyncToGenerator(function* () {
      // Retrieve records matching change ids.
      try {
        const { imports, resolved } = yield _this3.db.execute(function (transaction) {
          const imports = decodedChanges.map(function (remote) {
            // Store remote change into local database.
            return importChange(transaction, remote, _this3.localFields);
          });
          const conflicts = imports.filter(function (i) {
            return i.type === "conflicts";
          }).map(function (i) {
            return i.data;
          });
          const resolved = _this3._handleConflicts(transaction, conflicts, strategy);
          return { imports, resolved };
        }, { preload: decodedChanges.map(function (record) {
            return record.id;
          }) });

        // Lists of created/updated/deleted records
        imports.forEach(function ({ type, data }) {
          return syncResultObject.add(type, data);
        });

        // Automatically resolved conflicts (if not manual)
        if (resolved.length > 0) {
          syncResultObject.reset("conflicts").add("resolved", resolved);
        }
      } catch (err) {
        const data = {
          type: "incoming",
          message: err.message,
          stack: err.stack
        };
        // XXX one error of the whole transaction instead of per atomic op
        syncResultObject.add("errors", data);
      }

      return syncResultObject;
    })();
  }

  /**
   * Imports the responses of pushed changes into the local database.
   * Basically it stores the timestamp assigned by the server into the local
   * database.
   * @param  {SyncResultObject} syncResultObject The sync result object.
   * @param  {Array}            toApplyLocally   The list of changes to import in the local database.
   * @param  {Array}            conflicts        The list of conflicts that have to be resolved.
   * @param  {String}           strategy         The {@link Collection.strategy}.
   * @return {Promise}
   */
  _applyPushedResults(syncResultObject, toApplyLocally, conflicts, strategy = Collection.strategy.MANUAL) {
    var _this4 = this;

    return _asyncToGenerator(function* () {
      const toDeleteLocally = toApplyLocally.filter(function (r) {
        return r.deleted;
      });
      const toUpdateLocally = toApplyLocally.filter(function (r) {
        return !r.deleted;
      });

      const { published, resolved } = yield _this4.db.execute(function (transaction) {
        const updated = toUpdateLocally.map(function (record) {
          const synced = markSynced(record);
          transaction.update(synced);
          return synced;
        });
        const deleted = toDeleteLocally.map(function (record) {
          transaction.delete(record.id);
          // Amend result data with the deleted attribute set
          return { id: record.id, deleted: true };
        });
        const published = updated.concat(deleted);
        // Handle conflicts, if any
        const resolved = _this4._handleConflicts(transaction, conflicts, strategy);
        return { published, resolved };
      });

      syncResultObject.add("published", published);

      if (resolved.length > 0) {
        syncResultObject.reset("conflicts").reset("resolved").add("resolved", resolved);
      }
      return syncResultObject;
    })();
  }

  /**
   * Handles synchronization conflicts according to specified strategy.
   *
   * @param  {SyncResultObject} result    The sync result object.
   * @param  {String}           strategy  The {@link Collection.strategy}.
   * @return {Promise<Array<Object>>} The resolved conflicts, as an
   *    array of {accepted, rejected} objects
   */
  _handleConflicts(transaction, conflicts, strategy) {
    if (strategy === Collection.strategy.MANUAL) {
      return [];
    }
    return conflicts.map(conflict => {
      const resolution = strategy === Collection.strategy.CLIENT_WINS ? conflict.local : conflict.remote;
      const rejected = strategy === Collection.strategy.CLIENT_WINS ? conflict.remote : conflict.local;
      let accepted, status, id;
      if (resolution === null) {
        // We "resolved" with the server-side deletion. Delete locally.
        // This only happens during SERVER_WINS because the local
        // version of a record can never be null.
        // We can get "null" from the remote side if we got a conflict
        // and there is no remote version available; see kinto-http.js
        // batch.js:aggregate.
        transaction.delete(conflict.local.id);
        accepted = null;
        // The record was deleted, but that status is "synced" with
        // the server, so we don't need to push the change.
        status = "synced";
        id = conflict.local.id;
      } else {
        const updated = this._resolveRaw(conflict, resolution);
        transaction.update(updated);
        accepted = updated;
        status = updated._status;
        id = updated.id;
      }
      return { rejected, accepted, id, _status: status };
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
    for (const id of preloadIds) {
      if (!this.idSchema.validate(id)) {
        return Promise.reject(Error(`Invalid Id: ${id}`));
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
    var _this5 = this;

    return _asyncToGenerator(function* () {
      const unsynced = yield _this5.list({ filters: { _status: ["deleted", "synced"] }, order: "" }, { includeDeleted: true });
      yield _this5.db.execute(function (transaction) {
        unsynced.data.forEach(function (record) {
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
      _this5._lastModified = null;
      yield _this5.db.saveLastModified(null);
      return unsynced.data.length;
    })();
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
    var _this6 = this;

    return _asyncToGenerator(function* () {
      const unsynced = yield _this6.list({
        filters: { _status: ["created", "updated"] },
        order: ""
      });
      const deleted = yield _this6.list({ filters: { _status: "deleted" }, order: "" }, { includeDeleted: true });

      return yield Promise.all(unsynced.data.concat(deleted.data).map(_this6._encodeRecord.bind(_this6, "remote")));
    })();
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
    var _this7 = this;

    return _asyncToGenerator(function* () {
      if (!syncResultObject.ok) {
        return syncResultObject;
      }

      const since = _this7.lastModified ? _this7.lastModified : yield _this7.db.getLastModified();

      options = _extends({
        strategy: Collection.strategy.MANUAL,
        lastModified: since,
        headers: {}
      }, options);

      // Optionally ignore some records when pulling for changes.
      // (avoid redownloading our own changes on last step of #sync())
      let filters;
      if (options.exclude) {
        // Limit the list of excluded records to the first 50 records in order
        // to remain under de-facto URL size limit (~2000 chars).
        // http://stackoverflow.com/questions/417142/what-is-the-maximum-length-of-a-url-in-different-browsers/417184#417184
        const exclude_id = options.exclude.slice(0, 50).map(function (r) {
          return r.id;
        }).join(",");
        filters = { exclude_id };
      }
      // First fetch remote changes from the server
      const { data, last_modified } = yield client.listRecords({
        // Since should be ETag (see https://github.com/Kinto/kinto.js/issues/356)
        since: options.lastModified ? `${options.lastModified}` : undefined,
        headers: options.headers,
        retry: options.retry,
        filters
      });
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

      syncResultObject.lastModified = unquoted;

      // Decode incoming changes.
      const decodedChanges = yield Promise.all(data.map(function (change) {
        return _this7._decodeRecord("remote", change);
      }));
      // Hook receives decoded records.
      const payload = { lastModified: unquoted, changes: decodedChanges };
      const afterHooks = yield _this7.applyHook("incoming-changes", payload);

      // No change, nothing to import.
      if (afterHooks.changes.length > 0) {
        // Reflect these changes locally
        yield _this7.importChanges(syncResultObject, afterHooks.changes, options.strategy);
      }
      return syncResultObject;
    })();
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
          throw new Error(`Invalid return value for hook: ${JSON.stringify(result)} has no 'then()' or 'changes' properties`);
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
   * @param  {Object}                 changes          The change object.
   * @param  {Array}                  changes.toDelete The list of records to delete.
   * @param  {Array}                  changes.toSync   The list of records to create/update.
   * @param  {Object}                 options          The options object.
   * @return {Promise}
   */
  pushChanges(client, changes, syncResultObject, options = {}) {
    var _this8 = this;

    return _asyncToGenerator(function* () {
      if (!syncResultObject.ok) {
        return syncResultObject;
      }
      const safe = !options.strategy || options.strategy !== Collection.CLIENT_WINS;
      const toDelete = changes.filter(function (r) {
        return r._status == "deleted";
      });
      const toSync = changes.filter(function (r) {
        return r._status != "deleted";
      });

      // Perform a batch request with every changes.
      const synced = yield client.batch(function (batch) {
        toDelete.forEach(function (r) {
          // never published locally deleted records should not be pusblished
          if (r.last_modified) {
            batch.deleteRecord(r);
          }
        });
        toSync.forEach(function (r) {
          // Clean local fields (like _status) before sending to server.
          const published = _this8.cleanLocalFields(r);
          if (r._status === "created") {
            batch.createRecord(published);
          } else {
            batch.updateRecord(published);
          }
        });
      }, {
        headers: options.headers,
        retry: options.retry,
        safe,
        aggregate: true
      });

      // Store outgoing errors into sync result object
      syncResultObject.add("errors", synced.errors.map(function (e) {
        return _extends({}, e, { type: "outgoing" });
      }));

      // Store outgoing conflicts into sync result object
      const conflicts = [];
      for (const _ref of synced.conflicts) {
        const { type, local, remote } = _ref;

        // Note: we ensure that local data are actually available, as they may
        // be missing in the case of a published deletion.
        const safeLocal = local && local.data || { id: remote.id };
        const realLocal = yield _this8._decodeRecord("remote", safeLocal);
        // We can get "null" from the remote side if we got a conflict
        // and there is no remote version available; see kinto-http.js
        // batch.js:aggregate.
        const realRemote = remote && (yield _this8._decodeRecord("remote", remote));
        const conflict = { type, local: realLocal, remote: realRemote };
        conflicts.push(conflict);
      }
      syncResultObject.add("conflicts", conflicts);

      // Records that must be deleted are either deletions that were pushed
      // to server (published) or deleted records that were never pushed (skipped).
      const missingRemotely = synced.skipped.map(function (r) {
        return _extends({}, r, { deleted: true });
      });

      // For created and updated records, the last_modified coming from server
      // will be stored locally.
      // Reflect publication results locally using the response from
      // the batch request.
      const published = synced.published.map(function (c) {
        return c.data;
      });
      const toApplyLocally = published.concat(missingRemotely);

      // Apply the decode transformers, if any
      const decoded = yield Promise.all(toApplyLocally.map(function (record) {
        return _this8._decodeRecord("remote", record);
      }));

      // We have to update the local records with the responses of the server
      // (eg. last_modified values etc.).
      if (decoded.length > 0 || conflicts.length > 0) {
        yield _this8._applyPushedResults(syncResultObject, decoded, conflicts, options.strategy);
      }

      return syncResultObject;
    })();
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
      last_modified: conflict.remote && conflict.remote.last_modified
    });
    // If the resolution object is strictly equal to the
    // remote record, then we can mark it as synced locally.
    // Otherwise, mark it as updated (so that the resolution is pushed).
    const synced = (0, _utils.deepEqual)(resolved, conflict.remote);
    return markStatus(resolved, synced ? "synced" : "updated");
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
   * - {Number} retry: Number of retries when server fails to process the request (default: 1).
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
    retry: 1,
    ignoreBackoff: false,
    bucket: null,
    collection: null,
    remote: null
  }) {
    var _this9 = this;

    return _asyncToGenerator(function* () {
      options = _extends({}, options, {
        bucket: options.bucket || _this9.bucket,
        collection: options.collection || _this9.name
      });

      const previousRemote = _this9.api.remote;
      if (options.remote) {
        // Note: setting the remote ensures it's valid, throws when invalid.
        _this9.api.remote = options.remote;
      }
      if (!options.ignoreBackoff && _this9.api.backoff > 0) {
        const seconds = Math.ceil(_this9.api.backoff / 1000);
        return Promise.reject(new Error(`Server is asking clients to back off; retry in ${seconds}s or use the ignoreBackoff option.`));
      }

      const client = _this9.api.bucket(options.bucket).collection(options.collection);

      const result = new SyncResultObject();
      try {
        // Fetch last changes from the server.
        yield _this9.pullChanges(client, result, options);
        const { lastModified } = result;

        // Fetch local changes
        const toSync = yield _this9.gatherLocalChanges();

        // Publish local changes and pull local resolutions
        yield _this9.pushChanges(client, toSync, result, options);

        // Publish local resolution of push conflicts to server (on CLIENT_WINS)
        const resolvedUnsynced = result.resolved.filter(function (r) {
          return r._status !== "synced";
        });
        if (resolvedUnsynced.length > 0) {
          const resolvedEncoded = yield Promise.all(resolvedUnsynced.map(function (resolution) {
            let record = resolution.accepted;
            if (record === null) {
              record = { id: resolution.id, _status: resolution._status };
            }
            return _this9._encodeRecord("remote", record);
          }));
          yield _this9.pushChanges(client, resolvedEncoded, result, options);
        }
        // Perform a last pull to catch changes that occured after the last pull,
        // while local changes were pushed. Do not do it nothing was pushed.
        if (result.published.length > 0) {
          // Avoid redownloading our own changes during the last pull.
          const pullOpts = _extends({}, options, {
            lastModified,
            exclude: result.published
          });
          yield _this9.pullChanges(client, result, pullOpts);
        }

        // Don't persist lastModified value if any conflict or error occured
        if (result.ok) {
          // No conflict occured, persist collection's lastModified value
          _this9._lastModified = yield _this9.db.saveLastModified(result.lastModified);
        }
      } catch (e) {
        _this9.events.emit("sync:error", _extends({}, options, { error: e }));
        throw e;
      } finally {
        // Ensure API default remote is reverted if a custom one's been used
        _this9.api.remote = previousRemote;
      }
      _this9.events.emit("sync:success", _extends({}, options, { result }));
      return result;
    })();
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
    var _this10 = this;

    return _asyncToGenerator(function* () {
      if (!Array.isArray(records)) {
        throw new Error("Records is not an array.");
      }

      for (const record of records) {
        if (!record.hasOwnProperty("id") || !_this10.idSchema.validate(record.id)) {
          throw new Error("Record has invalid ID: " + JSON.stringify(record));
        }

        if (!record.last_modified) {
          throw new Error("Record has no last_modified value: " + JSON.stringify(record));
        }
      }

      // Fetch all existing records from local database,
      // and skip those who are newer or not marked as synced.

      // XXX filter by status / ids in records

      const { data } = yield _this10.list({}, { includeDeleted: true });
      const existingById = data.reduce(function (acc, record) {
        acc[record.id] = record;
        return acc;
      }, {});

      const newRecords = records.filter(function (record) {
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

      return yield _this10.db.loadDump(newRecords.map(markSynced));
    })();
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
    for (const _ref2 of this._events) {
      const { action, payload } = _ref2;

      this.collection.events.emit(action, payload);
    }
    if (this._events.length > 0) {
      const targets = this._events.map(({ action, payload }) => _extends({
        action
      }, payload));
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
      throw new Error(`Record with id=${id} not found.`);
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
      throw new Error(`Record with id=${id} not found.`);
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
      throw new Error(`Invalid Id: ${record.id}`);
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
      throw new Error(`Invalid Id: ${record.id}`);
    }

    const oldRecord = this.adapterTransaction.get(record.id);
    if (!oldRecord) {
      throw new Error(`Record with id=${record.id} not found.`);
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
      throw new Error(`Invalid Id: ${record.id}`);
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

},{"./adapters/IDB":4,"./adapters/base":5,"./utils":7,"uuid":2}],7:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.sortObjects = sortObjects;
exports.filterObject = filterObject;
exports.filterObjects = filterObjects;
exports.isUUID = isUUID;
exports.waterfall = waterfall;
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
 * Test if a single object matches all given filters.
 *
 * @param  {Object} filters  The filters object.
 * @param  {Object} entry    The object to filter.
 * @return {Function}
 */
function filterObject(filters, entry) {
  return Object.keys(filters).every(filter => {
    const value = filters[filter];
    if (Array.isArray(value)) {
      return value.some(candidate => candidate === entry[filter]);
    }
    return entry[filter] === value;
  });
}

/**
 * Filters records in a list matching all given filters.
 *
 * @param  {Object} filters  The filters object.
 * @param  {Array}  list     The collection to filter.
 * @return {Array}
 */
function filterObjects(filters, list) {
  return list.filter(entry => {
    return filterObject(filters, entry);
  });
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
  if (!(a && typeof a == "object") || !(b && typeof b == "object")) {
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

},{}]},{},[1])(1)
});