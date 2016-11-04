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
 * Version 5.1.0 - 8beb61d
 */

(function(f){if(typeof exports==="object"&&typeof module!=="undefined"){module.exports=f()}else if(typeof define==="function"&&define.amd){define([],f)}else{var g;if(typeof window!=="undefined"){g=window}else if(typeof global!=="undefined"){g=global}else if(typeof self!=="undefined"){g=self}else{g=this}g.loadKinto = f()}})(function(){var define,module,exports;return (function e(t,n,r){function s(o,u){if(!n[o]){if(!t[o]){var a=typeof require=="function"&&require;if(!u&&a)return a(o,!0);if(i)return i(o,!0);var f=new Error("Cannot find module '"+o+"'");throw f.code="MODULE_NOT_FOUND",f}var l=n[o]={exports:{}};t[o][0].call(l.exports,function(e){var n=t[o][1][e];return s(n?n:e)},l,l.exports,e,t,n,r)}return n[o].exports}var i=typeof require=="function"&&require;for(var o=0;o<r.length;o++)s(r[o]);return s})({1:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _extends2 = require("babel-runtime/helpers/extends");

var _extends3 = _interopRequireDefault(_extends2);

var _stringify = require("babel-runtime/core-js/json/stringify");

var _stringify2 = _interopRequireDefault(_stringify);

var _promise = require("babel-runtime/core-js/promise");

var _promise2 = _interopRequireDefault(_promise);

exports.reduceRecords = reduceRecords;

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
      VALUES (:collection_name, :record_id, :record);`,

  "scanAllRecords": `SELECT * FROM collection_data;`,

  "clearCollectionMetadata": `DELETE FROM collection_metadata;`
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
    const { sqliteHandle = null } = options;
    this.collection = collection;
    this._connection = sqliteHandle;
    this._options = options;
  }

  // We need to be capable of calling this from "outside" the adapter
  // so that someone can initialize a connection and pass it to us in
  // adapterOptions.
  static _init(connection) {
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
      if (!self._connection) {
        const path = self._options.path || SQLITE_PATH;
        const opts = { path, sharedMemoryCache: false };
        self._connection = yield Sqlite.openConnection(opts).then(FirefoxAdapter._init);
      }
    });
  }

  close() {
    if (this._connection) {
      const promise = this._connection.close();
      this._connection = null;
      return promise;
    }
    return _promise2.default.resolve();
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
      return reduceRecords(params.filters, params.order, results);
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
            record: (0, _stringify2.default)(record)
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

  /**
   * Reset the sync status of every record and collection we have
   * access to.
   */
  resetSyncStatus() {
    // We're going to use execute instead of executeCached, so build
    // in our own sanity check
    if (!this._connection) {
      throw new Error("The storage adapter is not open");
    }

    return this._connection.executeTransaction(function* (conn) {
      const promises = [];
      yield conn.execute(statements.scanAllRecords, null, function (row) {
        const record = JSON.parse(row.getResultByName("record"));
        const record_id = row.getResultByName("record_id");
        const collection_name = row.getResultByName("collection_name");
        if (record._status === "deleted") {
          // Garbage collect deleted records.
          promises.push(conn.execute(statements.deleteData, { collection_name, record_id }));
        } else {
          const newRecord = (0, _extends3.default)({}, record, {
            _status: "created",
            last_modified: undefined
          });
          promises.push(conn.execute(statements.updateData, { record: (0, _stringify2.default)(newRecord), record_id, collection_name }));
        }
      });
      yield _promise2.default.all(promises);
      yield conn.execute(statements.clearCollectionMetadata);
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
          record: (0, _stringify2.default)(record)
        }
      });
    },

    update(record) {
      _operations.push({
        statement: statements.updateData,
        params: {
          collection_name: collection,
          record_id: record.id,
          record: (0, _stringify2.default)(record)
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

/**
 * Filter and sort list against provided filters and order.
 *
 * @param  {Object} filters  The filters to apply.
 * @param  {String} order    The order to apply.
 * @param  {Array}  list     The list to reduce.
 * @return {Array}
 */
function reduceRecords(filters, order, list) {
  const filtered = filters ? (0, _utils.filterObjects)(filters, list) : list;
  return order ? (0, _utils.sortObjects)(order, filtered) : filtered;
}

},{"../src/adapters/base":85,"../src/utils":87,"babel-runtime/core-js/json/stringify":3,"babel-runtime/core-js/promise":6,"babel-runtime/helpers/extends":8}],2:[function(require,module,exports){
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

var _extends2 = require("babel-runtime/helpers/extends");

var _extends3 = _interopRequireDefault(_extends2);

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

      const expandedOptions = (0, _extends3.default)({}, defaults, options);
      super(expandedOptions);
    }

    collection(collName, options = {}) {
      const idSchema = makeIDSchema();
      const expandedOptions = (0, _extends3.default)({ idSchema }, options);
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

},{"../src/KintoBase":83,"../src/adapters/base":85,"../src/utils":87,"./FirefoxStorage":1,"babel-runtime/helpers/extends":8}],3:[function(require,module,exports){
module.exports = { "default": require("core-js/library/fn/json/stringify"), __esModule: true };
},{"core-js/library/fn/json/stringify":10}],4:[function(require,module,exports){
module.exports = { "default": require("core-js/library/fn/object/assign"), __esModule: true };
},{"core-js/library/fn/object/assign":11}],5:[function(require,module,exports){
module.exports = { "default": require("core-js/library/fn/object/keys"), __esModule: true };
},{"core-js/library/fn/object/keys":12}],6:[function(require,module,exports){
module.exports = { "default": require("core-js/library/fn/promise"), __esModule: true };
},{"core-js/library/fn/promise":13}],7:[function(require,module,exports){
"use strict";

exports.__esModule = true;

var _promise = require("../core-js/promise");

var _promise2 = _interopRequireDefault(_promise);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

exports.default = function (fn) {
  return function () {
    var gen = fn.apply(this, arguments);
    return new _promise2.default(function (resolve, reject) {
      function step(key, arg) {
        try {
          var info = gen[key](arg);
          var value = info.value;
        } catch (error) {
          reject(error);
          return;
        }

        if (info.done) {
          resolve(value);
        } else {
          return _promise2.default.resolve(value).then(function (value) {
            return step("next", value);
          }, function (err) {
            return step("throw", err);
          });
        }
      }

      return step("next");
    });
  };
};
},{"../core-js/promise":6}],8:[function(require,module,exports){
"use strict";

exports.__esModule = true;

var _assign = require("../core-js/object/assign");

var _assign2 = _interopRequireDefault(_assign);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

exports.default = _assign2.default || function (target) {
  for (var i = 1; i < arguments.length; i++) {
    var source = arguments[i];

    for (var key in source) {
      if (Object.prototype.hasOwnProperty.call(source, key)) {
        target[key] = source[key];
      }
    }
  }

  return target;
};
},{"../core-js/object/assign":4}],9:[function(require,module,exports){

},{}],10:[function(require,module,exports){
var core  = require('../../modules/_core')
  , $JSON = core.JSON || (core.JSON = {stringify: JSON.stringify});
module.exports = function stringify(it){ // eslint-disable-line no-unused-vars
  return $JSON.stringify.apply($JSON, arguments);
};
},{"../../modules/_core":21}],11:[function(require,module,exports){
require('../../modules/es6.object.assign');
module.exports = require('../../modules/_core').Object.assign;
},{"../../modules/_core":21,"../../modules/es6.object.assign":77}],12:[function(require,module,exports){
require('../../modules/es6.object.keys');
module.exports = require('../../modules/_core').Object.keys;
},{"../../modules/_core":21,"../../modules/es6.object.keys":78}],13:[function(require,module,exports){
require('../modules/es6.object.to-string');
require('../modules/es6.string.iterator');
require('../modules/web.dom.iterable');
require('../modules/es6.promise');
module.exports = require('../modules/_core').Promise;
},{"../modules/_core":21,"../modules/es6.object.to-string":79,"../modules/es6.promise":80,"../modules/es6.string.iterator":81,"../modules/web.dom.iterable":82}],14:[function(require,module,exports){
module.exports = function(it){
  if(typeof it != 'function')throw TypeError(it + ' is not a function!');
  return it;
};
},{}],15:[function(require,module,exports){
module.exports = function(){ /* empty */ };
},{}],16:[function(require,module,exports){
module.exports = function(it, Constructor, name, forbiddenField){
  if(!(it instanceof Constructor) || (forbiddenField !== undefined && forbiddenField in it)){
    throw TypeError(name + ': incorrect invocation!');
  } return it;
};
},{}],17:[function(require,module,exports){
var isObject = require('./_is-object');
module.exports = function(it){
  if(!isObject(it))throw TypeError(it + ' is not an object!');
  return it;
};
},{"./_is-object":38}],18:[function(require,module,exports){
// false -> Array#indexOf
// true  -> Array#includes
var toIObject = require('./_to-iobject')
  , toLength  = require('./_to-length')
  , toIndex   = require('./_to-index');
module.exports = function(IS_INCLUDES){
  return function($this, el, fromIndex){
    var O      = toIObject($this)
      , length = toLength(O.length)
      , index  = toIndex(fromIndex, length)
      , value;
    // Array#includes uses SameValueZero equality algorithm
    if(IS_INCLUDES && el != el)while(length > index){
      value = O[index++];
      if(value != value)return true;
    // Array#toIndex ignores holes, Array#includes - not
    } else for(;length > index; index++)if(IS_INCLUDES || index in O){
      if(O[index] === el)return IS_INCLUDES || index || 0;
    } return !IS_INCLUDES && -1;
  };
};
},{"./_to-index":67,"./_to-iobject":69,"./_to-length":70}],19:[function(require,module,exports){
// getting tag from 19.1.3.6 Object.prototype.toString()
var cof = require('./_cof')
  , TAG = require('./_wks')('toStringTag')
  // ES3 wrong here
  , ARG = cof(function(){ return arguments; }()) == 'Arguments';

// fallback for IE11 Script Access Denied error
var tryGet = function(it, key){
  try {
    return it[key];
  } catch(e){ /* empty */ }
};

module.exports = function(it){
  var O, T, B;
  return it === undefined ? 'Undefined' : it === null ? 'Null'
    // @@toStringTag case
    : typeof (T = tryGet(O = Object(it), TAG)) == 'string' ? T
    // builtinTag case
    : ARG ? cof(O)
    // ES3 arguments fallback
    : (B = cof(O)) == 'Object' && typeof O.callee == 'function' ? 'Arguments' : B;
};
},{"./_cof":20,"./_wks":74}],20:[function(require,module,exports){
var toString = {}.toString;

module.exports = function(it){
  return toString.call(it).slice(8, -1);
};
},{}],21:[function(require,module,exports){
var core = module.exports = {version: '2.4.0'};
if(typeof __e == 'number')__e = core; // eslint-disable-line no-undef
},{}],22:[function(require,module,exports){
// optional / simple context binding
var aFunction = require('./_a-function');
module.exports = function(fn, that, length){
  aFunction(fn);
  if(that === undefined)return fn;
  switch(length){
    case 1: return function(a){
      return fn.call(that, a);
    };
    case 2: return function(a, b){
      return fn.call(that, a, b);
    };
    case 3: return function(a, b, c){
      return fn.call(that, a, b, c);
    };
  }
  return function(/* ...args */){
    return fn.apply(that, arguments);
  };
};
},{"./_a-function":14}],23:[function(require,module,exports){
// 7.2.1 RequireObjectCoercible(argument)
module.exports = function(it){
  if(it == undefined)throw TypeError("Can't call method on  " + it);
  return it;
};
},{}],24:[function(require,module,exports){
// Thank's IE8 for his funny defineProperty
module.exports = !require('./_fails')(function(){
  return Object.defineProperty({}, 'a', {get: function(){ return 7; }}).a != 7;
});
},{"./_fails":28}],25:[function(require,module,exports){
var isObject = require('./_is-object')
  , document = require('./_global').document
  // in old IE typeof document.createElement is 'object'
  , is = isObject(document) && isObject(document.createElement);
module.exports = function(it){
  return is ? document.createElement(it) : {};
};
},{"./_global":30,"./_is-object":38}],26:[function(require,module,exports){
// IE 8- don't enum bug keys
module.exports = (
  'constructor,hasOwnProperty,isPrototypeOf,propertyIsEnumerable,toLocaleString,toString,valueOf'
).split(',');
},{}],27:[function(require,module,exports){
var global    = require('./_global')
  , core      = require('./_core')
  , ctx       = require('./_ctx')
  , hide      = require('./_hide')
  , PROTOTYPE = 'prototype';

var $export = function(type, name, source){
  var IS_FORCED = type & $export.F
    , IS_GLOBAL = type & $export.G
    , IS_STATIC = type & $export.S
    , IS_PROTO  = type & $export.P
    , IS_BIND   = type & $export.B
    , IS_WRAP   = type & $export.W
    , exports   = IS_GLOBAL ? core : core[name] || (core[name] = {})
    , expProto  = exports[PROTOTYPE]
    , target    = IS_GLOBAL ? global : IS_STATIC ? global[name] : (global[name] || {})[PROTOTYPE]
    , key, own, out;
  if(IS_GLOBAL)source = name;
  for(key in source){
    // contains in native
    own = !IS_FORCED && target && target[key] !== undefined;
    if(own && key in exports)continue;
    // export native or passed
    out = own ? target[key] : source[key];
    // prevent global pollution for namespaces
    exports[key] = IS_GLOBAL && typeof target[key] != 'function' ? source[key]
    // bind timers to global for call from export context
    : IS_BIND && own ? ctx(out, global)
    // wrap global constructors for prevent change them in library
    : IS_WRAP && target[key] == out ? (function(C){
      var F = function(a, b, c){
        if(this instanceof C){
          switch(arguments.length){
            case 0: return new C;
            case 1: return new C(a);
            case 2: return new C(a, b);
          } return new C(a, b, c);
        } return C.apply(this, arguments);
      };
      F[PROTOTYPE] = C[PROTOTYPE];
      return F;
    // make static versions for prototype methods
    })(out) : IS_PROTO && typeof out == 'function' ? ctx(Function.call, out) : out;
    // export proto methods to core.%CONSTRUCTOR%.methods.%NAME%
    if(IS_PROTO){
      (exports.virtual || (exports.virtual = {}))[key] = out;
      // export proto methods to core.%CONSTRUCTOR%.prototype.%NAME%
      if(type & $export.R && expProto && !expProto[key])hide(expProto, key, out);
    }
  }
};
// type bitmap
$export.F = 1;   // forced
$export.G = 2;   // global
$export.S = 4;   // static
$export.P = 8;   // proto
$export.B = 16;  // bind
$export.W = 32;  // wrap
$export.U = 64;  // safe
$export.R = 128; // real proto method for `library` 
module.exports = $export;
},{"./_core":21,"./_ctx":22,"./_global":30,"./_hide":32}],28:[function(require,module,exports){
module.exports = function(exec){
  try {
    return !!exec();
  } catch(e){
    return true;
  }
};
},{}],29:[function(require,module,exports){
var ctx         = require('./_ctx')
  , call        = require('./_iter-call')
  , isArrayIter = require('./_is-array-iter')
  , anObject    = require('./_an-object')
  , toLength    = require('./_to-length')
  , getIterFn   = require('./core.get-iterator-method')
  , BREAK       = {}
  , RETURN      = {};
var exports = module.exports = function(iterable, entries, fn, that, ITERATOR){
  var iterFn = ITERATOR ? function(){ return iterable; } : getIterFn(iterable)
    , f      = ctx(fn, that, entries ? 2 : 1)
    , index  = 0
    , length, step, iterator, result;
  if(typeof iterFn != 'function')throw TypeError(iterable + ' is not iterable!');
  // fast case for arrays with default iterator
  if(isArrayIter(iterFn))for(length = toLength(iterable.length); length > index; index++){
    result = entries ? f(anObject(step = iterable[index])[0], step[1]) : f(iterable[index]);
    if(result === BREAK || result === RETURN)return result;
  } else for(iterator = iterFn.call(iterable); !(step = iterator.next()).done; ){
    result = call(iterator, f, step.value, entries);
    if(result === BREAK || result === RETURN)return result;
  }
};
exports.BREAK  = BREAK;
exports.RETURN = RETURN;
},{"./_an-object":17,"./_ctx":22,"./_is-array-iter":37,"./_iter-call":39,"./_to-length":70,"./core.get-iterator-method":75}],30:[function(require,module,exports){
// https://github.com/zloirock/core-js/issues/86#issuecomment-115759028
var global = module.exports = typeof window != 'undefined' && window.Math == Math
  ? window : typeof self != 'undefined' && self.Math == Math ? self : Function('return this')();
if(typeof __g == 'number')__g = global; // eslint-disable-line no-undef
},{}],31:[function(require,module,exports){
var hasOwnProperty = {}.hasOwnProperty;
module.exports = function(it, key){
  return hasOwnProperty.call(it, key);
};
},{}],32:[function(require,module,exports){
var dP         = require('./_object-dp')
  , createDesc = require('./_property-desc');
module.exports = require('./_descriptors') ? function(object, key, value){
  return dP.f(object, key, createDesc(1, value));
} : function(object, key, value){
  object[key] = value;
  return object;
};
},{"./_descriptors":24,"./_object-dp":49,"./_property-desc":57}],33:[function(require,module,exports){
module.exports = require('./_global').document && document.documentElement;
},{"./_global":30}],34:[function(require,module,exports){
module.exports = !require('./_descriptors') && !require('./_fails')(function(){
  return Object.defineProperty(require('./_dom-create')('div'), 'a', {get: function(){ return 7; }}).a != 7;
});
},{"./_descriptors":24,"./_dom-create":25,"./_fails":28}],35:[function(require,module,exports){
// fast apply, http://jsperf.lnkit.com/fast-apply/5
module.exports = function(fn, args, that){
  var un = that === undefined;
  switch(args.length){
    case 0: return un ? fn()
                      : fn.call(that);
    case 1: return un ? fn(args[0])
                      : fn.call(that, args[0]);
    case 2: return un ? fn(args[0], args[1])
                      : fn.call(that, args[0], args[1]);
    case 3: return un ? fn(args[0], args[1], args[2])
                      : fn.call(that, args[0], args[1], args[2]);
    case 4: return un ? fn(args[0], args[1], args[2], args[3])
                      : fn.call(that, args[0], args[1], args[2], args[3]);
  } return              fn.apply(that, args);
};
},{}],36:[function(require,module,exports){
// fallback for non-array-like ES3 and non-enumerable old V8 strings
var cof = require('./_cof');
module.exports = Object('z').propertyIsEnumerable(0) ? Object : function(it){
  return cof(it) == 'String' ? it.split('') : Object(it);
};
},{"./_cof":20}],37:[function(require,module,exports){
// check on default Array iterator
var Iterators  = require('./_iterators')
  , ITERATOR   = require('./_wks')('iterator')
  , ArrayProto = Array.prototype;

module.exports = function(it){
  return it !== undefined && (Iterators.Array === it || ArrayProto[ITERATOR] === it);
};
},{"./_iterators":44,"./_wks":74}],38:[function(require,module,exports){
module.exports = function(it){
  return typeof it === 'object' ? it !== null : typeof it === 'function';
};
},{}],39:[function(require,module,exports){
// call something on iterator step with safe closing on error
var anObject = require('./_an-object');
module.exports = function(iterator, fn, value, entries){
  try {
    return entries ? fn(anObject(value)[0], value[1]) : fn(value);
  // 7.4.6 IteratorClose(iterator, completion)
  } catch(e){
    var ret = iterator['return'];
    if(ret !== undefined)anObject(ret.call(iterator));
    throw e;
  }
};
},{"./_an-object":17}],40:[function(require,module,exports){
'use strict';
var create         = require('./_object-create')
  , descriptor     = require('./_property-desc')
  , setToStringTag = require('./_set-to-string-tag')
  , IteratorPrototype = {};

// 25.1.2.1.1 %IteratorPrototype%[@@iterator]()
require('./_hide')(IteratorPrototype, require('./_wks')('iterator'), function(){ return this; });

module.exports = function(Constructor, NAME, next){
  Constructor.prototype = create(IteratorPrototype, {next: descriptor(1, next)});
  setToStringTag(Constructor, NAME + ' Iterator');
};
},{"./_hide":32,"./_object-create":48,"./_property-desc":57,"./_set-to-string-tag":61,"./_wks":74}],41:[function(require,module,exports){
'use strict';
var LIBRARY        = require('./_library')
  , $export        = require('./_export')
  , redefine       = require('./_redefine')
  , hide           = require('./_hide')
  , has            = require('./_has')
  , Iterators      = require('./_iterators')
  , $iterCreate    = require('./_iter-create')
  , setToStringTag = require('./_set-to-string-tag')
  , getPrototypeOf = require('./_object-gpo')
  , ITERATOR       = require('./_wks')('iterator')
  , BUGGY          = !([].keys && 'next' in [].keys()) // Safari has buggy iterators w/o `next`
  , FF_ITERATOR    = '@@iterator'
  , KEYS           = 'keys'
  , VALUES         = 'values';

var returnThis = function(){ return this; };

module.exports = function(Base, NAME, Constructor, next, DEFAULT, IS_SET, FORCED){
  $iterCreate(Constructor, NAME, next);
  var getMethod = function(kind){
    if(!BUGGY && kind in proto)return proto[kind];
    switch(kind){
      case KEYS: return function keys(){ return new Constructor(this, kind); };
      case VALUES: return function values(){ return new Constructor(this, kind); };
    } return function entries(){ return new Constructor(this, kind); };
  };
  var TAG        = NAME + ' Iterator'
    , DEF_VALUES = DEFAULT == VALUES
    , VALUES_BUG = false
    , proto      = Base.prototype
    , $native    = proto[ITERATOR] || proto[FF_ITERATOR] || DEFAULT && proto[DEFAULT]
    , $default   = $native || getMethod(DEFAULT)
    , $entries   = DEFAULT ? !DEF_VALUES ? $default : getMethod('entries') : undefined
    , $anyNative = NAME == 'Array' ? proto.entries || $native : $native
    , methods, key, IteratorPrototype;
  // Fix native
  if($anyNative){
    IteratorPrototype = getPrototypeOf($anyNative.call(new Base));
    if(IteratorPrototype !== Object.prototype){
      // Set @@toStringTag to native iterators
      setToStringTag(IteratorPrototype, TAG, true);
      // fix for some old engines
      if(!LIBRARY && !has(IteratorPrototype, ITERATOR))hide(IteratorPrototype, ITERATOR, returnThis);
    }
  }
  // fix Array#{values, @@iterator}.name in V8 / FF
  if(DEF_VALUES && $native && $native.name !== VALUES){
    VALUES_BUG = true;
    $default = function values(){ return $native.call(this); };
  }
  // Define iterator
  if((!LIBRARY || FORCED) && (BUGGY || VALUES_BUG || !proto[ITERATOR])){
    hide(proto, ITERATOR, $default);
  }
  // Plug for library
  Iterators[NAME] = $default;
  Iterators[TAG]  = returnThis;
  if(DEFAULT){
    methods = {
      values:  DEF_VALUES ? $default : getMethod(VALUES),
      keys:    IS_SET     ? $default : getMethod(KEYS),
      entries: $entries
    };
    if(FORCED)for(key in methods){
      if(!(key in proto))redefine(proto, key, methods[key]);
    } else $export($export.P + $export.F * (BUGGY || VALUES_BUG), NAME, methods);
  }
  return methods;
};
},{"./_export":27,"./_has":31,"./_hide":32,"./_iter-create":40,"./_iterators":44,"./_library":45,"./_object-gpo":52,"./_redefine":59,"./_set-to-string-tag":61,"./_wks":74}],42:[function(require,module,exports){
var ITERATOR     = require('./_wks')('iterator')
  , SAFE_CLOSING = false;

try {
  var riter = [7][ITERATOR]();
  riter['return'] = function(){ SAFE_CLOSING = true; };
  Array.from(riter, function(){ throw 2; });
} catch(e){ /* empty */ }

module.exports = function(exec, skipClosing){
  if(!skipClosing && !SAFE_CLOSING)return false;
  var safe = false;
  try {
    var arr  = [7]
      , iter = arr[ITERATOR]();
    iter.next = function(){ return {done: safe = true}; };
    arr[ITERATOR] = function(){ return iter; };
    exec(arr);
  } catch(e){ /* empty */ }
  return safe;
};
},{"./_wks":74}],43:[function(require,module,exports){
module.exports = function(done, value){
  return {value: value, done: !!done};
};
},{}],44:[function(require,module,exports){
module.exports = {};
},{}],45:[function(require,module,exports){
module.exports = true;
},{}],46:[function(require,module,exports){
var global    = require('./_global')
  , macrotask = require('./_task').set
  , Observer  = global.MutationObserver || global.WebKitMutationObserver
  , process   = global.process
  , Promise   = global.Promise
  , isNode    = require('./_cof')(process) == 'process';

module.exports = function(){
  var head, last, notify;

  var flush = function(){
    var parent, fn;
    if(isNode && (parent = process.domain))parent.exit();
    while(head){
      fn   = head.fn;
      head = head.next;
      try {
        fn();
      } catch(e){
        if(head)notify();
        else last = undefined;
        throw e;
      }
    } last = undefined;
    if(parent)parent.enter();
  };

  // Node.js
  if(isNode){
    notify = function(){
      process.nextTick(flush);
    };
  // browsers with MutationObserver
  } else if(Observer){
    var toggle = true
      , node   = document.createTextNode('');
    new Observer(flush).observe(node, {characterData: true}); // eslint-disable-line no-new
    notify = function(){
      node.data = toggle = !toggle;
    };
  // environments with maybe non-completely correct, but existent Promise
  } else if(Promise && Promise.resolve){
    var promise = Promise.resolve();
    notify = function(){
      promise.then(flush);
    };
  // for other environments - macrotask based on:
  // - setImmediate
  // - MessageChannel
  // - window.postMessag
  // - onreadystatechange
  // - setTimeout
  } else {
    notify = function(){
      // strange IE + webpack dev server bug - use .call(global)
      macrotask.call(global, flush);
    };
  }

  return function(fn){
    var task = {fn: fn, next: undefined};
    if(last)last.next = task;
    if(!head){
      head = task;
      notify();
    } last = task;
  };
};
},{"./_cof":20,"./_global":30,"./_task":66}],47:[function(require,module,exports){
'use strict';
// 19.1.2.1 Object.assign(target, source, ...)
var getKeys  = require('./_object-keys')
  , gOPS     = require('./_object-gops')
  , pIE      = require('./_object-pie')
  , toObject = require('./_to-object')
  , IObject  = require('./_iobject')
  , $assign  = Object.assign;

// should work with symbols and should have deterministic property order (V8 bug)
module.exports = !$assign || require('./_fails')(function(){
  var A = {}
    , B = {}
    , S = Symbol()
    , K = 'abcdefghijklmnopqrst';
  A[S] = 7;
  K.split('').forEach(function(k){ B[k] = k; });
  return $assign({}, A)[S] != 7 || Object.keys($assign({}, B)).join('') != K;
}) ? function assign(target, source){ // eslint-disable-line no-unused-vars
  var T     = toObject(target)
    , aLen  = arguments.length
    , index = 1
    , getSymbols = gOPS.f
    , isEnum     = pIE.f;
  while(aLen > index){
    var S      = IObject(arguments[index++])
      , keys   = getSymbols ? getKeys(S).concat(getSymbols(S)) : getKeys(S)
      , length = keys.length
      , j      = 0
      , key;
    while(length > j)if(isEnum.call(S, key = keys[j++]))T[key] = S[key];
  } return T;
} : $assign;
},{"./_fails":28,"./_iobject":36,"./_object-gops":51,"./_object-keys":54,"./_object-pie":55,"./_to-object":71}],48:[function(require,module,exports){
// 19.1.2.2 / 15.2.3.5 Object.create(O [, Properties])
var anObject    = require('./_an-object')
  , dPs         = require('./_object-dps')
  , enumBugKeys = require('./_enum-bug-keys')
  , IE_PROTO    = require('./_shared-key')('IE_PROTO')
  , Empty       = function(){ /* empty */ }
  , PROTOTYPE   = 'prototype';

// Create object with fake `null` prototype: use iframe Object with cleared prototype
var createDict = function(){
  // Thrash, waste and sodomy: IE GC bug
  var iframe = require('./_dom-create')('iframe')
    , i      = enumBugKeys.length
    , lt     = '<'
    , gt     = '>'
    , iframeDocument;
  iframe.style.display = 'none';
  require('./_html').appendChild(iframe);
  iframe.src = 'javascript:'; // eslint-disable-line no-script-url
  // createDict = iframe.contentWindow.Object;
  // html.removeChild(iframe);
  iframeDocument = iframe.contentWindow.document;
  iframeDocument.open();
  iframeDocument.write(lt + 'script' + gt + 'document.F=Object' + lt + '/script' + gt);
  iframeDocument.close();
  createDict = iframeDocument.F;
  while(i--)delete createDict[PROTOTYPE][enumBugKeys[i]];
  return createDict();
};

module.exports = Object.create || function create(O, Properties){
  var result;
  if(O !== null){
    Empty[PROTOTYPE] = anObject(O);
    result = new Empty;
    Empty[PROTOTYPE] = null;
    // add "__proto__" for Object.getPrototypeOf polyfill
    result[IE_PROTO] = O;
  } else result = createDict();
  return Properties === undefined ? result : dPs(result, Properties);
};

},{"./_an-object":17,"./_dom-create":25,"./_enum-bug-keys":26,"./_html":33,"./_object-dps":50,"./_shared-key":62}],49:[function(require,module,exports){
var anObject       = require('./_an-object')
  , IE8_DOM_DEFINE = require('./_ie8-dom-define')
  , toPrimitive    = require('./_to-primitive')
  , dP             = Object.defineProperty;

exports.f = require('./_descriptors') ? Object.defineProperty : function defineProperty(O, P, Attributes){
  anObject(O);
  P = toPrimitive(P, true);
  anObject(Attributes);
  if(IE8_DOM_DEFINE)try {
    return dP(O, P, Attributes);
  } catch(e){ /* empty */ }
  if('get' in Attributes || 'set' in Attributes)throw TypeError('Accessors not supported!');
  if('value' in Attributes)O[P] = Attributes.value;
  return O;
};
},{"./_an-object":17,"./_descriptors":24,"./_ie8-dom-define":34,"./_to-primitive":72}],50:[function(require,module,exports){
var dP       = require('./_object-dp')
  , anObject = require('./_an-object')
  , getKeys  = require('./_object-keys');

module.exports = require('./_descriptors') ? Object.defineProperties : function defineProperties(O, Properties){
  anObject(O);
  var keys   = getKeys(Properties)
    , length = keys.length
    , i = 0
    , P;
  while(length > i)dP.f(O, P = keys[i++], Properties[P]);
  return O;
};
},{"./_an-object":17,"./_descriptors":24,"./_object-dp":49,"./_object-keys":54}],51:[function(require,module,exports){
exports.f = Object.getOwnPropertySymbols;
},{}],52:[function(require,module,exports){
// 19.1.2.9 / 15.2.3.2 Object.getPrototypeOf(O)
var has         = require('./_has')
  , toObject    = require('./_to-object')
  , IE_PROTO    = require('./_shared-key')('IE_PROTO')
  , ObjectProto = Object.prototype;

module.exports = Object.getPrototypeOf || function(O){
  O = toObject(O);
  if(has(O, IE_PROTO))return O[IE_PROTO];
  if(typeof O.constructor == 'function' && O instanceof O.constructor){
    return O.constructor.prototype;
  } return O instanceof Object ? ObjectProto : null;
};
},{"./_has":31,"./_shared-key":62,"./_to-object":71}],53:[function(require,module,exports){
var has          = require('./_has')
  , toIObject    = require('./_to-iobject')
  , arrayIndexOf = require('./_array-includes')(false)
  , IE_PROTO     = require('./_shared-key')('IE_PROTO');

module.exports = function(object, names){
  var O      = toIObject(object)
    , i      = 0
    , result = []
    , key;
  for(key in O)if(key != IE_PROTO)has(O, key) && result.push(key);
  // Don't enum bug & hidden keys
  while(names.length > i)if(has(O, key = names[i++])){
    ~arrayIndexOf(result, key) || result.push(key);
  }
  return result;
};
},{"./_array-includes":18,"./_has":31,"./_shared-key":62,"./_to-iobject":69}],54:[function(require,module,exports){
// 19.1.2.14 / 15.2.3.14 Object.keys(O)
var $keys       = require('./_object-keys-internal')
  , enumBugKeys = require('./_enum-bug-keys');

module.exports = Object.keys || function keys(O){
  return $keys(O, enumBugKeys);
};
},{"./_enum-bug-keys":26,"./_object-keys-internal":53}],55:[function(require,module,exports){
exports.f = {}.propertyIsEnumerable;
},{}],56:[function(require,module,exports){
// most Object methods by ES6 should accept primitives
var $export = require('./_export')
  , core    = require('./_core')
  , fails   = require('./_fails');
module.exports = function(KEY, exec){
  var fn  = (core.Object || {})[KEY] || Object[KEY]
    , exp = {};
  exp[KEY] = exec(fn);
  $export($export.S + $export.F * fails(function(){ fn(1); }), 'Object', exp);
};
},{"./_core":21,"./_export":27,"./_fails":28}],57:[function(require,module,exports){
module.exports = function(bitmap, value){
  return {
    enumerable  : !(bitmap & 1),
    configurable: !(bitmap & 2),
    writable    : !(bitmap & 4),
    value       : value
  };
};
},{}],58:[function(require,module,exports){
var hide = require('./_hide');
module.exports = function(target, src, safe){
  for(var key in src){
    if(safe && target[key])target[key] = src[key];
    else hide(target, key, src[key]);
  } return target;
};
},{"./_hide":32}],59:[function(require,module,exports){
module.exports = require('./_hide');
},{"./_hide":32}],60:[function(require,module,exports){
'use strict';
var global      = require('./_global')
  , core        = require('./_core')
  , dP          = require('./_object-dp')
  , DESCRIPTORS = require('./_descriptors')
  , SPECIES     = require('./_wks')('species');

module.exports = function(KEY){
  var C = typeof core[KEY] == 'function' ? core[KEY] : global[KEY];
  if(DESCRIPTORS && C && !C[SPECIES])dP.f(C, SPECIES, {
    configurable: true,
    get: function(){ return this; }
  });
};
},{"./_core":21,"./_descriptors":24,"./_global":30,"./_object-dp":49,"./_wks":74}],61:[function(require,module,exports){
var def = require('./_object-dp').f
  , has = require('./_has')
  , TAG = require('./_wks')('toStringTag');

module.exports = function(it, tag, stat){
  if(it && !has(it = stat ? it : it.prototype, TAG))def(it, TAG, {configurable: true, value: tag});
};
},{"./_has":31,"./_object-dp":49,"./_wks":74}],62:[function(require,module,exports){
var shared = require('./_shared')('keys')
  , uid    = require('./_uid');
module.exports = function(key){
  return shared[key] || (shared[key] = uid(key));
};
},{"./_shared":63,"./_uid":73}],63:[function(require,module,exports){
var global = require('./_global')
  , SHARED = '__core-js_shared__'
  , store  = global[SHARED] || (global[SHARED] = {});
module.exports = function(key){
  return store[key] || (store[key] = {});
};
},{"./_global":30}],64:[function(require,module,exports){
// 7.3.20 SpeciesConstructor(O, defaultConstructor)
var anObject  = require('./_an-object')
  , aFunction = require('./_a-function')
  , SPECIES   = require('./_wks')('species');
module.exports = function(O, D){
  var C = anObject(O).constructor, S;
  return C === undefined || (S = anObject(C)[SPECIES]) == undefined ? D : aFunction(S);
};
},{"./_a-function":14,"./_an-object":17,"./_wks":74}],65:[function(require,module,exports){
var toInteger = require('./_to-integer')
  , defined   = require('./_defined');
// true  -> String#at
// false -> String#codePointAt
module.exports = function(TO_STRING){
  return function(that, pos){
    var s = String(defined(that))
      , i = toInteger(pos)
      , l = s.length
      , a, b;
    if(i < 0 || i >= l)return TO_STRING ? '' : undefined;
    a = s.charCodeAt(i);
    return a < 0xd800 || a > 0xdbff || i + 1 === l || (b = s.charCodeAt(i + 1)) < 0xdc00 || b > 0xdfff
      ? TO_STRING ? s.charAt(i) : a
      : TO_STRING ? s.slice(i, i + 2) : (a - 0xd800 << 10) + (b - 0xdc00) + 0x10000;
  };
};
},{"./_defined":23,"./_to-integer":68}],66:[function(require,module,exports){
var ctx                = require('./_ctx')
  , invoke             = require('./_invoke')
  , html               = require('./_html')
  , cel                = require('./_dom-create')
  , global             = require('./_global')
  , process            = global.process
  , setTask            = global.setImmediate
  , clearTask          = global.clearImmediate
  , MessageChannel     = global.MessageChannel
  , counter            = 0
  , queue              = {}
  , ONREADYSTATECHANGE = 'onreadystatechange'
  , defer, channel, port;
var run = function(){
  var id = +this;
  if(queue.hasOwnProperty(id)){
    var fn = queue[id];
    delete queue[id];
    fn();
  }
};
var listener = function(event){
  run.call(event.data);
};
// Node.js 0.9+ & IE10+ has setImmediate, otherwise:
if(!setTask || !clearTask){
  setTask = function setImmediate(fn){
    var args = [], i = 1;
    while(arguments.length > i)args.push(arguments[i++]);
    queue[++counter] = function(){
      invoke(typeof fn == 'function' ? fn : Function(fn), args);
    };
    defer(counter);
    return counter;
  };
  clearTask = function clearImmediate(id){
    delete queue[id];
  };
  // Node.js 0.8-
  if(require('./_cof')(process) == 'process'){
    defer = function(id){
      process.nextTick(ctx(run, id, 1));
    };
  // Browsers with MessageChannel, includes WebWorkers
  } else if(MessageChannel){
    channel = new MessageChannel;
    port    = channel.port2;
    channel.port1.onmessage = listener;
    defer = ctx(port.postMessage, port, 1);
  // Browsers with postMessage, skip WebWorkers
  // IE8 has postMessage, but it's sync & typeof its postMessage is 'object'
  } else if(global.addEventListener && typeof postMessage == 'function' && !global.importScripts){
    defer = function(id){
      global.postMessage(id + '', '*');
    };
    global.addEventListener('message', listener, false);
  // IE8-
  } else if(ONREADYSTATECHANGE in cel('script')){
    defer = function(id){
      html.appendChild(cel('script'))[ONREADYSTATECHANGE] = function(){
        html.removeChild(this);
        run.call(id);
      };
    };
  // Rest old browsers
  } else {
    defer = function(id){
      setTimeout(ctx(run, id, 1), 0);
    };
  }
}
module.exports = {
  set:   setTask,
  clear: clearTask
};
},{"./_cof":20,"./_ctx":22,"./_dom-create":25,"./_global":30,"./_html":33,"./_invoke":35}],67:[function(require,module,exports){
var toInteger = require('./_to-integer')
  , max       = Math.max
  , min       = Math.min;
module.exports = function(index, length){
  index = toInteger(index);
  return index < 0 ? max(index + length, 0) : min(index, length);
};
},{"./_to-integer":68}],68:[function(require,module,exports){
// 7.1.4 ToInteger
var ceil  = Math.ceil
  , floor = Math.floor;
module.exports = function(it){
  return isNaN(it = +it) ? 0 : (it > 0 ? floor : ceil)(it);
};
},{}],69:[function(require,module,exports){
// to indexed object, toObject with fallback for non-array-like ES3 strings
var IObject = require('./_iobject')
  , defined = require('./_defined');
module.exports = function(it){
  return IObject(defined(it));
};
},{"./_defined":23,"./_iobject":36}],70:[function(require,module,exports){
// 7.1.15 ToLength
var toInteger = require('./_to-integer')
  , min       = Math.min;
module.exports = function(it){
  return it > 0 ? min(toInteger(it), 0x1fffffffffffff) : 0; // pow(2, 53) - 1 == 9007199254740991
};
},{"./_to-integer":68}],71:[function(require,module,exports){
// 7.1.13 ToObject(argument)
var defined = require('./_defined');
module.exports = function(it){
  return Object(defined(it));
};
},{"./_defined":23}],72:[function(require,module,exports){
// 7.1.1 ToPrimitive(input [, PreferredType])
var isObject = require('./_is-object');
// instead of the ES6 spec version, we didn't implement @@toPrimitive case
// and the second argument - flag - preferred type is a string
module.exports = function(it, S){
  if(!isObject(it))return it;
  var fn, val;
  if(S && typeof (fn = it.toString) == 'function' && !isObject(val = fn.call(it)))return val;
  if(typeof (fn = it.valueOf) == 'function' && !isObject(val = fn.call(it)))return val;
  if(!S && typeof (fn = it.toString) == 'function' && !isObject(val = fn.call(it)))return val;
  throw TypeError("Can't convert object to primitive value");
};
},{"./_is-object":38}],73:[function(require,module,exports){
var id = 0
  , px = Math.random();
module.exports = function(key){
  return 'Symbol('.concat(key === undefined ? '' : key, ')_', (++id + px).toString(36));
};
},{}],74:[function(require,module,exports){
var store      = require('./_shared')('wks')
  , uid        = require('./_uid')
  , Symbol     = require('./_global').Symbol
  , USE_SYMBOL = typeof Symbol == 'function';

var $exports = module.exports = function(name){
  return store[name] || (store[name] =
    USE_SYMBOL && Symbol[name] || (USE_SYMBOL ? Symbol : uid)('Symbol.' + name));
};

$exports.store = store;
},{"./_global":30,"./_shared":63,"./_uid":73}],75:[function(require,module,exports){
var classof   = require('./_classof')
  , ITERATOR  = require('./_wks')('iterator')
  , Iterators = require('./_iterators');
module.exports = require('./_core').getIteratorMethod = function(it){
  if(it != undefined)return it[ITERATOR]
    || it['@@iterator']
    || Iterators[classof(it)];
};
},{"./_classof":19,"./_core":21,"./_iterators":44,"./_wks":74}],76:[function(require,module,exports){
'use strict';
var addToUnscopables = require('./_add-to-unscopables')
  , step             = require('./_iter-step')
  , Iterators        = require('./_iterators')
  , toIObject        = require('./_to-iobject');

// 22.1.3.4 Array.prototype.entries()
// 22.1.3.13 Array.prototype.keys()
// 22.1.3.29 Array.prototype.values()
// 22.1.3.30 Array.prototype[@@iterator]()
module.exports = require('./_iter-define')(Array, 'Array', function(iterated, kind){
  this._t = toIObject(iterated); // target
  this._i = 0;                   // next index
  this._k = kind;                // kind
// 22.1.5.2.1 %ArrayIteratorPrototype%.next()
}, function(){
  var O     = this._t
    , kind  = this._k
    , index = this._i++;
  if(!O || index >= O.length){
    this._t = undefined;
    return step(1);
  }
  if(kind == 'keys'  )return step(0, index);
  if(kind == 'values')return step(0, O[index]);
  return step(0, [index, O[index]]);
}, 'values');

// argumentsList[@@iterator] is %ArrayProto_values% (9.4.4.6, 9.4.4.7)
Iterators.Arguments = Iterators.Array;

addToUnscopables('keys');
addToUnscopables('values');
addToUnscopables('entries');
},{"./_add-to-unscopables":15,"./_iter-define":41,"./_iter-step":43,"./_iterators":44,"./_to-iobject":69}],77:[function(require,module,exports){
// 19.1.3.1 Object.assign(target, source)
var $export = require('./_export');

$export($export.S + $export.F, 'Object', {assign: require('./_object-assign')});
},{"./_export":27,"./_object-assign":47}],78:[function(require,module,exports){
// 19.1.2.14 Object.keys(O)
var toObject = require('./_to-object')
  , $keys    = require('./_object-keys');

require('./_object-sap')('keys', function(){
  return function keys(it){
    return $keys(toObject(it));
  };
});
},{"./_object-keys":54,"./_object-sap":56,"./_to-object":71}],79:[function(require,module,exports){
arguments[4][9][0].apply(exports,arguments)
},{"dup":9}],80:[function(require,module,exports){
'use strict';
var LIBRARY            = require('./_library')
  , global             = require('./_global')
  , ctx                = require('./_ctx')
  , classof            = require('./_classof')
  , $export            = require('./_export')
  , isObject           = require('./_is-object')
  , aFunction          = require('./_a-function')
  , anInstance         = require('./_an-instance')
  , forOf              = require('./_for-of')
  , speciesConstructor = require('./_species-constructor')
  , task               = require('./_task').set
  , microtask          = require('./_microtask')()
  , PROMISE            = 'Promise'
  , TypeError          = global.TypeError
  , process            = global.process
  , $Promise           = global[PROMISE]
  , process            = global.process
  , isNode             = classof(process) == 'process'
  , empty              = function(){ /* empty */ }
  , Internal, GenericPromiseCapability, Wrapper;

var USE_NATIVE = !!function(){
  try {
    // correct subclassing with @@species support
    var promise     = $Promise.resolve(1)
      , FakePromise = (promise.constructor = {})[require('./_wks')('species')] = function(exec){ exec(empty, empty); };
    // unhandled rejections tracking support, NodeJS Promise without it fails @@species test
    return (isNode || typeof PromiseRejectionEvent == 'function') && promise.then(empty) instanceof FakePromise;
  } catch(e){ /* empty */ }
}();

// helpers
var sameConstructor = function(a, b){
  // with library wrapper special case
  return a === b || a === $Promise && b === Wrapper;
};
var isThenable = function(it){
  var then;
  return isObject(it) && typeof (then = it.then) == 'function' ? then : false;
};
var newPromiseCapability = function(C){
  return sameConstructor($Promise, C)
    ? new PromiseCapability(C)
    : new GenericPromiseCapability(C);
};
var PromiseCapability = GenericPromiseCapability = function(C){
  var resolve, reject;
  this.promise = new C(function($$resolve, $$reject){
    if(resolve !== undefined || reject !== undefined)throw TypeError('Bad Promise constructor');
    resolve = $$resolve;
    reject  = $$reject;
  });
  this.resolve = aFunction(resolve);
  this.reject  = aFunction(reject);
};
var perform = function(exec){
  try {
    exec();
  } catch(e){
    return {error: e};
  }
};
var notify = function(promise, isReject){
  if(promise._n)return;
  promise._n = true;
  var chain = promise._c;
  microtask(function(){
    var value = promise._v
      , ok    = promise._s == 1
      , i     = 0;
    var run = function(reaction){
      var handler = ok ? reaction.ok : reaction.fail
        , resolve = reaction.resolve
        , reject  = reaction.reject
        , domain  = reaction.domain
        , result, then;
      try {
        if(handler){
          if(!ok){
            if(promise._h == 2)onHandleUnhandled(promise);
            promise._h = 1;
          }
          if(handler === true)result = value;
          else {
            if(domain)domain.enter();
            result = handler(value);
            if(domain)domain.exit();
          }
          if(result === reaction.promise){
            reject(TypeError('Promise-chain cycle'));
          } else if(then = isThenable(result)){
            then.call(result, resolve, reject);
          } else resolve(result);
        } else reject(value);
      } catch(e){
        reject(e);
      }
    };
    while(chain.length > i)run(chain[i++]); // variable length - can't use forEach
    promise._c = [];
    promise._n = false;
    if(isReject && !promise._h)onUnhandled(promise);
  });
};
var onUnhandled = function(promise){
  task.call(global, function(){
    var value = promise._v
      , abrupt, handler, console;
    if(isUnhandled(promise)){
      abrupt = perform(function(){
        if(isNode){
          process.emit('unhandledRejection', value, promise);
        } else if(handler = global.onunhandledrejection){
          handler({promise: promise, reason: value});
        } else if((console = global.console) && console.error){
          console.error('Unhandled promise rejection', value);
        }
      });
      // Browsers should not trigger `rejectionHandled` event if it was handled here, NodeJS - should
      promise._h = isNode || isUnhandled(promise) ? 2 : 1;
    } promise._a = undefined;
    if(abrupt)throw abrupt.error;
  });
};
var isUnhandled = function(promise){
  if(promise._h == 1)return false;
  var chain = promise._a || promise._c
    , i     = 0
    , reaction;
  while(chain.length > i){
    reaction = chain[i++];
    if(reaction.fail || !isUnhandled(reaction.promise))return false;
  } return true;
};
var onHandleUnhandled = function(promise){
  task.call(global, function(){
    var handler;
    if(isNode){
      process.emit('rejectionHandled', promise);
    } else if(handler = global.onrejectionhandled){
      handler({promise: promise, reason: promise._v});
    }
  });
};
var $reject = function(value){
  var promise = this;
  if(promise._d)return;
  promise._d = true;
  promise = promise._w || promise; // unwrap
  promise._v = value;
  promise._s = 2;
  if(!promise._a)promise._a = promise._c.slice();
  notify(promise, true);
};
var $resolve = function(value){
  var promise = this
    , then;
  if(promise._d)return;
  promise._d = true;
  promise = promise._w || promise; // unwrap
  try {
    if(promise === value)throw TypeError("Promise can't be resolved itself");
    if(then = isThenable(value)){
      microtask(function(){
        var wrapper = {_w: promise, _d: false}; // wrap
        try {
          then.call(value, ctx($resolve, wrapper, 1), ctx($reject, wrapper, 1));
        } catch(e){
          $reject.call(wrapper, e);
        }
      });
    } else {
      promise._v = value;
      promise._s = 1;
      notify(promise, false);
    }
  } catch(e){
    $reject.call({_w: promise, _d: false}, e); // wrap
  }
};

// constructor polyfill
if(!USE_NATIVE){
  // 25.4.3.1 Promise(executor)
  $Promise = function Promise(executor){
    anInstance(this, $Promise, PROMISE, '_h');
    aFunction(executor);
    Internal.call(this);
    try {
      executor(ctx($resolve, this, 1), ctx($reject, this, 1));
    } catch(err){
      $reject.call(this, err);
    }
  };
  Internal = function Promise(executor){
    this._c = [];             // <- awaiting reactions
    this._a = undefined;      // <- checked in isUnhandled reactions
    this._s = 0;              // <- state
    this._d = false;          // <- done
    this._v = undefined;      // <- value
    this._h = 0;              // <- rejection state, 0 - default, 1 - handled, 2 - unhandled
    this._n = false;          // <- notify
  };
  Internal.prototype = require('./_redefine-all')($Promise.prototype, {
    // 25.4.5.3 Promise.prototype.then(onFulfilled, onRejected)
    then: function then(onFulfilled, onRejected){
      var reaction    = newPromiseCapability(speciesConstructor(this, $Promise));
      reaction.ok     = typeof onFulfilled == 'function' ? onFulfilled : true;
      reaction.fail   = typeof onRejected == 'function' && onRejected;
      reaction.domain = isNode ? process.domain : undefined;
      this._c.push(reaction);
      if(this._a)this._a.push(reaction);
      if(this._s)notify(this, false);
      return reaction.promise;
    },
    // 25.4.5.1 Promise.prototype.catch(onRejected)
    'catch': function(onRejected){
      return this.then(undefined, onRejected);
    }
  });
  PromiseCapability = function(){
    var promise  = new Internal;
    this.promise = promise;
    this.resolve = ctx($resolve, promise, 1);
    this.reject  = ctx($reject, promise, 1);
  };
}

$export($export.G + $export.W + $export.F * !USE_NATIVE, {Promise: $Promise});
require('./_set-to-string-tag')($Promise, PROMISE);
require('./_set-species')(PROMISE);
Wrapper = require('./_core')[PROMISE];

// statics
$export($export.S + $export.F * !USE_NATIVE, PROMISE, {
  // 25.4.4.5 Promise.reject(r)
  reject: function reject(r){
    var capability = newPromiseCapability(this)
      , $$reject   = capability.reject;
    $$reject(r);
    return capability.promise;
  }
});
$export($export.S + $export.F * (LIBRARY || !USE_NATIVE), PROMISE, {
  // 25.4.4.6 Promise.resolve(x)
  resolve: function resolve(x){
    // instanceof instead of internal slot check because we should fix it without replacement native Promise core
    if(x instanceof $Promise && sameConstructor(x.constructor, this))return x;
    var capability = newPromiseCapability(this)
      , $$resolve  = capability.resolve;
    $$resolve(x);
    return capability.promise;
  }
});
$export($export.S + $export.F * !(USE_NATIVE && require('./_iter-detect')(function(iter){
  $Promise.all(iter)['catch'](empty);
})), PROMISE, {
  // 25.4.4.1 Promise.all(iterable)
  all: function all(iterable){
    var C          = this
      , capability = newPromiseCapability(C)
      , resolve    = capability.resolve
      , reject     = capability.reject;
    var abrupt = perform(function(){
      var values    = []
        , index     = 0
        , remaining = 1;
      forOf(iterable, false, function(promise){
        var $index        = index++
          , alreadyCalled = false;
        values.push(undefined);
        remaining++;
        C.resolve(promise).then(function(value){
          if(alreadyCalled)return;
          alreadyCalled  = true;
          values[$index] = value;
          --remaining || resolve(values);
        }, reject);
      });
      --remaining || resolve(values);
    });
    if(abrupt)reject(abrupt.error);
    return capability.promise;
  },
  // 25.4.4.4 Promise.race(iterable)
  race: function race(iterable){
    var C          = this
      , capability = newPromiseCapability(C)
      , reject     = capability.reject;
    var abrupt = perform(function(){
      forOf(iterable, false, function(promise){
        C.resolve(promise).then(capability.resolve, reject);
      });
    });
    if(abrupt)reject(abrupt.error);
    return capability.promise;
  }
});
},{"./_a-function":14,"./_an-instance":16,"./_classof":19,"./_core":21,"./_ctx":22,"./_export":27,"./_for-of":29,"./_global":30,"./_is-object":38,"./_iter-detect":42,"./_library":45,"./_microtask":46,"./_redefine-all":58,"./_set-species":60,"./_set-to-string-tag":61,"./_species-constructor":64,"./_task":66,"./_wks":74}],81:[function(require,module,exports){
'use strict';
var $at  = require('./_string-at')(true);

// 21.1.3.27 String.prototype[@@iterator]()
require('./_iter-define')(String, 'String', function(iterated){
  this._t = String(iterated); // target
  this._i = 0;                // next index
// 21.1.5.2.1 %StringIteratorPrototype%.next()
}, function(){
  var O     = this._t
    , index = this._i
    , point;
  if(index >= O.length)return {value: undefined, done: true};
  point = $at(O, index);
  this._i += point.length;
  return {value: point, done: false};
});
},{"./_iter-define":41,"./_string-at":65}],82:[function(require,module,exports){
require('./es6.array.iterator');
var global        = require('./_global')
  , hide          = require('./_hide')
  , Iterators     = require('./_iterators')
  , TO_STRING_TAG = require('./_wks')('toStringTag');

for(var collections = ['NodeList', 'DOMTokenList', 'MediaList', 'StyleSheetList', 'CSSRuleList'], i = 0; i < 5; i++){
  var NAME       = collections[i]
    , Collection = global[NAME]
    , proto      = Collection && Collection.prototype;
  if(proto && !proto[TO_STRING_TAG])hide(proto, TO_STRING_TAG, NAME);
  Iterators[NAME] = Iterators.Array;
}
},{"./_global":30,"./_hide":32,"./_iterators":44,"./_wks":74,"./es6.array.iterator":76}],83:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _extends2 = require("babel-runtime/helpers/extends");

var _extends3 = _interopRequireDefault(_extends2);

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
    this._options = (0, _extends3.default)({}, defaults, options);
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

},{"./adapters/base":85,"./collection":86,"babel-runtime/helpers/extends":8}],84:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _asyncToGenerator2 = require("babel-runtime/helpers/asyncToGenerator");

var _asyncToGenerator3 = _interopRequireDefault(_asyncToGenerator2);

var _promise = require("babel-runtime/core-js/promise");

var _promise2 = _interopRequireDefault(_promise);

var _keys = require("babel-runtime/core-js/object/keys");

var _keys2 = _interopRequireDefault(_keys);

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
  const filteredFields = (0, _keys2.default)(filters);
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
      return _promise2.default.resolve(this);
    }
    return new _promise2.default((resolve, reject) => {
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
    var _this = this;

    return (0, _asyncToGenerator3.default)(function* () {
      try {
        yield _this.open();
        return new _promise2.default(function (resolve, reject) {
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

    return (0, _asyncToGenerator3.default)(function* () {
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
      return new _promise2.default(function (resolve, reject) {
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
          if (result instanceof _promise2.default) {
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

    return (0, _asyncToGenerator3.default)(function* () {
      try {
        yield _this3.open();
        return new _promise2.default(function (resolve, reject) {
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

    return (0, _asyncToGenerator3.default)(function* () {
      const { filters } = params;
      const indexField = findIndexedField(filters);
      const value = filters[indexField];
      try {
        yield _this4.open();
        const results = yield new _promise2.default(function (resolve, reject) {
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

    return (0, _asyncToGenerator3.default)(function* () {
      const value = parseInt(lastModified, 10) || null;
      yield _this5.open();
      return new _promise2.default(function (resolve, reject) {
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

    return (0, _asyncToGenerator3.default)(function* () {
      yield _this6.open();
      return new _promise2.default(function (resolve, reject) {
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

    return (0, _asyncToGenerator3.default)(function* () {
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

},{"../utils":87,"./base.js":85,"babel-runtime/core-js/object/keys":5,"babel-runtime/core-js/promise":6,"babel-runtime/helpers/asyncToGenerator":7}],85:[function(require,module,exports){
"use strict";

/**
 * Base db adapter.
 *
 * @abstract
 */

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _promise = require("babel-runtime/core-js/promise");

var _promise2 = _interopRequireDefault(_promise);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

class BaseAdapter {
  /**
   * Opens a connection to the database.
   *
   * @abstract
   * @return {Promise}
   */
  open() {
    return _promise2.default.resolve();
  }

  /**
   * Closes current connection to the database.
   *
   * @abstract
   * @return {Promise}
   */
  close() {
    return _promise2.default.resolve();
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

},{"babel-runtime/core-js/promise":6}],86:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.CollectionTransaction = exports.SyncResultObject = undefined;

var _stringify = require("babel-runtime/core-js/json/stringify");

var _stringify2 = _interopRequireDefault(_stringify);

var _promise = require("babel-runtime/core-js/promise");

var _promise2 = _interopRequireDefault(_promise);

var _asyncToGenerator2 = require("babel-runtime/helpers/asyncToGenerator");

var _asyncToGenerator3 = _interopRequireDefault(_asyncToGenerator2);

var _extends2 = require("babel-runtime/helpers/extends");

var _extends3 = _interopRequireDefault(_extends2);

var _assign = require("babel-runtime/core-js/object/assign");

var _assign2 = _interopRequireDefault(_assign);

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
    (0, _assign2.default)(this, SyncResultObject.defaults);
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
  return (0, _extends3.default)({}, record, { _status: status });
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
  const synced = (0, _extends3.default)({}, local, markSynced(remote));
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
    var _this = this;

    return (0, _asyncToGenerator3.default)(function* () {
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
    if (!this[`${ type }Transformers`].length) {
      return _promise2.default.resolve(record);
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
      return _promise2.default.resolve(record);
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
    const reject = msg => _promise2.default.reject(new Error(msg));
    if (typeof record !== "object") {
      return reject("Record is not an object.");
    }
    if ((options.synced || options.useRecordId) && !record.hasOwnProperty("id")) {
      return reject("Missing required Id; synced and useRecordId options require one");
    }
    if (!options.synced && !options.useRecordId && record.hasOwnProperty("id")) {
      return reject("Extraneous Id; can't create a record having one set.");
    }
    const newRecord = (0, _extends3.default)({}, record, {
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
      return _promise2.default.reject(new Error("Record is not an object."));
    }
    if (!record.hasOwnProperty("id")) {
      return _promise2.default.reject(new Error("Cannot update a record missing id."));
    }
    if (!this.idSchema.validate(record.id)) {
      return _promise2.default.reject(new Error(`Invalid Id: ${ record.id }`));
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
      return _promise2.default.reject(new Error("Record is not an object."));
    }
    if (!record.hasOwnProperty("id")) {
      return _promise2.default.reject(new Error("Cannot update a record missing id."));
    }
    if (!this.idSchema.validate(record.id)) {
      return _promise2.default.reject(new Error(`Invalid Id: ${ record.id }`));
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

    return (0, _asyncToGenerator3.default)(function* () {
      params = (0, _extends3.default)({ order: "-last_modified", filters: {} }, params);
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

    return (0, _asyncToGenerator3.default)(function* () {
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

    return (0, _asyncToGenerator3.default)(function* () {
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
   * @return {Promise}
   */
  _handleConflicts(transaction, conflicts, strategy) {
    if (strategy === Collection.strategy.MANUAL) {
      return [];
    }
    return conflicts.map(conflict => {
      const resolution = strategy === Collection.strategy.CLIENT_WINS ? conflict.local : conflict.remote;
      const updated = this._resolveRaw(conflict, resolution);
      transaction.update(updated);
      return updated;
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
        return _promise2.default.reject(Error(`Invalid Id: ${ id }`));
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

    return (0, _asyncToGenerator3.default)(function* () {
      const unsynced = yield _this5.list({ filters: { _status: ["deleted", "synced"] }, order: "" }, { includeDeleted: true });
      yield _this5.db.execute(function (transaction) {
        unsynced.data.forEach(function (record) {
          if (record._status === "deleted") {
            // Garbage collect deleted records.
            transaction.delete(record.id);
          } else {
            // Records that were synced become «created».
            transaction.update((0, _extends3.default)({}, record, {
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

    return (0, _asyncToGenerator3.default)(function* () {
      const unsynced = yield _this6.list({ filters: { _status: ["created", "updated"] }, order: "" });
      const deleted = yield _this6.list({ filters: { _status: "deleted" }, order: "" }, { includeDeleted: true });

      const toSync = yield _promise2.default.all(unsynced.data.map(_this6._encodeRecord.bind(_this6, "remote")));
      const toDelete = yield _promise2.default.all(deleted.data.map(_this6._encodeRecord.bind(_this6, "remote")));

      return { toSync, toDelete };
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

    return (0, _asyncToGenerator3.default)(function* () {
      if (!syncResultObject.ok) {
        return syncResultObject;
      }

      const since = _this7.lastModified ? _this7.lastModified : yield _this7.db.getLastModified();

      options = (0, _extends3.default)({
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
        since: options.lastModified ? `${ options.lastModified }` : undefined,
        headers: options.headers,
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
      const decodedChanges = yield _promise2.default.all(data.map(function (change) {
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
      return _promise2.default.resolve(payload);
    }
    return (0, _utils.waterfall)(this.hooks[hookName].map(hook => {
      return record => {
        const result = hook(payload, this);
        const resultThenable = result && typeof result.then === "function";
        const resultChanges = result && result.hasOwnProperty("changes");
        if (!(resultThenable || resultChanges)) {
          throw new Error(`Invalid return value for hook: ${ (0, _stringify2.default)(result) } has no 'then()' or 'changes' properties`);
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
  pushChanges(client, { toDelete = [], toSync }, syncResultObject, options = {}) {
    var _this8 = this;

    return (0, _asyncToGenerator3.default)(function* () {
      if (!syncResultObject.ok) {
        return syncResultObject;
      }
      const safe = !options.strategy || options.strategy !== Collection.CLIENT_WINS;

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
      }, { headers: options.headers, safe, aggregate: true });

      // Store outgoing errors into sync result object
      syncResultObject.add("errors", synced.errors.map(function (e) {
        return (0, _extends3.default)({}, e, { type: "outgoing" });
      }));

      // Store outgoing conflicts into sync result object
      const conflicts = [];
      for (let { type, local, remote } of synced.conflicts) {
        // Note: we ensure that local data are actually available, as they may
        // be missing in the case of a published deletion.
        const safeLocal = local && local.data || { id: remote.id };
        const realLocal = yield _this8._decodeRecord("remote", safeLocal);
        const realRemote = yield _this8._decodeRecord("remote", remote);
        const conflict = { type, local: realLocal, remote: realRemote };
        conflicts.push(conflict);
      }
      syncResultObject.add("conflicts", conflicts);

      // Records that must be deleted are either deletions that were pushed
      // to server (published) or deleted records that were never pushed (skipped).
      const missingRemotely = synced.skipped.map(function (r) {
        return (0, _extends3.default)({}, r, { deleted: true });
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
      const decoded = yield _promise2.default.all(toApplyLocally.map(function (record) {
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
    const resolved = (0, _extends3.default)({}, resolution, {
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
    var _this9 = this;

    return (0, _asyncToGenerator3.default)(function* () {
      const previousRemote = _this9.api.remote;
      if (options.remote) {
        // Note: setting the remote ensures it's valid, throws when invalid.
        _this9.api.remote = options.remote;
      }
      if (!options.ignoreBackoff && _this9.api.backoff > 0) {
        const seconds = Math.ceil(_this9.api.backoff / 1000);
        return _promise2.default.reject(new Error(`Server is asking clients to back off; retry in ${ seconds }s or use the ignoreBackoff option.`));
      }

      const client = _this9.api.bucket(options.bucket || _this9.bucket).collection(options.collection || _this9.name);

      const result = new SyncResultObject();
      try {
        // Fetch last changes from the server.
        yield _this9.pullChanges(client, result, options);
        const { lastModified } = result;

        // Fetch local changes
        const { toDelete, toSync } = yield _this9.gatherLocalChanges();

        // Publish local changes and pull local resolutions
        yield _this9.pushChanges(client, { toDelete, toSync }, result, options);

        // Publish local resolution of push conflicts to server (on CLIENT_WINS)
        const resolvedUnsynced = result.resolved.filter(function (r) {
          return r._status !== "synced";
        });
        if (resolvedUnsynced.length > 0) {
          const resolvedEncoded = yield _promise2.default.all(resolvedUnsynced.map(_this9._encodeRecord.bind(_this9, "remote")));
          yield _this9.pushChanges(client, { toSync: resolvedEncoded }, result, options);
        }
        // Perform a last pull to catch changes that occured after the last pull,
        // while local changes were pushed. Do not do it nothing was pushed.
        if (result.published.length > 0) {
          // Avoid redownloading our own changes during the last pull.
          const pullOpts = (0, _extends3.default)({}, options, { lastModified, exclude: result.published });
          yield _this9.pullChanges(client, result, pullOpts);
        }

        // Don't persist lastModified value if any conflict or error occured
        if (result.ok) {
          // No conflict occured, persist collection's lastModified value
          _this9._lastModified = yield _this9.db.saveLastModified(result.lastModified);
        }
      } finally {
        // Ensure API default remote is reverted if a custom one's been used
        _this9.api.remote = previousRemote;
      }
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

    return (0, _asyncToGenerator3.default)(function* () {
      if (!Array.isArray(records)) {
        throw new Error("Records is not an array.");
      }

      for (let record of records) {
        if (!record.hasOwnProperty("id") || !_this10.idSchema.validate(record.id)) {
          throw new Error("Record has invalid ID: " + (0, _stringify2.default)(record));
        }

        if (!record.last_modified) {
          throw new Error("Record has no last_modified value: " + (0, _stringify2.default)(record));
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
    for (let { action, payload } of this._events) {
      this.collection.events.emit(action, payload);
    }
    if (this._events.length > 0) {
      const targets = this._events.map(({ action, payload }) => (0, _extends3.default)({ action }, payload));
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
    return { data: (0, _extends3.default)({ id }, existing), deleted: !!existing, permissions: {} };
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
    const newRecord = options.patch ? (0, _extends3.default)({}, oldRecord, record) : record;
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
    const updated = (0, _extends3.default)({}, newRecord);
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

},{"./adapters/IDB":84,"./adapters/base":85,"./utils":87,"babel-runtime/core-js/json/stringify":3,"babel-runtime/core-js/object/assign":4,"babel-runtime/core-js/promise":6,"babel-runtime/helpers/asyncToGenerator":7,"babel-runtime/helpers/extends":8,"uuid":9}],87:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.RE_UUID = undefined;

var _promise = require("babel-runtime/core-js/promise");

var _promise2 = _interopRequireDefault(_promise);

var _keys = require("babel-runtime/core-js/object/keys");

var _keys2 = _interopRequireDefault(_keys);

exports.sortObjects = sortObjects;
exports.filterObject = filterObject;
exports.filterObjects = filterObjects;
exports.isUUID = isUUID;
exports.waterfall = waterfall;
exports.deepEqual = deepEqual;
exports.omitKeys = omitKeys;

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

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
  return (0, _keys2.default)(filters).every(filter => {
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
    return _promise2.default.resolve(init);
  }
  return fns.reduce((promise, nextFn) => {
    return promise.then(nextFn);
  }, _promise2.default.resolve(init));
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
  if ((0, _keys2.default)(a).length !== (0, _keys2.default)(b).length) {
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
  return (0, _keys2.default)(obj).reduce((acc, key) => {
    if (keys.indexOf(key) === -1) {
      acc[key] = obj[key];
    }
    return acc;
  }, {});
}

},{"babel-runtime/core-js/object/keys":5,"babel-runtime/core-js/promise":6}]},{},[2])(2)
});