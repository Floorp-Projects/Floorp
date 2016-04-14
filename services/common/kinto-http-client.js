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
 * This file is generated from kinto-client.js - do not modify directly.
 */

this.EXPORTED_SYMBOLS = ["KintoHttpClient"];

/*
 * Version 0.6.0 - 6b6c736
 */

(function(f){if(typeof exports==="object"&&typeof module!=="undefined"){module.exports=f()}else if(typeof define==="function"&&define.amd){define([],f)}else{var g;if(typeof window!=="undefined"){g=window}else if(typeof global!=="undefined"){g=global}else if(typeof self!=="undefined"){g=self}else{g=this}g.KintoHttpClient = f()}})(function(){var define,module,exports;return (function e(t,n,r){function s(o,u){if(!n[o]){if(!t[o]){var a=typeof require=="function"&&require;if(!u&&a)return a(o,!0);if(i)return i(o,!0);var f=new Error("Cannot find module '"+o+"'");throw f.code="MODULE_NOT_FOUND",f}var l=n[o]={exports:{}};t[o][0].call(l.exports,function(e){var n=t[o][1][e];return s(n?n:e)},l,l.exports,e,t,n,r)}return n[o].exports}var i=typeof require=="function"&&require;for(var o=0;o<r.length;o++)s(r[o]);return s})({1:[function(require,module,exports){
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
exports.default = undefined;

var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

var _base = require("../src/base");

var _base2 = _interopRequireDefault(_base);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const Cu = Components.utils;

Cu.import("resource://gre/modules/Timer.jsm");
Cu.importGlobalProperties(['fetch']);
const { EventEmitter } = Cu.import("resource://devtools/shared/event-emitter.js", {});

let KintoHttpClient = class KintoHttpClient extends _base2.default {
  constructor(remote, options = {}) {
    const events = {};
    EventEmitter.decorate(events);
    super(remote, _extends({ events }, options));
  }
};

// This fixes compatibility with CommonJS required by browserify.
// See http://stackoverflow.com/questions/33505992/babel-6-changes-how-it-exports-default/33683495#33683495

exports.default = KintoHttpClient;
if (typeof module === "object") {
  module.exports = KintoHttpClient;
}

},{"../src/base":2}],2:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = exports.SUPPORTED_PROTOCOL_VERSION = undefined;

var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

var _dec, _dec2, _dec3, _dec4, _dec5, _dec6, _desc, _value, _class;

var _utils = require("./utils");

var _http = require("./http");

var _http2 = _interopRequireDefault(_http);

var _endpoint = require("./endpoint");

var _endpoint2 = _interopRequireDefault(_endpoint);

var _requests = require("./requests");

var requests = _interopRequireWildcard(_requests);

var _batch = require("./batch");

var _bucket2 = require("./bucket");

var _bucket3 = _interopRequireDefault(_bucket2);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _applyDecoratedDescriptor(target, property, decorators, descriptor, context) {
  var desc = {};
  Object['ke' + 'ys'](descriptor).forEach(function (key) {
    desc[key] = descriptor[key];
  });
  desc.enumerable = !!desc.enumerable;
  desc.configurable = !!desc.configurable;

  if ('value' in desc || desc.initializer) {
    desc.writable = true;
  }

  desc = decorators.slice().reverse().reduce(function (desc, decorator) {
    return decorator(target, property, desc) || desc;
  }, desc);

  if (context && desc.initializer !== void 0) {
    desc.value = desc.initializer ? desc.initializer.call(context) : void 0;
    desc.initializer = undefined;
  }

  if (desc.initializer === void 0) {
    Object['define' + 'Property'](target, property, desc);
    desc = null;
  }

  return desc;
}

/**
 * Currently supported protocol version.
 * @type {String}
 */
const SUPPORTED_PROTOCOL_VERSION = exports.SUPPORTED_PROTOCOL_VERSION = "v1";

/**
 * High level HTTP client for the Kinto API.
 *
 * @example
 * const client = new KintoClient("https://kinto.dev.mozaws.net/v1");
 * client.bucket("default")
*    .collection("my-blog")
*    .createRecord({title: "First article"})
 *   .then(console.log.bind(console))
 *   .catch(console.error.bind(console));
 */
let KintoClientBase = (_dec = (0, _utils.nobatch)("This operation is not supported within a batch operation."), _dec2 = (0, _utils.nobatch)("This operation is not supported within a batch operation."), _dec3 = (0, _utils.nobatch)("This operation is not supported within a batch operation."), _dec4 = (0, _utils.nobatch)("This operation is not supported within a batch operation."), _dec5 = (0, _utils.nobatch)("Can't use batch within a batch!"), _dec6 = (0, _utils.support)("1.4", "2.0"), (_class = class KintoClientBase {
  /**
   * Constructor.
   *
   * @param  {String} remote  The remote URL.
   * @param  {Object}  options The options object.
   * @param  {Boolean} options.safe        Adds concurrency headers to every
   * requests (default: `true`).
   * @param  {EventEmitter} options.events The events handler. If none provided
   * an `EventEmitter` instance will be created.
   * @param  {Object}  options.headers     The key-value headers to pass to each
   * request (default: `{}`).
   * @param  {String}  options.bucket      The default bucket to use (default:
   * `"default"`)
   * @param  {String}  options.requestMode The HTTP request mode (from ES6 fetch
   * spec).
   */
  constructor(remote, options = {}) {
    if (typeof remote !== "string" || !remote.length) {
      throw new Error("Invalid remote URL: " + remote);
    }
    if (remote[remote.length - 1] === "/") {
      remote = remote.slice(0, -1);
    }
    this._backoffReleaseTime = null;

    /**
     * Default request options container.
     * @private
     * @type {Object}
     */
    this.defaultReqOptions = {
      bucket: options.bucket || "default",
      headers: options.headers || {},
      safe: !!options.safe
    };

    this._options = options;
    this._requests = [];
    this._isBatch = !!options.batch;

    // public properties
    /**
     * The remote server base URL.
     * @type {String}
     */
    this.remote = remote;
    /**
     * Current server information.
     * @ignore
     * @type {Object|null}
     */
    this.serverInfo = null;
    /**
     * The event emitter instance. Should comply with the `EventEmitter`
     * interface.
     * @ignore
     * @type {Class}
     */
    this.events = options.events;

    /**
     * The HTTP instance.
     * @ignore
     * @type {HTTP}
     */
    this.http = new _http2.default(this.events, { requestMode: options.requestMode });
    this._registerHTTPEvents();
  }

  /**
   * The remote endpoint base URL. Setting the value will also extract and
   * validate the version.
   * @type {String}
   */
  get remote() {
    return this._remote;
  }

  /**
   * @ignore
   */
  set remote(url) {
    let version;
    try {
      version = url.match(/\/(v\d+)\/?$/)[1];
    } catch (err) {
      throw new Error("The remote URL must contain the version: " + url);
    }
    if (version !== SUPPORTED_PROTOCOL_VERSION) {
      throw new Error(`Unsupported protocol version: ${ version }`);
    }
    this._remote = url;
    this._version = version;
  }

  /**
   * The current server protocol version, eg. `v1`.
   * @type {String}
   */
  get version() {
    return this._version;
  }

  /**
   * Backoff remaining time, in milliseconds. Defaults to zero if no backoff is
   * ongoing.
   *
   * @type {Number}
   */
  get backoff() {
    const currentTime = new Date().getTime();
    if (this._backoffReleaseTime && currentTime < this._backoffReleaseTime) {
      return this._backoffReleaseTime - currentTime;
    }
    return 0;
  }

  /**
   * Registers HTTP events.
   * @private
   */
  _registerHTTPEvents() {
    this.events.on("backoff", backoffMs => {
      this._backoffReleaseTime = backoffMs;
    });
  }

  /**
   * Retrieve a bucket object to perform operations on it.
   *
   * @param  {String}  name    The bucket name.
   * @param  {Object}  options The request options.
   * @param  {Boolean} safe    The resulting safe option.
   * @param  {String}  bucket  The resulting bucket name option.
   * @param  {Object}  headers The extended headers object option.
   * @return {Bucket}
   */
  bucket(name, options = {}) {
    const bucketOptions = (0, _utils.omit)(this._getRequestOptions(options), "bucket");
    return new _bucket3.default(this, name, bucketOptions);
  }

  /**
   * Generates a request options object, deeply merging the client configured
   * defaults with the ones provided as argument.
   *
   * Note: Headers won't be overriden but merged with instance default ones.
   *
   * @private
   * @param    {Object} options The request options.
   * @return   {Object}
   * @property {Boolean} safe    The resulting safe option.
   * @property {String}  bucket  The resulting bucket name option.
   * @property {Object}  headers The extended headers object option.
   */
  _getRequestOptions(options = {}) {
    return _extends({}, this.defaultReqOptions, options, {
      batch: this._isBatch,
      // Note: headers should never be overriden but extended
      headers: _extends({}, this.defaultReqOptions.headers, options.headers)
    });
  }

  /**
   * Retrieves server information and persist them locally. This operation is
   * usually performed a single time during the instance lifecycle.
   *
   * @return {Promise<Object, Error>}
   */
  fetchServerInfo() {
    if (this.serverInfo) {
      return Promise.resolve(this.serverInfo);
    }
    return this.http.request(this.remote + (0, _endpoint2.default)("root"), {
      headers: this.defaultReqOptions.headers
    }).then(({ json }) => {
      this.serverInfo = json;
      return this.serverInfo;
    });
  }

  /**
   * Retrieves Kinto server settings.
   *
   * @return {Promise<Object, Error>}
   */

  fetchServerSettings() {
    return this.fetchServerInfo().then(({ settings }) => settings);
  }

  /**
   * Retrieve server capabilities information.
   *
   * @return {Promise<Object, Error>}
   */

  fetchServerCapabilities() {
    return this.fetchServerInfo().then(({ capabilities }) => capabilities);
  }

  /**
   * Retrieve authenticated user information.
   *
   * @return {Promise<Object, Error>}
   */

  fetchUser() {
    return this.fetchServerInfo().then(({ user }) => user);
  }

  /**
   * Retrieve authenticated user information.
   *
   * @return {Promise<Object, Error>}
   */

  fetchHTTPApiVersion() {
    return this.fetchServerInfo().then(({ http_api_version }) => {
      return http_api_version;
    });
  }

  /**
   * Process batch requests, chunking them according to the batch_max_requests
   * server setting when needed.
   *
   * @param  {Array}  requests The list of batch subrequests to perform.
   * @param  {Object} options  The options object.
   * @return {Promise<Object, Error>}
   */
  _batchRequests(requests, options = {}) {
    const headers = _extends({}, this.defaultReqOptions.headers, options.headers);
    if (!requests.length) {
      return Promise.resolve([]);
    }
    return this.fetchServerSettings().then(serverSettings => {
      const maxRequests = serverSettings["batch_max_requests"];
      if (maxRequests && requests.length > maxRequests) {
        const chunks = (0, _utils.partition)(requests, maxRequests);
        return (0, _utils.pMap)(chunks, chunk => this._batchRequests(chunk, options));
      }
      return this.execute({
        path: (0, _endpoint2.default)("batch"),
        method: "POST",
        headers: headers,
        body: {
          defaults: { headers },
          requests: requests
        }
      })
      // we only care about the responses
      .then(({ responses }) => responses);
    });
  }

  /**
   * Sends batch requests to the remote server.
   *
   * Note: Reserved for internal use only.
   *
   * @ignore
   * @param  {Function} fn      The function to use for describing batch ops.
   * @param  {Object}   options The options object.
   * @param  {Boolean}  options.safe      The safe option.
   * @param  {String}   options.bucket    The bucket name option.
   * @param  {Object}   options.headers   The headers object option.
   * @param  {Boolean}  options.aggregate Produces an aggregated result object
   * (default: `false`).
   * @return {Promise<Object, Error>}
   */

  batch(fn, options = {}) {
    const rootBatch = new KintoClientBase(this.remote, _extends({}, this._options, this._getRequestOptions(options), {
      batch: true
    }));
    let bucketBatch, collBatch;
    if (options.bucket) {
      bucketBatch = rootBatch.bucket(options.bucket);
      if (options.collection) {
        collBatch = bucketBatch.collection(options.collection);
      }
    }
    const batchClient = collBatch || bucketBatch || rootBatch;
    try {
      fn(batchClient);
    } catch (err) {
      return Promise.reject(err);
    }
    return this._batchRequests(rootBatch._requests, options).then(responses => {
      if (options.aggregate) {
        return (0, _batch.aggregate)(responses, rootBatch._requests);
      }
      return responses;
    });
  }

  /**
   * Executes an atomic HTTP request.
   *
   * @private
   * @param  {Object}  request     The request object.
   * @param  {Object}  options     The options object.
   * @param  {Boolean} options.raw Resolve with full response object, including
   * json body and headers (Default: `false`, so only the json body is
   * retrieved).
   * @return {Promise<Object, Error>}
   */
  execute(request, options = { raw: false }) {
    // If we're within a batch, add the request to the stack to send at once.
    if (this._isBatch) {
      this._requests.push(request);
      // Resolve with a message in case people attempt at consuming the result
      // from within a batch operation.
      const msg = "This result is generated from within a batch " + "operation and should not be consumed.";
      return Promise.resolve(options.raw ? { json: msg } : msg);
    }
    const promise = this.fetchServerSettings().then(_ => {
      return this.http.request(this.remote + request.path, _extends({}, request, {
        body: JSON.stringify(request.body)
      }));
    });
    return options.raw ? promise : promise.then(({ json }) => json);
  }

  /**
   * Retrieves the list of buckets.
   *
   * @param  {Object} options         The options object.
   * @param  {Object} options.headers The headers object option.
   * @return {Promise<Object[], Error>}
   */
  listBuckets(options = {}) {
    return this.execute({
      path: (0, _endpoint2.default)("buckets"),
      headers: _extends({}, this.defaultReqOptions.headers, options.headers)
    });
  }

  /**
   * Creates a new bucket on the server.
   *
   * @param  {String}   bucketName      The bucket name.
   * @param  {Object}   options         The options object.
   * @param  {Boolean}  options.safe    The safe option.
   * @param  {Object}   options.headers The headers object option.
   * @return {Promise<Object, Error>}
   */
  createBucket(bucketName, options = {}) {
    const reqOptions = this._getRequestOptions(options);
    return this.execute(requests.createBucket(bucketName, reqOptions));
  }

  /**
   * Deletes a bucket from the server.
   *
   * @ignore
   * @param  {Object|String} bucket          The bucket to delete.
   * @param  {Object}        options         The options object.
   * @param  {Boolean}       options.safe    The safe option.
   * @param  {Object}        options.headers The headers object option.
   * @return {Promise<Object, Error>}
   */
  deleteBucket(bucket, options = {}) {
    const _bucket = typeof bucket === "object" ? bucket : { id: bucket };
    const reqOptions = this._getRequestOptions(options);
    return this.execute(requests.deleteBucket(_bucket, reqOptions));
  }

  /**
   * Deletes all buckets on the server.
   *
   * @ignore
   * @param  {Object}  options         The options object.
   * @param  {Boolean} options.safe    The safe option.
   * @param  {Object}  options.headers The headers object option.
   * @return {Promise<Object, Error>}
   */

  deleteBuckets(options = {}) {
    const reqOptions = this._getRequestOptions(options);
    return this.execute(requests.deleteBuckets(reqOptions));
  }
}, (_applyDecoratedDescriptor(_class.prototype, "fetchServerSettings", [_dec], Object.getOwnPropertyDescriptor(_class.prototype, "fetchServerSettings"), _class.prototype), _applyDecoratedDescriptor(_class.prototype, "fetchServerCapabilities", [_dec2], Object.getOwnPropertyDescriptor(_class.prototype, "fetchServerCapabilities"), _class.prototype), _applyDecoratedDescriptor(_class.prototype, "fetchUser", [_dec3], Object.getOwnPropertyDescriptor(_class.prototype, "fetchUser"), _class.prototype), _applyDecoratedDescriptor(_class.prototype, "fetchHTTPApiVersion", [_dec4], Object.getOwnPropertyDescriptor(_class.prototype, "fetchHTTPApiVersion"), _class.prototype), _applyDecoratedDescriptor(_class.prototype, "batch", [_dec5], Object.getOwnPropertyDescriptor(_class.prototype, "batch"), _class.prototype), _applyDecoratedDescriptor(_class.prototype, "deleteBuckets", [_dec6], Object.getOwnPropertyDescriptor(_class.prototype, "deleteBuckets"), _class.prototype)), _class));
exports.default = KintoClientBase;

},{"./batch":3,"./bucket":4,"./endpoint":6,"./http":8,"./requests":9,"./utils":10}],3:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.aggregate = aggregate;
/**
 * Exports batch responses as a result object.
 *
 * @private
 * @param  {Array} responses The batch subrequest responses.
 * @param  {Array} requests  The initial issued requests.
 * @return {Object}
 */
function aggregate(responses = [], requests = []) {
  if (responses.length !== requests.length) {
    throw new Error("Responses length should match requests one.");
  }
  const results = {
    errors: [],
    published: [],
    conflicts: [],
    skipped: []
  };
  return responses.reduce((acc, response, index) => {
    const { status } = response;
    if (status >= 200 && status < 400) {
      acc.published.push(response.body);
    } else if (status === 404) {
      acc.skipped.push(response.body);
    } else if (status === 412) {
      acc.conflicts.push({
        // XXX: specifying the type is probably superfluous
        type: "outgoing",
        local: requests[index].body,
        remote: response.body.details && response.body.details.existing || null
      });
    } else {
      acc.errors.push({
        path: response.path,
        sent: requests[index],
        error: response.body
      });
    }
    return acc;
  }, results);
}

},{}],4:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = undefined;

var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

var _utils = require("./utils");

var _collection = require("./collection");

var _collection2 = _interopRequireDefault(_collection);

var _requests = require("./requests");

var requests = _interopRequireWildcard(_requests);

var _endpoint = require("./endpoint");

var _endpoint2 = _interopRequireDefault(_endpoint);

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * Abstract representation of a selected bucket.
 *
 */
let Bucket = class Bucket {
  /**
   * Constructor.
   *
   * @param  {KintoClient} client          The client instance.
   * @param  {String}      name            The bucket name.
   * @param  {Object}      options.headers The headers object option.
   * @param  {Boolean}     options.safe    The safe option.
   */
  constructor(client, name, options = {}) {
    /**
     * @ignore
     */
    this.client = client;
    /**
     * The bucket name.
     * @type {String}
     */
    this.name = name;
    /**
     * The default options object.
     * @ignore
     * @type {Object}
     */
    this.options = options;
    /**
     * @ignore
     */
    this._isBatch = !!options.batch;
  }

  /**
   * Merges passed request options with default bucket ones, if any.
   *
   * @private
   * @param  {Object} options The options to merge.
   * @return {Object}         The merged options.
   */
  _bucketOptions(options = {}) {
    const headers = _extends({}, this.options && this.options.headers, options.headers);
    return _extends({}, this.options, options, {
      headers,
      bucket: this.name,
      batch: this._isBatch
    });
  }

  /**
   * Selects a collection.
   *
   * @param  {String} name            The collection name.
   * @param  {Object} options         The options object.
   * @param  {Object} options.headers The headers object option.
   * @param  {Boolean}  options.safe  The safe option.
   * @return {Collection}
   */
  collection(name, options) {
    return new _collection2.default(this.client, this, name, this._bucketOptions(options));
  }

  /**
   * Retrieves bucket properties.
   *
   * @param  {Object} options         The options object.
   * @param  {Object} options.headers The headers object option.
   * @return {Promise<Object, Error>}
   */
  getAttributes(options = {}) {
    return this.client.execute({
      path: (0, _endpoint2.default)("bucket", this.name),
      headers: _extends({}, this.options.headers, options.headers)
    });
  }

  /**
   * Retrieves the list of collections in the current bucket.
   *
   * @param  {Object} options         The options object.
   * @param  {Object} options.headers The headers object option.
   * @return {Promise<Array<Object>, Error>}
   */
  listCollections(options = {}) {
    return this.client.execute({
      path: (0, _endpoint2.default)("collections", this.name),
      headers: _extends({}, this.options.headers, options.headers)
    });
  }

  /**
   * Creates a new collection in current bucket.
   *
   * @param  {String|undefined}  id        The collection id.
   * @param  {Object}  options             The options object.
   * @param  {Boolean} options.safe        The safe option.
   * @param  {Object}  options.headers     The headers object option.
   * @param  {Object}  options.permissions The permissions object.
   * @param  {Object}  options.data        The metadadata object.
   * @param  {Object}  options.schema      The JSONSchema object.
   * @return {Promise<Object, Error>}
   */
  createCollection(id, options) {
    const reqOptions = this._bucketOptions(options);
    const request = requests.createCollection(id, reqOptions);
    return this.client.execute(request);
  }

  /**
   * Deletes a collection from the current bucket.
   *
   * @param  {Object|String} collection  The collection to delete.
   * @param  {Object}    options         The options object.
   * @param  {Object}    options.headers The headers object option.
   * @param  {Boolean}   options.safe    The safe option.
   * @return {Promise<Object, Error>}
   */
  deleteCollection(collection, options) {
    const reqOptions = this._bucketOptions(options);
    const request = requests.deleteCollection((0, _utils.toDataBody)(collection), reqOptions);
    return this.client.execute(request);
  }

  /**
   * Retrieves the list of permissions for this bucket.
   *
   * @param  {Object} options         The options object.
   * @param  {Object} options.headers The headers object option.
   * @return {Promise<Object, Error>}
   */
  getPermissions(options) {
    return this.getAttributes(this._bucketOptions(options)).then(res => res.permissions);
  }

  /**
   * Recplaces all existing bucket permissions with the ones provided.
   *
   * @param  {Object}  permissions           The permissions object.
   * @param  {Object}  options               The options object
   * @param  {Object}  options               The options object.
   * @param  {Boolean} options.safe          The safe option.
   * @param  {Object}  options.headers       The headers object option.
   * @param  {Object}  options.last_modified The last_modified option.
   * @return {Promise<Object, Error>}
   */
  setPermissions(permissions, options = {}) {
    return this.client.execute(requests.updateBucket({
      id: this.name,
      last_modified: options.last_modified
    }, _extends({}, this._bucketOptions(options), { permissions })));
  }

  /**
   * Performs batch operations at the current bucket level.
   *
   * @param  {Function} fn                 The batch operation function.
   * @param  {Object}   options            The options object.
   * @param  {Object}   options.headers    The headers object option.
   * @param  {Boolean}  options.safe       The safe option.
   * @param  {Boolean}  options.aggregate  Produces a grouped result object.
   * @return {Promise<Object, Error>}
   */
  batch(fn, options) {
    return this.client.batch(fn, this._bucketOptions(options));
  }
};
exports.default = Bucket;

},{"./collection":5,"./endpoint":6,"./requests":9,"./utils":10}],5:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = undefined;

var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

var _utils = require("./utils");

var _requests = require("./requests");

var requests = _interopRequireWildcard(_requests);

var _endpoint = require("./endpoint");

var _endpoint2 = _interopRequireDefault(_endpoint);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

function _interopRequireWildcard(obj) { if (obj && obj.__esModule) { return obj; } else { var newObj = {}; if (obj != null) { for (var key in obj) { if (Object.prototype.hasOwnProperty.call(obj, key)) newObj[key] = obj[key]; } } newObj.default = obj; return newObj; } }

/**
 * Abstract representation of a selected collection.
 *
 */
let Collection = class Collection {
  /**
   * Constructor.
   *
   * @param  {KintoClient}  client          The client instance.
   * @param  {Bucket}       bucket          The bucket instance.
   * @param  {String}       name            The collection name.
   * @param  {Object}       options.headers The headers object option.
   * @param  {Boolean}      options.safe    The safe option.
   */
  constructor(client, bucket, name, options = {}) {
    /**
     * @ignore
     */
    this.client = client;
    /**
     * @ignore
     */
    this.bucket = bucket;
    /**
     * The collection name.
     * @type {String}
     */
    this.name = name;

    /**
     * The default collection options object, embedding the default bucket ones.
     * @ignore
     * @type {Object}
     */
    this.options = _extends({}, this.bucket.options, options, {
      headers: _extends({}, this.bucket.options && this.bucket.options.headers, options.headers)
    });
    /**
     * @ignore
     */
    this._isBatch = !!options.batch;
  }

  /**
   * Merges passed request options with default bucket and collection ones, if
   * any.
   *
   * @private
   * @param  {Object} options The options to merge.
   * @return {Object}         The merged options.
   */
  _collOptions(options = {}) {
    const headers = _extends({}, this.options && this.options.headers, options.headers);
    return _extends({}, this.options, options, {
      headers,
      // XXX soon to be removed once we've migrated everything from KintoClient
      bucket: this.bucket.name
    });
  }

  /**
   * Updates current collection properties.
   *
   * @private
   * @param  {Object} options  The request options.
   * @return {Promise<Object, Error>}
   */
  _updateAttributes(options = {}) {
    const collection = (0, _utils.toDataBody)(this.name);
    const reqOptions = this._collOptions(options);
    const request = requests.updateCollection(collection, reqOptions);
    return this.client.execute(request);
  }

  /**
   * Retrieves collection properties.
   *
   * @param  {Object} options         The options object.
   * @param  {Object} options.headers The headers object option.
   * @return {Promise<Object, Error>}
   */
  getAttributes(options) {
    const { headers } = this._collOptions(options);
    return this.client.execute({
      path: (0, _endpoint2.default)("collection", this.bucket.name, this.name),
      headers
    });
  }

  /**
   * Retrieves the list of permissions for this collection.
   *
   * @param  {Object} options         The options object.
   * @param  {Object} options.headers The headers object option.
   * @return {Promise<Object, Error>}
   */
  getPermissions(options) {
    return this.getAttributes(options).then(res => res.permissions);
  }

  /**
   * Replaces all existing collection permissions with the ones provided.
   *
   * @param  {Object}   permissions     The permissions object.
   * @param  {Object}   options         The options object
   * @param  {Object}   options.headers The headers object option.
   * @param  {Boolean}  options.safe    The safe option.
   * @param  {Number}   options.last_modified The last_modified option.
   * @return {Promise<Object, Error>}
   */
  setPermissions(permissions, options) {
    return this._updateAttributes(_extends({}, options, { permissions }));
  }

  /**
   * Retrieves the JSON schema for this collection, if any.
   *
   * @param  {Object} options         The options object.
   * @param  {Object} options.headers The headers object option.
   * @return {Promise<Object|null, Error>}
   */
  getSchema(options) {
    return this.getAttributes(options).then(res => res.data && res.data.schema || null);
  }

  /**
   * Sets the JSON schema for this collection.
   *
   * @param  {Object}   schema          The JSON schema object.
   * @param  {Object}   options         The options object.
   * @param  {Object}   options.headers The headers object option.
   * @param  {Boolean}  options.safe    The safe option.
   * @param  {Number}   options.last_modified The last_modified option.
   * @return {Promise<Object|null, Error>}
   */
  setSchema(schema, options) {
    return this._updateAttributes(_extends({}, options, { schema }));
  }

  /**
   * Retrieves metadata attached to current collection.
   *
   * @param  {Object} options         The options object.
   * @param  {Object} options.headers The headers object option.
   * @return {Promise<Object, Error>}
   */
  getMetadata(options) {
    return this.getAttributes(options).then(({ data }) => (0, _utils.omit)(data, "schema"));
  }

  /**
   * Sets metadata for current collection.
   *
   * @param  {Object}   metadata        The metadata object.
   * @param  {Object}   options         The options object.
   * @param  {Object}   options.headers The headers object option.
   * @param  {Boolean}  options.safe  The safe option.
   * @param  {Number}   options.last_modified The last_modified option.
   * @return {Promise<Object, Error>}
   */
  setMetadata(metadata, options) {
    // Note: patching allows preventing overridding the schema, which lives
    // within the "data" namespace.
    return this._updateAttributes(_extends({}, options, { metadata, patch: true }));
  }

  /**
   * Creates a record in current collection.
   *
   * @param  {Object} record          The record to create.
   * @param  {Object} options         The options object.
   * @param  {Object} options.headers The headers object option.
   * @param  {Boolean}  options.safe  The safe option.
   * @return {Promise<Object, Error>}
   */
  createRecord(record, options) {
    const reqOptions = this._collOptions(options);
    const request = requests.createRecord(this.name, record, reqOptions);
    return this.client.execute(request);
  }

  /**
   * Updates a record in current collection.
   *
   * @param  {Object}  record                The record to update.
   * @param  {Object}  options               The options object.
   * @param  {Object}  options.headers       The headers object option.
   * @param  {Boolean} options.safe          The safe option.
   * @param  {Number}  options.last_modified The last_modified option.
   * @return {Promise<Object, Error>}
   */
  updateRecord(record, options) {
    const reqOptions = this._collOptions(options);
    const request = requests.updateRecord(this.name, record, reqOptions);
    return this.client.execute(request);
  }

  /**
   * Deletes a record from the current collection.
   *
   * @param  {Object|String} record          The record to delete.
   * @param  {Object}        options         The options object.
   * @param  {Object}        options.headers The headers object option.
   * @param  {Boolean}       options.safe    The safe option.
   * @param  {Number}        options.last_modified The last_modified option.
   * @return {Promise<Object, Error>}
   */
  deleteRecord(record, options) {
    const reqOptions = this._collOptions(options);
    const request = requests.deleteRecord(this.name, (0, _utils.toDataBody)(record), reqOptions);
    return this.client.execute(request);
  }

  /**
   * Retrieves a record from the current collection.
   *
   * @param  {String} id              The record id to retrieve.
   * @param  {Object} options         The options object.
   * @param  {Object} options.headers The headers object option.
   * @return {Promise<Object, Error>}
   */
  getRecord(id, options) {
    return this.client.execute(_extends({
      path: (0, _endpoint2.default)("record", this.bucket.name, this.name, id)
    }, this._collOptions(options)));
  }

  /**
   * Lists records from the current collection.
   *
   * Sorting is done by passing a `sort` string option:
   *
   * - The field to order the results by, prefixed with `-` for descending.
   * Default: `-last_modified`.
   *
   * @see http://kinto.readthedocs.org/en/latest/api/1.x/cliquet/resource.html#sorting
   *
   * Filtering is done by passing a `filters` option object:
   *
   * - `{fieldname: "value"}`
   * - `{min_fieldname: 4000}`
   * - `{in_fieldname: "1,2,3"}`
   * - `{not_fieldname: 0}`
   * - `{exclude_fieldname: "0,1"}`
   *
   * @see http://kinto.readthedocs.org/en/latest/api/1.x/cliquet/resource.html#filtering
   *
   * Paginating is done by passing a `limit` option, then calling the `next()`
   * method from the resolved result object to fetch the next page, if any.
   *
   * @param  {Object}   options         The options object.
   * @param  {Object}   options.headers The headers object option.
   * @param  {Object}   options.filters The filters object.
   * @param  {String}   options.sort    The sort field.
   * @param  {String}   options.limit   The limit field.
   * @param  {String}   options.pages   The number of result pages to aggregate.
   * @param  {Number}   options.since   Only retrieve records modified since the
   * provided timestamp.
   * @return {Promise<Object, Error>}
   */
  listRecords(options = {}) {
    const { http } = this.client;
    const { sort, filters, limit, pages, since } = _extends({
      sort: "-last_modified"
    }, options);
    const collHeaders = this.options.headers;
    const path = (0, _endpoint2.default)("records", this.bucket.name, this.name);
    const querystring = (0, _utils.qsify)(_extends({}, filters, {
      _sort: sort,
      _limit: limit,
      _since: since
    }));
    let results = [],
        current = 0;

    const next = function (nextPage) {
      if (!nextPage) {
        throw new Error("Pagination exhausted.");
      }
      return processNextPage(nextPage);
    };

    const processNextPage = nextPage => {
      return http.request(nextPage, { headers: collHeaders }).then(handleResponse);
    };

    const pageResults = (results, nextPage, etag) => {
      return {
        last_modified: etag,
        data: results,
        next: next.bind(null, nextPage)
      };
    };

    const handleResponse = ({ headers, json }) => {
      const nextPage = headers.get("Next-Page");
      // ETag are supposed to be opaque and stored «as-is».
      const etag = headers.get("ETag");
      if (!pages) {
        return pageResults(json.data, nextPage, etag);
      }
      // Aggregate new results with previous ones
      results = results.concat(json.data);
      current += 1;
      if (current >= pages || !nextPage) {
        // Pagination exhausted
        return pageResults(results, nextPage, etag);
      }
      // Follow next page
      return processNextPage(nextPage);
    };

    return this.client.execute(_extends({
      path: path + "?" + querystring
    }, this._collOptions(options)), { raw: true }).then(handleResponse);
  }

  /**
   * Performs batch operations at the current collection level.
   *
   * @param  {Function} fn                 The batch operation function.
   * @param  {Object}   options            The options object.
   * @param  {Object}   options.headers    The headers object option.
   * @param  {Boolean}  options.safe       The safe option.
   * @param  {Boolean}  options.aggregate  Produces a grouped result object.
   * @return {Promise<Object, Error>}
   */
  batch(fn, options) {
    const reqOptions = this._collOptions(options);
    return this.client.batch(fn, _extends({}, reqOptions, {
      collection: this.name
    }));
  }
};
exports.default = Collection;

},{"./endpoint":6,"./requests":9,"./utils":10}],6:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = endpoint;
/**
 * Endpoints templates.
 * @type {Object}
 */
const ENDPOINTS = {
  root: () => "/",
  batch: () => "/batch",
  buckets: () => "/buckets",
  bucket: bucket => `/buckets/${ bucket }`,
  collections: bucket => `${ ENDPOINTS.bucket(bucket) }/collections`,
  collection: (bucket, coll) => `${ ENDPOINTS.bucket(bucket) }/collections/${ coll }`,
  records: (bucket, coll) => `${ ENDPOINTS.collection(bucket, coll) }/records`,
  record: (bucket, coll, id) => `${ ENDPOINTS.records(bucket, coll) }/${ id }`
};

/**
 * Retrieves a server enpoint by its name.
 *
 * @private
 * @param  {String}    name The endpoint name.
 * @param  {...string} args The endpoint parameters.
 * @return {String}
 */
function endpoint(name, ...args) {
  return ENDPOINTS[name](...args);
}

},{}],7:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
/**
 * Kinto server error code descriptors.
 * @type {Object}
 */
exports.default = {
  104: "Missing Authorization Token",
  105: "Invalid Authorization Token",
  106: "Request body was not valid JSON",
  107: "Invalid request parameter",
  108: "Missing request parameter",
  109: "Invalid posted data",
  110: "Invalid Token / id",
  111: "Missing Token / id",
  112: "Content-Length header was not provided",
  113: "Request body too large",
  114: "Resource was modified meanwhile",
  115: "Method not allowed on this end point",
  116: "Requested version not available on this server",
  117: "Client has sent too many requests",
  121: "Resource access is forbidden for this user",
  122: "Another resource violates constraint",
  201: "Service Temporary unavailable due to high load",
  202: "Service deprecated",
  999: "Internal Server Error"
};

},{}],8:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.default = undefined;

var _errors = require("./errors");

var _errors2 = _interopRequireDefault(_errors);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

/**
 * Enhanced HTTP client for the Kinto protocol.
 * @private
 */
let HTTP = class HTTP {
  /**
   * Default HTTP request headers applied to each outgoing request.
   *
   * @type {Object}
   */
  static get DEFAULT_REQUEST_HEADERS() {
    return {
      "Accept": "application/json",
      "Content-Type": "application/json"
    };
  }

  /**
   * Default options.
   *
   * @type {Object}
   */
  static get defaultOptions() {
    return { timeout: 5000, requestMode: "cors" };
  }

  /**
   * Constructor.
   *
   * Options:
   * - {Number} timeout      The request timeout in ms (default: `5000`).
   * - {String} requestMode  The HTTP request mode (default: `"cors"`).
   *
   * @param {EventEmitter} events  The event handler.
   * @param {Object}       options The options object.
   */
  constructor(events, options = {}) {
    // public properties
    /**
     * The event emitter instance.
     * @type {EventEmitter}
     */
    if (!events) {
      throw new Error("No events handler provided");
    }
    this.events = events;

    options = Object.assign({}, HTTP.defaultOptions, options);

    /**
     * The request mode.
     * @see  https://fetch.spec.whatwg.org/#requestmode
     * @type {String}
     */
    this.requestMode = options.requestMode;

    /**
     * The request timeout.
     * @type {Number}
     */
    this.timeout = options.timeout;
  }

  /**
   * Performs an HTTP request to the Kinto server.
   *
   * Options:
   * - `{Object} headers` The request headers object (default: {})
   *
   * Resolves with an objet containing the following HTTP response properties:
   * - `{Number}  status`  The HTTP status code.
   * - `{Object}  json`    The JSON response body.
   * - `{Headers} headers` The response headers object; see the ES6 fetch() spec.
   *
   * @param  {String} url     The URL.
   * @param  {Object} options The fetch() options object.
   * @return {Promise}
   */
  request(url, options = { headers: {} }) {
    let response, status, statusText, headers, hasTimedout;
    // Ensure default request headers are always set
    options.headers = Object.assign({}, HTTP.DEFAULT_REQUEST_HEADERS, options.headers);
    options.mode = this.requestMode;
    return new Promise((resolve, reject) => {
      const _timeoutId = setTimeout(() => {
        hasTimedout = true;
        reject(new Error("Request timeout."));
      }, this.timeout);
      fetch(url, options).then(res => {
        if (!hasTimedout) {
          clearTimeout(_timeoutId);
          resolve(res);
        }
      }).catch(err => {
        if (!hasTimedout) {
          clearTimeout(_timeoutId);
          reject(err);
        }
      });
    }).then(res => {
      response = res;
      headers = res.headers;
      status = res.status;
      statusText = res.statusText;
      this._checkForDeprecationHeader(headers);
      this._checkForBackoffHeader(status, headers);
      return res.text();
    })
    // Check if we have a body; if so parse it as JSON.
    .then(text => {
      if (text.length === 0) {
        return null;
      }
      // Note: we can't consume the response body twice.
      return JSON.parse(text);
    }).catch(err => {
      const error = new Error(`HTTP ${ status || 0 }; ${ err }`);
      error.response = response;
      error.stack = err.stack;
      throw error;
    }).then(json => {
      if (json && status >= 400) {
        let message = `HTTP ${ status } ${ json.error || "" }: `;
        if (json.errno && json.errno in _errors2.default) {
          const errnoMsg = _errors2.default[json.errno];
          message += errnoMsg;
          if (json.message && json.message !== errnoMsg) {
            message += ` (${ json.message })`;
          }
        } else {
          message += statusText || "";
        }
        const error = new Error(message.trim());
        error.response = response;
        error.data = json;
        throw error;
      }
      return { status, json, headers };
    });
  }

  _checkForDeprecationHeader(headers) {
    const alertHeader = headers.get("Alert");
    if (!alertHeader) {
      return;
    }
    let alert;
    try {
      alert = JSON.parse(alertHeader);
    } catch (err) {
      console.warn("Unable to parse Alert header message", alertHeader);
      return;
    }
    console.warn(alert.message, alert.url);
    this.events.emit("deprecated", alert);
  }

  _checkForBackoffHeader(status, headers) {
    let backoffMs;
    const backoffSeconds = parseInt(headers.get("Backoff"), 10);
    if (backoffSeconds > 0) {
      backoffMs = new Date().getTime() + backoffSeconds * 1000;
    } else {
      backoffMs = 0;
    }
    this.events.emit("backoff", backoffMs);
  }
};
exports.default = HTTP;

},{"./errors":7}],9:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});

var _extends = Object.assign || function (target) { for (var i = 1; i < arguments.length; i++) { var source = arguments[i]; for (var key in source) { if (Object.prototype.hasOwnProperty.call(source, key)) { target[key] = source[key]; } } } return target; };

exports.createBucket = createBucket;
exports.updateBucket = updateBucket;
exports.deleteBucket = deleteBucket;
exports.deleteBuckets = deleteBuckets;
exports.createCollection = createCollection;
exports.updateCollection = updateCollection;
exports.deleteCollection = deleteCollection;
exports.createRecord = createRecord;
exports.updateRecord = updateRecord;
exports.deleteRecord = deleteRecord;

var _endpoint = require("./endpoint");

var _endpoint2 = _interopRequireDefault(_endpoint);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

const requestDefaults = {
  safe: false,
  // check if we should set default content type here
  headers: {},
  bucket: "default",
  permissions: {},
  data: {},
  patch: false
};

function safeHeader(safe, last_modified) {
  if (!safe) {
    return {};
  }
  if (last_modified) {
    return { "If-Match": `"${ last_modified }"` };
  }
  return { "If-None-Match": "*" };
}

/**
 * @private
 */
function createBucket(bucketName, options = {}) {
  if (!bucketName) {
    throw new Error("A bucket name is required.");
  }
  // Note that we simply ignore any "bucket" option passed here, as the one
  // we're interested in is the one provided as a required argument.
  const { headers, permissions, safe } = _extends({}, requestDefaults, options);
  return {
    method: "PUT",
    path: (0, _endpoint2.default)("bucket", bucketName),
    headers: _extends({}, headers, safeHeader(safe)),
    body: {
      // XXX We can't pass the data option just yet, see Kinto/kinto/issues/239
      permissions
    }
  };
}

/**
 * @private
 */
function updateBucket(bucket, options = {}) {
  if (typeof bucket !== "object") {
    throw new Error("A bucket object is required.");
  }
  if (!bucket.id) {
    throw new Error("A bucket id is required.");
  }
  const { headers, permissions, safe, patch, last_modified } = _extends({}, requestDefaults, options);
  return {
    method: patch ? "PATCH" : "PUT",
    path: (0, _endpoint2.default)("bucket", bucket.id),
    headers: _extends({}, headers, safeHeader(safe, last_modified || bucket.last_modified)),
    body: {
      data: bucket,
      permissions
    }
  };
}

/**
 * @private
 */
function deleteBucket(bucket, options = {}) {
  if (typeof bucket !== "object") {
    throw new Error("A bucket object is required.");
  }
  if (!bucket.id) {
    throw new Error("A bucket id is required.");
  }
  const { headers, safe, last_modified } = _extends({}, requestDefaults, {
    last_modified: bucket.last_modified
  }, options);
  if (safe && !last_modified) {
    throw new Error("Safe concurrency check requires a last_modified value.");
  }
  return {
    method: "DELETE",
    path: (0, _endpoint2.default)("bucket", bucket.id),
    headers: _extends({}, headers, safeHeader(safe, last_modified))
  };
}

/**
 * @private
 */
function deleteBuckets(options = {}) {
  const { headers, safe, last_modified } = _extends({}, requestDefaults, options);
  if (safe && !last_modified) {
    throw new Error("Safe concurrency check requires a last_modified value.");
  }
  return {
    method: "DELETE",
    path: (0, _endpoint2.default)("buckets"),
    headers: _extends({}, headers, safeHeader(safe, last_modified))
  };
}

/**
 * @private
 */
function createCollection(id, options = {}) {
  const { bucket, headers, permissions, data, safe } = _extends({}, requestDefaults, options);
  // XXX checks that provided data can't override schema when provided
  const path = id ? (0, _endpoint2.default)("collection", bucket, id) : (0, _endpoint2.default)("collections", bucket);
  return {
    method: id ? "PUT" : "POST",
    path,
    headers: _extends({}, headers, safeHeader(safe)),
    body: { data, permissions }
  };
}

/**
 * @private
 */
function updateCollection(collection, options = {}) {
  if (typeof collection !== "object") {
    throw new Error("A collection object is required.");
  }
  if (!collection.id) {
    throw new Error("A collection id is required.");
  }
  const {
    bucket,
    headers,
    permissions,
    schema,
    metadata,
    safe,
    patch,
    last_modified
  } = _extends({}, requestDefaults, options);
  const collectionData = _extends({}, metadata, collection);
  if (options.schema) {
    collectionData.schema = schema;
  }
  return {
    method: patch ? "PATCH" : "PUT",
    path: (0, _endpoint2.default)("collection", bucket, collection.id),
    headers: _extends({}, headers, safeHeader(safe, last_modified || collection.last_modified)),
    body: {
      data: collectionData,
      permissions
    }
  };
}

/**
 * @private
 */
function deleteCollection(collection, options = {}) {
  if (typeof collection !== "object") {
    throw new Error("A collection object is required.");
  }
  if (!collection.id) {
    throw new Error("A collection id is required.");
  }
  const { bucket, headers, safe, last_modified } = _extends({}, requestDefaults, {
    last_modified: collection.last_modified
  }, options);
  if (safe && !last_modified) {
    throw new Error("Safe concurrency check requires a last_modified value.");
  }
  return {
    method: "DELETE",
    path: (0, _endpoint2.default)("collection", bucket, collection.id),
    headers: _extends({}, headers, safeHeader(safe, last_modified))
  };
}

/**
 * @private
 */
function createRecord(collName, record, options = {}) {
  if (!collName) {
    throw new Error("A collection name is required.");
  }
  const { bucket, headers, permissions, safe } = _extends({}, requestDefaults, options);
  return {
    // Note: Safe POST using a record id would fail.
    // see https://github.com/Kinto/kinto/issues/489
    method: record.id ? "PUT" : "POST",
    path: record.id ? (0, _endpoint2.default)("record", bucket, collName, record.id) : (0, _endpoint2.default)("records", bucket, collName),
    headers: _extends({}, headers, safeHeader(safe)),
    body: {
      data: record,
      permissions
    }
  };
}

/**
 * @private
 */
function updateRecord(collName, record, options = {}) {
  if (!collName) {
    throw new Error("A collection name is required.");
  }
  if (!record.id) {
    throw new Error("A record id is required.");
  }
  const { bucket, headers, permissions, safe, patch, last_modified } = _extends({}, requestDefaults, options);
  return {
    method: patch ? "PATCH" : "PUT",
    path: (0, _endpoint2.default)("record", bucket, collName, record.id),
    headers: _extends({}, headers, safeHeader(safe, last_modified || record.last_modified)),
    body: {
      data: record,
      permissions
    }
  };
}

/**
 * @private
 */
function deleteRecord(collName, record, options = {}) {
  if (!collName) {
    throw new Error("A collection name is required.");
  }
  if (typeof record !== "object") {
    throw new Error("A record object is required.");
  }
  if (!record.id) {
    throw new Error("A record id is required.");
  }
  const { bucket, headers, safe, last_modified } = _extends({}, requestDefaults, {
    last_modified: record.last_modified
  }, options);
  if (safe && !last_modified) {
    throw new Error("Safe concurrency check requires a last_modified value.");
  }
  return {
    method: "DELETE",
    path: (0, _endpoint2.default)("record", bucket, collName, record.id),
    headers: _extends({}, headers, safeHeader(safe, last_modified))
  };
}

},{"./endpoint":6}],10:[function(require,module,exports){
"use strict";

Object.defineProperty(exports, "__esModule", {
  value: true
});
exports.partition = partition;
exports.pMap = pMap;
exports.omit = omit;
exports.toDataBody = toDataBody;
exports.qsify = qsify;
exports.checkVersion = checkVersion;
exports.support = support;
exports.nobatch = nobatch;
/**
 * Chunks an array into n pieces.
 *
 * @private
 * @param  {Array}  array
 * @param  {Number} n
 * @return {Array}
 */
function partition(array, n) {
  if (n <= 0) {
    return array;
  }
  return array.reduce((acc, x, i) => {
    if (i === 0 || i % n === 0) {
      acc.push([x]);
    } else {
      acc[acc.length - 1].push(x);
    }
    return acc;
  }, []);
}

/**
 * Maps a list to promises using the provided mapping function, executes them
 * sequentially then returns a Promise resolving with ordered results obtained.
 * Think of this as a sequential Promise.all.
 *
 * @private
 * @param  {Array}    list The list to map.
 * @param  {Function} fn   The mapping function.
 * @return {Promise}
 */
function pMap(list, fn) {
  let results = [];
  return list.reduce((promise, entry) => {
    return promise.then(() => {
      return Promise.resolve(fn(entry)).then(result => results = results.concat(result));
    });
  }, Promise.resolve()).then(() => results);
}

/**
 * Takes an object and returns a copy of it with the provided keys omitted.
 *
 * @private
 * @param  {Object}    obj  The source object.
 * @param  {...String} keys The keys to omit.
 * @return {Object}
 */
function omit(obj, ...keys) {
  return Object.keys(obj).reduce((acc, key) => {
    if (keys.indexOf(key) === -1) {
      acc[key] = obj[key];
    }
    return acc;
  }, {});
}

/**
 * Always returns a resource data object from the provided argument.
 *
 * @private
 * @param  {Object|String} value
 * @return {Object}
 */
function toDataBody(value) {
  if (typeof value === "object") {
    return value;
  }
  if (typeof value === "string") {
    return { id: value };
  }
  throw new Error("Invalid collection argument.");
}

/**
 * Transforms an object into an URL query string, stripping out any undefined
 * values.
 *
 * @param  {Object} obj
 * @return {String}
 */
function qsify(obj) {
  const sep = "&";
  const encode = v => encodeURIComponent(typeof v === "boolean" ? String(v) : v);
  const stripUndefined = o => JSON.parse(JSON.stringify(o));
  const stripped = stripUndefined(obj);
  return Object.keys(stripped).map(k => {
    const ks = encode(k) + "=";
    if (Array.isArray(stripped[k])) {
      return stripped[k].map(v => ks + encode(v)).join(sep);
    } else {
      return ks + encode(stripped[k]);
    }
  }).join(sep);
}

/**
 * Checks if a version is within the provided range.
 *
 * @param  {String} version    The version to check.
 * @param  {String} minVersion The minimum supported version (inclusive).
 * @param  {String} maxVersion The minimum supported version (exclusive).
 * @throws {Error} If the version is outside of the provided range.
 */
function checkVersion(version, minVersion, maxVersion) {
  const extract = str => str.split(".").map(x => parseInt(x, 10));
  const [verMajor, verMinor] = extract(version);
  const [minMajor, minMinor] = extract(minVersion);
  const [maxMajor, maxMinor] = extract(maxVersion);
  const checks = [verMajor < minMajor, verMajor === minMajor && verMinor < minMinor, verMajor > maxMajor, verMajor === maxMajor && verMinor >= maxMinor];
  if (checks.some(x => x)) {
    throw new Error(`Version ${ version } doesn't satisfy ` + `${ minVersion } <= x < ${ maxVersion }`);
  }
}

/**
 * Generates a decorator function ensuring a version check is performed against
 * the provided requirements before executing it.
 *
 * @param  {String} min The required min version (inclusive).
 * @param  {String} max The required max version (inclusive).
 * @return {Function}
 */
function support(min, max) {
  return function (target, key, descriptor) {
    const fn = descriptor.value;
    return {
      configurable: true,
      get() {
        const wrappedMethod = (...args) => {
          // "this" is the current instance which its method is decorated.
          const client = "client" in this ? this.client : this;
          return client.fetchHTTPApiVersion().then(version => checkVersion(version, min, max)).then(Promise.resolve(fn.apply(this, args)));
        };
        Object.defineProperty(this, key, {
          value: wrappedMethod,
          configurable: true,
          writable: true
        });
        return wrappedMethod;
      }
    };
  };
}

/**
 * Generates a decorator function ensuring an operation is not performed from
 * within a batch request.
 *
 * @param  {String} message The error message to throw.
 * @return {Function}
 */
function nobatch(message) {
  return function (target, key, descriptor) {
    const fn = descriptor.value;
    return {
      configurable: true,
      get() {
        const wrappedMethod = (...args) => {
          // "this" is the current instance which its method is decorated.
          if (this._isBatch) {
            throw new Error(message);
          }
          return fn.apply(this, args);
        };
        Object.defineProperty(this, key, {
          value: wrappedMethod,
          configurable: true,
          writable: true
        });
        return wrappedMethod;
      }
    };
  };
}

},{}]},{},[1])(1)
});