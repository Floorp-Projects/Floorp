/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains an implementation of the Storage Server in JavaScript.
 *
 * The server should not be used for any production purposes.
 */

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

const EXPORTED_SYMBOLS = [
  "ServerBSO",
  "StorageServerCallback",
  "StorageServerCollection",
  "StorageServer",
  "storageServerForUsers",
];

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://services-common/async.js");
Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/utils.js");

const STORAGE_HTTP_LOGGER = "Services.Common.Test.Server";
const STORAGE_API_VERSION = "2.0";

// Use the same method that record.js does, which mirrors the server.
function new_timestamp() {
  return Math.round(Date.now());
}

function isInteger(s) {
  let re = /^[0-9]+$/;
  return re.test(s);
}

function writeHttpBody(response, body) {
  if (!body) {
    return;
  }

  response.bodyOutputStream.write(body, body.length);
}

function sendMozSvcError(request, response, code) {
  response.setStatusLine(request.httpVersion, 400, "Bad Request");
  response.setHeader("Content-Type", "text/plain", false);
  response.bodyOutputStream.write(code, code.length);
}

/**
 * Represent a BSO on the server.
 *
 * A BSO is constructed from an ID, content, and a modified time.
 *
 * @param id
 *        (string) ID of the BSO being created.
 * @param payload
 *        (strong|object) Payload for the BSO. Should ideally be a string. If
 *        an object is passed, it will be fed into JSON.stringify and that
 *        output will be set as the payload.
 * @param modified
 *        (number) Milliseconds since UNIX epoch that the BSO was last
 *        modified. If not defined or null, the current time will be used.
 */
function ServerBSO(id, payload, modified) {
  if (!id) {
    throw new Error("No ID for ServerBSO!");
  }

  if (!id.match(/^[a-zA-Z0-9_-]{1,64}$/)) {
    throw new Error("BSO ID is invalid: " + id);
  }

  this._log = Log4Moz.repository.getLogger(STORAGE_HTTP_LOGGER);

  this.id = id;
  if (!payload) {
    return;
  }

  CommonUtils.ensureMillisecondsTimestamp(modified);

  if (typeof payload == "object") {
    payload = JSON.stringify(payload);
  }

  this.payload = payload;
  this.modified = modified || new_timestamp();
}
ServerBSO.prototype = {
  FIELDS: [
    "id",
    "modified",
    "payload",
    "ttl",
    "sortindex",
  ],

  toJSON: function toJSON() {
    let obj = {};

    for each (let key in this.FIELDS) {
      if (this[key] !== undefined) {
        obj[key] = this[key];
      }
    }

    return obj;
  },

  delete: function delete() {
    this.deleted = true;

    delete this.payload;
    delete this.modified;
  },

  /**
   * Handler for GET requests for this BSO.
   */
  getHandler: function getHandler(request, response) {
    let code = 200;
    let status = "OK";
    let body;

    function sendResponse() {
      response.setStatusLine(request.httpVersion, code, status);
      writeHttpBody(response, body);
    }

    if (request.hasHeader("x-if-modified-since")) {
      let headerModified = parseInt(request.getHeader("x-if-modified-since"),
                                    10);
      CommonUtils.ensureMillisecondsTimestamp(headerModified);

      if (headerModified >= this.modified) {
        code = 304;
        status = "Not Modified";

        sendResponse();
        return;
      }
    } else if (request.hasHeader("x-if-unmodified-since")) {
      let requestModified = parseInt(request.getHeader("x-if-unmodified-since"),
                                     10);
      let serverModified = this.modified;

      if (serverModified > requestModified) {
        code = 412;
        status = "Precondition Failed";
        sendResponse();
        return;
      }
    }

    if (!this.deleted) {
      body = JSON.stringify(this.toJSON());
      response.setHeader("Content-Type", "application/json", false);
      response.setHeader("X-Last-Modified", "" + this.modified, false);
    } else {
      code = 404;
      status = "Not Found";
    }

    sendResponse();
  },

  /**
   * Handler for PUT requests for this BSO.
   */
  putHandler: function putHandler(request, response) {
    if (request.hasHeader("Content-Type")) {
      let ct = request.getHeader("Content-Type");
      if (ct != "application/json") {
        throw HTTP_415;
      }
    }

    let input = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
    let parsed;
    try {
      parsed = JSON.parse(input);
    } catch (ex) {
      return sendMozSvcError(request, response, "8");
    }

    if (typeof(parsed) != "object") {
      return sendMozSvcError(request, response, "8");
    }

    // Don't update if a conditional request fails preconditions.
    if (request.hasHeader("x-if-unmodified-since")) {
      let reqModified = parseInt(request.getHeader("x-if-unmodified-since"));

      if (reqModified < this.modified) {
        response.setStatusLine(request.httpVersion, 412, "Precondition Failed");
        return;
      }
    }

    let code, status;
    if (this.payload) {
      code = 204;
      status = "No Content";
    } else {
      code = 201;
      status = "Created";
    }

    // Alert when we see unrecognized fields.
    for (let [key, value] in Iterator(parsed)) {
      switch (key) {
        case "payload":
          if (typeof(value) != "string") {
            sendMozSvcError(request, response, "8");
            return true;
          }

          this.payload = value;
          break;

        case "ttl":
          if (!isInteger(value)) {
            sendMozSvcError(request, response, "8");
            return true;
          }
          this.ttl = parseInt(value, 10);
          break;

        case "sortindex":
          if (!isInteger(value) || value.length > 9) {
            sendMozSvcError(request, response, "8");
            return true;
          }
          this.sortindex = parseInt(value, 10);
          break;

        case "id":
          break;

        default:
          this._log.warn("Unexpected field in BSO record: " + key);
          sendMozSvcError(request, response, "8");
          return true;
      }
    }

    this.modified = request.timestamp;
    response.setHeader("X-Last-Modified", "" + this.modified, false);

    response.setStatusLine(request.httpVersion, code, status);
  },
};

/**
 * Represent a collection on the server.
 *
 * The '_bsos' attribute is a mapping of id -> ServerBSO objects.
 *
 * Note that if you want these records to be accessible individually,
 * you need to register their handlers with the server separately, or use a
 * containing HTTP server that will do so on your behalf.
 *
 * @param bsos
 *        An object mapping BSO IDs to ServerBSOs.
 * @param acceptNew
 *        If true, POSTs to this collection URI will result in new BSOs being
 *        created and wired in on the fly.
 * @param timestamp
 *        An optional timestamp value to initialize the modified time of the
 *        collection. This should be in the format returned by new_timestamp().
 */
function StorageServerCollection(bsos, acceptNew, timestamp=new_timestamp()) {
  this._bsos = bsos || {};
  this.acceptNew = acceptNew || false;

  /*
   * Track modified timestamp.
   * We can't just use the timestamps of contained BSOs: an empty collection
   * has a modified time.
   */
  CommonUtils.ensureMillisecondsTimestamp(timestamp);
  this._timestamp = timestamp;

  this._log = Log4Moz.repository.getLogger(STORAGE_HTTP_LOGGER);
}
StorageServerCollection.prototype = {
  BATCH_MAX_COUNT: 100,         // # of records.
  BATCH_MAX_SIZE: 1024 * 1024,  // # bytes.

  _timestamp: null,

  get timestamp() {
    return this._timestamp;
  },

  set timestamp(timestamp) {
    CommonUtils.ensureMillisecondsTimestamp(timestamp);
    this._timestamp = timestamp;
  },

  get totalPayloadSize() {
    let size = 0;
    for each (let bso in this.bsos()) {
      size += bso.payload.length;
    }

    return size;
  },

  /**
   * Convenience accessor for our BSO keys.
   * Excludes deleted items, of course.
   *
   * @param filter
   *        A predicate function (applied to the ID and BSO) which dictates
   *        whether to include the BSO's ID in the output.
   *
   * @return an array of IDs.
   */
  keys: function keys(filter) {
    return [id for ([id, bso] in Iterator(this._bsos))
               if (!bso.deleted && (!filter || filter(id, bso)))];
  },

  /**
   * Convenience method to get an array of BSOs.
   * Optionally provide a filter function.
   *
   * @param filter
   *        A predicate function, applied to the BSO, which dictates whether to
   *        include the BSO in the output.
   *
   * @return an array of ServerBSOs.
   */
  bsos: function bsos(filter) {
    let os = [bso for ([id, bso] in Iterator(this._bsos))
              if (!bso.deleted)];

    if (!filter) {
      return os;
    }

    return os.filter(filter);
  },

  /**
   * Obtain a BSO by ID.
   */
  bso: function bso(id) {
    return this._bsos[id];
  },

  /**
   * Obtain the payload of a specific BSO.
   *
   * Raises if the specified BSO does not exist.
   */
  payload: function payload(id) {
    return this.bso(id).payload;
  },

  /**
   * Insert the provided BSO under its ID.
   *
   * @return the provided BSO.
   */
  insertBSO: function insertBSO(bso) {
    return this._bsos[bso.id] = bso;
  },

  /**
   * Insert the provided payload as part of a new ServerBSO with the provided
   * ID.
   *
   * @param id
   *        The GUID for the BSO.
   * @param payload
   *        The payload, as provided to the ServerBSO constructor.
   * @param modified
   *        An optional modified time for the ServerBSO. If not specified, the
   *        current time will be used.
   *
   * @return the inserted BSO.
   */
  insert: function insert(id, payload, modified) {
    return this.insertBSO(new ServerBSO(id, payload, modified));
  },

  /**
   * Removes an object entirely from the collection.
   *
   * @param id
   *        (string) ID to remove.
   */
  remove: function remove(id) {
    delete this._bsos[id];
  },

  _inResultSet: function _inResultSet(bso, options) {
    if (!bso.payload) {
      return false;
    }

    if (options.ids) {
      if (options.ids.indexOf(bso.id) == -1) {
        return false;
      }
    }

    if (options.newer) {
      if (bso.modified <= options.newer) {
        return false;
      }
    }

    if (options.older) {
      if (bso.modified >= options.older) {
        return false;
      }
    }

    return true;
  },

  count: function count(options) {
    options = options || {};
    let c = 0;
    for (let [id, bso] in Iterator(this._bsos)) {
      if (bso.modified && this._inResultSet(bso, options)) {
        c++;
      }
    }
    return c;
  },

  get: function get(options) {
    let data = [];
    for each (let bso in this._bsos) {
      if (!bso.modified) {
        continue;
      }

      if (!this._inResultSet(bso, options)) {
        continue;
      }

      data.push(bso);
    }

    if (options.sort) {
      if (options.sort == "oldest") {
        data.sort(function sortOldest(a, b) {
          if (a.modified == b.modified) {
            return 0;
          }

          return a.modified < b.modified ? -1 : 1;
        });
      } else if (options.sort == "newest") {
        data.sort(function sortNewest(a, b) {
          if (a.modified == b.modified) {
            return 0;
          }

          return a.modified > b.modified ? -1 : 1;
        });
      } else if (options.sort == "index") {
        data.sort(function sortIndex(a, b) {
          if (a.sortindex == b.sortindex) {
            return 0;
          }

          if (a.sortindex !== undefined && b.sortindex == undefined) {
            return 1;
          }

          if (a.sortindex === undefined && b.sortindex !== undefined) {
            return -1;
          }

          return a.sortindex > b.sortindex ? -1 : 1;
        });
      }
    }

    if (options.limit) {
      data = data.slice(0, options.limit);
    }

    return data;
  },

  post: function post(input, timestamp) {
    let success = [];
    let failed = {};
    let count = 0;
    let size = 0;

    // This will count records where we have an existing ServerBSO
    // registered with us as successful and all other records as failed.
    for each (let record in input) {
      count += 1;
      if (count > this.BATCH_MAX_COUNT) {
        failed[record.id] = "Max record count exceeded.";
        continue;
      }

      if (typeof(record.payload) != "string") {
        failed[record.id] = "Payload is not a string!";
        continue;
      }

      size += record.payload.length;
      if (size > this.BATCH_MAX_SIZE) {
        failed[record.id] = "Payload max size exceeded!";
        continue;
      }

      if (record.sortindex) {
        if (!isInteger(record.sortindex)) {
          failed[record.id] = "sortindex is not an integer.";
          continue;
        }

        if (record.sortindex.length > 9) {
          failed[record.id] = "sortindex is too long.";
          continue;
        }
      }

      if ("ttl" in record) {
        if (!isInteger(record.ttl)) {
          failed[record.id] = "ttl is not an integer.";
          continue;
        }
      }

      try {
        let bso = this.bso(record.id);
        if (!bso && this.acceptNew) {
          this._log.debug("Creating BSO " + JSON.stringify(record.id) +
                          " on the fly.");
          bso = new ServerBSO(record.id);
          this.insertBSO(bso);
        }
        if (bso) {
          bso.payload = record.payload;
          bso.modified = timestamp;
          success.push(record.id);

          if (record.sortindex) {
            bso.sortindex = parseInt(record.sortindex, 10);
          }

        } else {
          failed[record.id] = "no bso configured";
        }
      } catch (ex) {
        this._log.info("Exception when processing BSO: " +
                       CommonUtils.exceptionStr(ex));
        failed[record.id] = "Exception when processing.";
      }
    }
    return {success: success, failed: failed};
  },

  delete: function delete(options) {
    options = options || {};

    // Protocol 2.0 only allows the "ids" query string argument.
    let keys = Object.keys(options).filter(function(k) {
      return k != "ids";
    });
    if (keys.length) {
      this._log.warn("Invalid query string parameter to collection delete: " +
                     keys.join(", "));
      throw new Error("Malformed client request.");
    }

    if (options.ids && options.ids.length > this.BATCH_MAX_COUNT) {
      throw HTTP_400;
    }

    let deleted = [];
    for (let [id, bso] in Iterator(this._bsos)) {
      if (this._inResultSet(bso, options)) {
        this._log.debug("Deleting " + JSON.stringify(bso));
        deleted.push(bso.id);
        bso.delete();
      }
    }
    return deleted;
  },

  parseOptions: function parseOptions(request) {
    let options = {};

    for each (let chunk in request.queryString.split("&")) {
      if (!chunk) {
        continue;
      }
      chunk = chunk.split("=");
      let key = decodeURIComponent(chunk[0]);
      if (chunk.length == 1) {
        options[key] = "";
      } else {
        options[key] = decodeURIComponent(chunk[1]);
      }
    }

    if (options.ids) {
      options.ids = options.ids.split(",");
    }

    if (options.newer) {
      if (!isInteger(options.newer)) {
        throw HTTP_400;
      }

      CommonUtils.ensureMillisecondsTimestamp(options.newer);
      options.newer = parseInt(options.newer, 10);
    }

    if (options.older) {
      if (!isInteger(options.older)) {
        throw HTTP_400;
      }

      CommonUtils.ensureMillisecondsTimestamp(options.older);
      options.older = parseInt(options.older, 10);
    }

    if (options.limit) {
      if (!isInteger(options.limit)) {
        throw HTTP_400;
      }

      options.limit = parseInt(options.limit, 10);
    }

    return options;
  },

  getHandler: function getHandler(request, response) {
    let options = this.parseOptions(request);
    let data = this.get(options);

    if (request.hasHeader("x-if-modified-since")) {
      let requestModified = parseInt(request.getHeader("x-if-modified-since"),
                                     10);
      let newestBSO = 0;
      for each (let bso in data) {
        if (bso.modified > newestBSO) {
          newestBSO = bso.modified;
        }
      }

      if (requestModified >= newestBSO) {
        response.setHeader("X-Last-Modified", "" + newestBSO);
        response.setStatusLine(request.httpVersion, 304, "Not Modified");
        return;
      }
    } else if (request.hasHeader("x-if-unmodified-since")) {
      let requestModified = parseInt(request.getHeader("x-if-unmodified-since"),
                                     10);
      let serverModified = this.timestamp;

      if (serverModified > requestModified) {
        response.setHeader("X-Last-Modified", "" + serverModified);
        response.setStatusLine(request.httpVersion, 412, "Precondition Failed");
        return;
      }
    }

    if (options.full) {
      data = data.map(function map(bso) {
        return bso.toJSON();
      });
    } else {
      data = data.map(function map(bso) {
        return bso.id;
      });
    }

    // application/json is default media type.
    let newlines = false;
    if (request.hasHeader("accept")) {
      let accept = request.getHeader("accept");
      if (accept == "application/newlines") {
        newlines = true;
      } else if (accept != "application/json") {
        throw HTTP_406;
      }
    }

    let body;
    if (newlines) {
      response.setHeader("Content-Type", "application/newlines", false);
      let normalized = data.map(function map(d) {
        return JSON.stringify(d);
      });

      body = normalized.join("\n") + "\n";
    } else {
      response.setHeader("Content-Type", "application/json", false);
      body = JSON.stringify({items: data});
    }

    this._log.info("Records: " + data.length);
    response.setHeader("X-Num-Records", "" + data.length, false);
    response.setHeader("X-Last-Modified", "" + this.timestamp, false);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(body, body.length);
  },

  postHandler: function postHandler(request, response) {
    let options = this.parseOptions(request);

    if (!request.hasHeader("content-type")) {
      this._log.info("No Content-Type request header!");
      throw HTTP_400;
    }

    let inputStream = request.bodyInputStream;
    let inputBody = CommonUtils.readBytesFromInputStream(inputStream);
    let input = [];

    let inputMediaType = request.getHeader("content-type");
    if (inputMediaType == "application/json") {
      try {
        input = JSON.parse(inputBody);
      } catch (ex) {
        this._log.info("JSON parse error on input body!");
        throw HTTP_400;
      }

      if (!Array.isArray(input)) {
        this._log.info("Input JSON type not an array!");
        return sendMozSvcError(request, response, "8");
      }
    } else if (inputMediaType == "application/newlines") {
      for each (let line in inputBody.split("\n")) {
        let record;
        try {
          record = JSON.parse(line);
        } catch (ex) {
          this._log.info("JSON parse error on line!");
          return sendMozSvcError(request, response, "8");
        }

        input.push(record);
      }
    } else {
      this._log.info("Unknown media type: " + inputMediaType);
      throw HTTP_415;
    }

    if (this._ensureUnmodifiedSince(request, response)) {
      return;
    }

    let res = this.post(input, request.timestamp);
    let body = JSON.stringify(res);
    response.setHeader("Content-Type", "application/json", false);
    this.timestamp = request.timestamp;
    response.setHeader("X-Last-Modified", "" + this.timestamp, false);

    response.setStatusLine(request.httpVersion, "200", "OK");
    response.bodyOutputStream.write(body, body.length);
  },

  deleteHandler: function deleteHandler(request, response) {
    this._log.debug("Invoking StorageServerCollection.DELETE.");

    let options = this.parseOptions(request);

    if (this._ensureUnmodifiedSince(request, response)) {
      return;
    }

    let deleted = this.delete(options);
    response.deleted = deleted;
    this.timestamp = request.timestamp;

    response.setStatusLine(request.httpVersion, 204, "No Content");
  },

  handler: function handler() {
    let self = this;

    return function(request, response) {
      switch(request.method) {
        case "GET":
          return self.getHandler(request, response);

        case "POST":
          return self.postHandler(request, response);

        case "DELETE":
          return self.deleteHandler(request, response);

      }

      request.setHeader("Allow", "GET,POST,DELETE");
      response.setStatusLine(request.httpVersion, 405, "Method Not Allowed");
    };
  },

  _ensureUnmodifiedSince: function _ensureUnmodifiedSince(request, response) {
    if (!request.hasHeader("x-if-unmodified-since")) {
      return false;
    }

    let requestModified = parseInt(request.getHeader("x-if-unmodified-since"),
                                   10);
    let serverModified = this.timestamp;

    this._log.debug("Request modified time: " + requestModified +
                    "; Server modified time: " + serverModified);
    if (serverModified <= requestModified) {
      return false;
    }

    this._log.info("Conditional request rejected because client time older " +
                   "than collection timestamp.");
    response.setStatusLine(request.httpVersion, 412, "Precondition Failed");
    return true;
  },
};


//===========================================================================//
// httpd.js-based Storage server.                                            //
//===========================================================================//

/**
 * In general, the preferred way of using StorageServer is to directly
 * introspect it. Callbacks are available for operations which are hard to
 * verify through introspection, such as deletions.
 *
 * One of the goals of this server is to provide enough hooks for test code to
 * find out what it needs without monkeypatching. Use this object as your
 * prototype, and override as appropriate.
 */
let StorageServerCallback = {
  onCollectionDeleted: function onCollectionDeleted(user, collection) {},
  onItemDeleted: function onItemDeleted(user, collection, bsoID) {},

  /**
   * Called at the top of every request.
   *
   * Allows the test to inspect the request. Hooks should be careful not to
   * modify or change state of the request or they may impact future processing.
   */
  onRequest: function onRequest(request) {},
};

/**
 * Construct a new test Storage server. Takes a callback object (e.g.,
 * StorageServerCallback) as input.
 */
function StorageServer(callback) {
  this.callback     = callback || {__proto__: StorageServerCallback};
  this.server       = new HttpServer();
  this.started      = false;
  this.users        = {};
  this.requestCount = 0;
  this._log         = Log4Moz.repository.getLogger(STORAGE_HTTP_LOGGER);

  // Install our own default handler. This allows us to mess around with the
  // whole URL space.
  let handler = this.server._handler;
  handler._handleDefault = this.handleDefault.bind(this, handler);
}
StorageServer.prototype = {
  DEFAULT_QUOTA: 1024 * 1024, // # bytes.

  port:   8080,
  server: null,    // HttpServer.
  users:  null,    // Map of username => {collections, password}.

  /**
   * If true, the server will allow any arbitrary user to be used.
   *
   * No authentication will be performed. Whatever user is detected from the
   * URL or auth headers will be created (if needed) and used.
   */
  allowAllUsers: false,

  /**
   * Start the StorageServer's underlying HTTP server.
   *
   * @param port
   *        The numeric port on which to start. A falsy value implies the
   *        default (8080).
   * @param cb
   *        A callback function (of no arguments) which is invoked after
   *        startup.
   */
  start: function start(port, cb) {
    if (this.started) {
      this._log.warn("Warning: server already started on " + this.port);
      return;
    }
    if (port) {
      this.port = port;
    }
    try {
      this.server.start(this.port);
      this.started = true;
      if (cb) {
        cb();
      }
    } catch (ex) {
      _("==========================================");
      _("Got exception starting Storage HTTP server on port " + this.port);
      _("Error: " + CommonUtils.exceptionStr(ex));
      _("Is there a process already listening on port " + this.port + "?");
      _("==========================================");
      do_throw(ex);
    }
  },

  /**
   * Start the server synchronously.
   *
   * @param port
   *        The numeric port on which to start. A falsy value implies the
   *        default (8080).
   */
  startSynchronous: function startSynchronous(port) {
    let cb = Async.makeSpinningCallback();
    this.start(port, cb);
    cb.wait();
  },

  /**
   * Stop the StorageServer's HTTP server.
   *
   * @param cb
   *        A callback function. Invoked after the server has been stopped.
   *
   */
  stop: function stop(cb) {
    if (!this.started) {
      this._log.warn("StorageServer: Warning: server not running. Can't stop " +
                     "me now!");
      return;
    }

    this.server.stop(cb);
    this.started = false;
  },

  serverTime: function serverTime() {
    return new_timestamp();
  },

  /**
   * Create a new user, complete with an empty set of collections.
   *
   * @param username
   *        The username to use. An Error will be thrown if a user by that name
   *        already exists.
   * @param password
   *        A password string.
   *
   * @return a user object, as would be returned by server.user(username).
   */
  registerUser: function registerUser(username, password) {
    if (username in this.users) {
      throw new Error("User already exists.");
    }

    if (!isFinite(parseInt(username))) {
      throw new Error("Usernames must be numeric: " + username);
    }

    this._log.info("Registering new user with server: " + username);
    this.users[username] = {
      password: password,
      collections: {},
      quota: this.DEFAULT_QUOTA,
    };
    return this.user(username);
  },

  userExists: function userExists(username) {
    return username in this.users;
  },

  getCollection: function getCollection(username, collection) {
    return this.users[username].collections[collection];
  },

  _insertCollection: function _insertCollection(collections, collection, bsos) {
    let coll = new StorageServerCollection(bsos, true);
    coll.collectionHandler = coll.handler();
    collections[collection] = coll;
    return coll;
  },

  createCollection: function createCollection(username, collection, bsos) {
    if (!(username in this.users)) {
      throw new Error("Unknown user.");
    }
    let collections = this.users[username].collections;
    if (collection in collections) {
      throw new Error("Collection already exists.");
    }
    return this._insertCollection(collections, collection, bsos);
  },

  deleteCollection: function deleteCollection(username, collection) {
    if (!(username in this.users)) {
      throw new Error("Unknown user.");
    }
    delete this.users[username].collections[collection];
  },

  /**
   * Accept a map like the following:
   * {
   *   meta: {global: {version: 1, ...}},
   *   crypto: {"keys": {}, foo: {bar: 2}},
   *   bookmarks: {}
   * }
   * to cause collections and BSOs to be created.
   * If a collection already exists, no error is raised.
   * If a BSO already exists, it will be updated to the new contents.
   */
  createContents: function createContents(username, collections) {
    if (!(username in this.users)) {
      throw new Error("Unknown user.");
    }
    let userCollections = this.users[username].collections;
    for (let [id, contents] in Iterator(collections)) {
      let coll = userCollections[id] ||
                 this._insertCollection(userCollections, id);
      for (let [bsoID, payload] in Iterator(contents)) {
        coll.insert(bsoID, payload);
      }
    }
  },

  /**
   * Insert a BSO in an existing collection.
   */
  insertBSO: function insertBSO(username, collection, bso) {
    if (!(username in this.users)) {
      throw new Error("Unknown user.");
    }
    let userCollections = this.users[username].collections;
    if (!(collection in userCollections)) {
      throw new Error("Unknown collection.");
    }
    userCollections[collection].insertBSO(bso);
    return bso;
  },

  /**
   * Delete all of the collections for the named user.
   *
   * @param username
   *        The name of the affected user.
   */
  deleteCollections: function deleteCollections(username) {
    if (!(username in this.users)) {
      throw new Error("Unknown user.");
    }
    let userCollections = this.users[username].collections;
    for each (let [name, coll] in Iterator(userCollections)) {
      this._log.trace("Bulk deleting " + name + " for " + username + "...");
      coll.delete({});
    }
    this.users[username].collections = {};
  },

  getQuota: function getQuota(username) {
    if (!(username in this.users)) {
      throw new Error("Unknown user.");
    }

    return this.users[username].quota;
  },

  /**
   * Obtain the newest timestamp of all collections for a user.
   */
  newestCollectionTimestamp: function newestCollectionTimestamp(username) {
    let collections = this.users[username].collections;
    let newest = 0;
    for each (let collection in collections) {
      if (collection.timestamp > newest) {
        newest = collection.timestamp;
      }
    }

    return newest;
  },

  /**
   * Compute the object that is returned for an info/collections request.
   */
  infoCollections: function infoCollections(username) {
    let responseObject = {};
    let colls = this.users[username].collections;
    for (let coll in colls) {
      responseObject[coll] = colls[coll].timestamp;
    }
    this._log.trace("StorageServer: info/collections returning " +
                    JSON.stringify(responseObject));
    return responseObject;
  },

  infoCounts: function infoCounts(username) {
    let data = {};
    let collections = this.users[username].collections;
    for (let [k, v] in Iterator(collections)) {
      let count = v.count();
      if (!count) {
        continue;
      }

      data[k] = count;
    }

    return data;
  },

  infoUsage: function infoUsage(username) {
    let data = {};
    let collections = this.users[username].collections;
    for (let [k, v] in Iterator(collections)) {
      data[k] = v.totalPayloadSize;
    }

    return data;
  },

  infoQuota: function infoQuota(username) {
    let total = 0;
    for each (let value in this.infoUsage(username)) {
      total += value;
    }

    return {
      quota: this.getQuota(username),
      usage: total
    };
  },

  /**
   * Simple accessor to allow collective binding and abbreviation of a bunch of
   * methods. Yay!
   * Use like this:
   *
   *   let u = server.user("john");
   *   u.collection("bookmarks").bso("abcdefg").payload;  // Etc.
   *
   * @return a proxy for the user data stored in this server.
   */
  user: function user(username) {
    let collection       = this.getCollection.bind(this, username);
    let createCollection = this.createCollection.bind(this, username);
    let createContents   = this.createContents.bind(this, username);
    let modified         = function (collectionName) {
      return collection(collectionName).timestamp;
    }
    let deleteCollections = this.deleteCollections.bind(this, username);
    let quota             = this.getQuota.bind(this, username);
    return {
      collection:        collection,
      createCollection:  createCollection,
      createContents:    createContents,
      deleteCollections: deleteCollections,
      modified:          modified,
      quota:             quota,
    };
  },

  _pruneExpired: function _pruneExpired() {
    let now = Date.now();

    for each (let user in this.users) {
      for each (let collection in user.collections) {
        for each (let bso in collection.bsos()) {
          // ttl === 0 is a special case, so we can't simply !ttl.
          if (typeof(bso.ttl) != "number") {
            continue;
          }

          let ttlDate = bso.modified + (bso.ttl * 1000);
          if (ttlDate < now) {
            this._log.info("Deleting BSO because TTL expired: " + bso.id);
            bso.delete();
          }
        }
      }
    }
  },

  /*
   * Regular expressions for splitting up Storage request paths.
   * Storage URLs are of the form:
   *   /$apipath/$version/$userid/$further
   * where $further is usually:
   *   storage/$collection/$bso
   * or
   *   storage/$collection
   * or
   *   info/$op
   *
   * We assume for the sake of simplicity that $apipath is empty.
   *
   * N.B., we don't follow any kind of username spec here, because as far as I
   * can tell there isn't one. See Bug 689671. Instead we follow the Python
   * server code.
   *
   * Path: [all, version, first, rest]
   * Storage: [all, collection?, id?]
   */
  pathRE: /^\/([0-9]+(?:\.[0-9]+)?)(?:\/([0-9]+)\/([^\/]+)(?:\/(.+))?)?$/,
  storageRE: /^([-_a-zA-Z0-9]+)(?:\/([-_a-zA-Z0-9]+)\/?)?$/,

  defaultHeaders: {},

  /**
   * HTTP response utility.
   */
  respond: function respond(req, resp, code, status, body, headers, timestamp) {
    this._log.info("Response: " + code + " " + status);
    resp.setStatusLine(req.httpVersion, code, status);
    for each (let [header, value] in Iterator(headers || this.defaultHeaders)) {
      resp.setHeader(header, value, false);
    }

    if (timestamp) {
      resp.setHeader("X-Timestamp", "" + timestamp, false);
    }

    if (body) {
      resp.bodyOutputStream.write(body, body.length);
    }
  },

  /**
   * This is invoked by the HttpServer. `this` is bound to the StorageServer;
   * `handler` is the HttpServer's handler.
   *
   * TODO: need to use the correct Storage API response codes and errors here.
   */
  handleDefault: function handleDefault(handler, req, resp) {
    this.requestCount++;
    let timestamp = new_timestamp();
    try {
      this._handleDefault(handler, req, resp, timestamp);
    } catch (e) {
      if (e instanceof HttpError) {
        this.respond(req, resp, e.code, e.description, "", {}, timestamp);
      } else {
        this._log.warn(CommonUtils.exceptionStr(e));
        throw e;
      }
    }
  },

  _handleDefault: function _handleDefault(handler, req, resp, timestamp) {
    let path = req.path;
    if (req.queryString.length) {
      path += "?" + req.queryString;
    }

    this._log.debug("StorageServer: Handling request: " + req.method + " " +
                    path);

    if (this.callback.onRequest) {
      this.callback.onRequest(req);
    }

    // Prune expired records for all users at top of request. This is the
    // easiest way to process TTLs since all requests go through here.
    this._pruneExpired();

    req.timestamp = timestamp;
    resp.setHeader("X-Timestamp", "" + timestamp, false);

    let parts = this.pathRE.exec(req.path);
    if (!parts) {
      this._log.debug("StorageServer: Unexpected request: bad URL " + req.path);
      throw HTTP_404;
    }

    let [all, version, userPath, first, rest] = parts;
    if (version != STORAGE_API_VERSION) {
      this._log.debug("StorageServer: Unknown version.");
      throw HTTP_404;
    }

    let username;

    // By default, the server requires users to be authenticated. When a
    // request arrives, the user must have been previously configured and
    // the request must have authentication. In "allow all users" mode, we
    // take the username from the URL, create the user on the fly, and don't
    // perform any authentication.
    if (!this.allowAllUsers) {
      // Enforce authentication.
      if (!req.hasHeader("authorization")) {
        this.respond(req, resp, 401, "Authorization Required", "{}", {
          "WWW-Authenticate": 'Basic realm="secret"'
        });
        return;
      }

      let ensureUserExists = function ensureUserExists(username) {
        if (this.userExists(username)) {
          return;
        }

        this._log.info("StorageServer: Unknown user: " + username);
        throw HTTP_401;
      }.bind(this);

      let auth = req.getHeader("authorization");
      this._log.debug("Authorization: " + auth);

      if (auth.indexOf("Basic ") == 0) {
        let decoded = CommonUtils.safeAtoB(auth.substr(6));
        this._log.debug("Decoded Basic Auth: " + decoded);
        let [user, password] = decoded.split(":", 2);

        if (!password) {
          this._log.debug("Malformed HTTP Basic Authorization header: " + auth);
          throw HTTP_400;
        }

        this._log.debug("Got HTTP Basic auth for user: " + user);
        ensureUserExists(user);
        username = user;

        if (this.users[user].password != password) {
          this._log.debug("StorageServer: Provided password is not correct.");
          throw HTTP_401;
        }
      // TODO support token auth.
      } else {
        this._log.debug("Unsupported HTTP authorization type: " + auth);
        throw HTTP_500;
      }
    // All users mode.
    } else {
      // Auto create user with dummy password.
      if (!this.userExists(userPath)) {
        this.registerUser(userPath, "DUMMY-PASSWORD-*&%#");
      }

      username = userPath;
    }

    // Hand off to the appropriate handler for this path component.
    if (first in this.toplevelHandlers) {
      let handler = this.toplevelHandlers[first];
      try {
        return handler.call(this, handler, req, resp, version, username, rest);
      } catch (ex) {
        this._log.warn("Got exception during request: " +
                       CommonUtils.exceptionStr(ex));
        throw ex;
      }
    }
    this._log.debug("StorageServer: Unknown top-level " + first);
    throw HTTP_404;
  },

  /**
   * Collection of the handler methods we use for top-level path components.
   */
  toplevelHandlers: {
    "storage": function handleStorage(handler, req, resp, version, username,
                                      rest) {
      let respond = this.respond.bind(this, req, resp);
      if (!rest || !rest.length) {
        this._log.debug("StorageServer: top-level storage " +
                        req.method + " request.");

        if (req.method != "DELETE") {
          respond(405, "Method Not Allowed", null, {"Allow": "DELETE"});
          return;
        }

        this.user(username).deleteCollections();

        respond(204, "No Content");
        return;
      }

      let match = this.storageRE.exec(rest);
      if (!match) {
        this._log.warn("StorageServer: Unknown storage operation " + rest);
        throw HTTP_404;
      }
      let [all, collection, bsoID] = match;
      let coll = this.getCollection(username, collection);
      let collectionExisted = !!coll;

      switch (req.method) {
        case "GET":
          // Tried to GET on a collection that doesn't exist.
          if (!coll) {
            respond(404, "Not Found");
            return;
          }

          // No BSO URL parameter goes to collection handler.
          if (!bsoID) {
            return coll.collectionHandler(req, resp);
          }

          // Handle non-existent BSO.
          let bso = coll.bso(bsoID);
          if (!bso) {
            respond(404, "Not Found");
            return;
          }

          // Proxy to BSO handler.
          return bso.getHandler(req, resp);

        case "DELETE":
          // Collection doesn't exist.
          if (!coll) {
            respond(404, "Not Found");
            return;
          }

          // Deleting a specific BSO.
          if (bsoID) {
            let bso = coll.bso(bsoID);

            // BSO does not exist on the server. Nothing to do.
            if (!bso) {
              respond(404, "Not Found");
              return;
            }

            if (req.hasHeader("x-if-unmodified-since")) {
              let modified = parseInt(req.getHeader("x-if-unmodified-since"));
              CommonUtils.ensureMillisecondsTimestamp(modified);

              if (bso.modified > modified) {
                respond(412, "Precondition Failed");
                return;
              }
            }

            bso.delete();
            coll.timestamp = req.timestamp;
            this.callback.onItemDeleted(username, collection, bsoID);
            respond(204, "No Content");
            return;
          }

          // Proxy to collection handler.
          coll.collectionHandler(req, resp);

          // Spot if this is a DELETE for some IDs, and don't blow away the
          // whole collection!
          //
          // We already handled deleting the BSOs by invoking the deleted
          // collection's handler. However, in the case of
          //
          //   DELETE storage/foobar
          //
          // we also need to remove foobar from the collections map. This
          // clause tries to differentiate the above request from
          //
          //  DELETE storage/foobar?ids=foo,baz
          //
          // and do the right thing.
          // TODO: less hacky method.
          if (-1 == req.queryString.indexOf("ids=")) {
            // When you delete the entire collection, we drop it.
            this._log.debug("Deleting entire collection.");
            delete this.users[username].collections[collection];
            this.callback.onCollectionDeleted(username, collection);
          }

          // Notify of item deletion.
          let deleted = resp.deleted || [];
          for (let i = 0; i < deleted.length; ++i) {
            this.callback.onItemDeleted(username, collection, deleted[i]);
          }
          return;

        case "POST":
        case "PUT":
          // Auto-create collection if it doesn't exist.
          if (!coll) {
            coll = this.createCollection(username, collection);
          }

          try {
            if (bsoID) {
              let bso = coll.bso(bsoID);
              if (!bso) {
                this._log.trace("StorageServer: creating BSO " + collection +
                                "/" + bsoID);
                try {
                  bso = coll.insert(bsoID);
                } catch (ex) {
                  return sendMozSvcError(req, resp, "8");
                }
              }

              bso.putHandler(req, resp);

              coll.timestamp = req.timestamp;
              return resp;
            }

            return coll.collectionHandler(req, resp);
          } catch (ex) {
            if (ex instanceof HttpError) {
              if (!collectionExisted) {
                this.deleteCollection(username, collection);
              }
            }

            throw ex;
          }

        default:
          throw new Error("Request method " + req.method + " not implemented.");
      }
    },

    "info": function handleInfo(handler, req, resp, version, username, rest) {
      switch (rest) {
        case "collections":
          return this.handleInfoCollections(req, resp, username);

        case "collection_counts":
          return this.handleInfoCounts(req, resp, username);

        case "collection_usage":
          return this.handleInfoUsage(req, resp, username);

        case "quota":
          return this.handleInfoQuota(req, resp, username);

        default:
          this._log.warn("StorageServer: Unknown info operation " + rest);
          throw HTTP_404;
      }
    }
  },

  handleInfoConditional: function handleInfoConditional(request, response,
                                                        user) {
    if (!request.hasHeader("x-if-modified-since")) {
      return false;
    }

    let requestModified = request.getHeader("x-if-modified-since");
    requestModified = parseInt(requestModified, 10);

    let serverModified = this.newestCollectionTimestamp(user);

    this._log.info("Server mtime: " + serverModified + "; Client modified: " +
                   requestModified);
    if (serverModified > requestModified) {
      return false;
    }

    this.respond(request, response, 304, "Not Modified", null, {
      "X-Last-Modified": "" + serverModified
    });

    return true;
  },

  handleInfoCollections: function handleInfoCollections(request, response,
                                                        user) {
    if (this.handleInfoConditional(request, response, user)) {
      return;
    }

    let info = this.infoCollections(user);
    let body = JSON.stringify(info);
    this.respond(request, response, 200, "OK", body, {
      "Content-Type":    "application/json",
      "X-Last-Modified": "" + this.newestCollectionTimestamp(user),
    });
  },

  handleInfoCounts: function handleInfoCounts(request, response, user) {
    if (this.handleInfoConditional(request, response, user)) {
      return;
    }

    let counts = this.infoCounts(user);
    let body = JSON.stringify(counts);

    this.respond(request, response, 200, "OK", body, {
      "Content-Type":    "application/json",
      "X-Last-Modified": "" + this.newestCollectionTimestamp(user),
    });
  },

  handleInfoUsage: function handleInfoUsage(request, response, user) {
    if (this.handleInfoConditional(request, response, user)) {
      return;
    }

    let body = JSON.stringify(this.infoUsage(user));
    this.respond(request, response, 200, "OK", body, {
      "Content-Type":    "application/json",
      "X-Last-Modified": "" + this.newestCollectionTimestamp(user),
    });
  },

  handleInfoQuota: function handleInfoQuota(request, response, user) {
    if (this.handleInfoConditional(request, response, user)) {
      return;
    }

    let body = JSON.stringify(this.infoQuota(user));
    this.respond(request, response, 200, "OK", body, {
      "Content-Type":    "application/json",
      "X-Last-Modified": "" + this.newestCollectionTimestamp(user),
    });
  },
};

/**
 * Helper to create a storage server for a set of users.
 *
 * Each user is specified by a map of username to password.
 */
function storageServerForUsers(users, contents, callback) {
  let server = new StorageServer(callback);
  for (let [user, pass] in Iterator(users)) {
    server.registerUser(user, pass);
    server.createContents(user, contents);
  }
  server.start();
  return server;
}
