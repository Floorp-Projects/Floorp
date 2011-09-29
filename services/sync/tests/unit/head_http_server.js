// Shared logging for all HTTP server functions.
Cu.import("resource://services-sync/log4moz.js");
const SYNC_HTTP_LOGGER = "Sync.Test.Server";

// Use the same method that record.js does, which mirrors the server.
// The server returns timestamps with 1/100 sec granularity. Note that this is
// subject to change: see Bug 650435.
function new_timestamp() {
  return Math.round(Date.now() / 10) / 100;
}

function httpd_setup (handlers) {
  let server = new nsHttpServer();
  let port   = 8080;
  for (let path in handlers) {
    server.registerPathHandler(path, handlers[path]);
  }
  try {
    server.start(port);
  } catch (ex) {
    _("==========================================");
    _("Got exception starting HTTP server on port " + port);
    _("Error: " + Utils.exceptionStr(ex));
    _("Is there a process already listening on port " + port + "?");
    _("==========================================");
    do_throw(ex);
  }

  return server;
}

function httpd_handler(statusCode, status, body) {
  return function handler(request, response) {
    // Allow test functions to inspect the request.
    request.body = readBytesFromInputStream(request.bodyInputStream);
    handler.request = request;

    response.setStatusLine(request.httpVersion, statusCode, status);
    if (body) {
      response.bodyOutputStream.write(body, body.length);
    }
  };
}

function basic_auth_header(user, password) {
  return "Basic " + btoa(user + ":" + Utils.encodeUTF8(password));
}

function basic_auth_matches(req, user, password) {
  return req.hasHeader("Authorization") &&
         (req.getHeader("Authorization") == basic_auth_header(user, password));
}

function httpd_basic_auth_handler(body, metadata, response) {
  if (basic_auth_matches(metadata, "guest", "guest")) {
    response.setStatusLine(metadata.httpVersion, 200, "OK, authorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
  } else {
    body = "This path exists and is protected - failed";
    response.setStatusLine(metadata.httpVersion, 401, "Unauthorized");
    response.setHeader("WWW-Authenticate", 'Basic realm="secret"', false);
  }
  response.bodyOutputStream.write(body, body.length);
}

/*
 * Read bytes string from an nsIInputStream.  If 'count' is omitted,
 * all available input is read.
 */
function readBytesFromInputStream(inputStream, count) {
  var BinaryInputStream = Components.Constructor(
      "@mozilla.org/binaryinputstream;1",
      "nsIBinaryInputStream",
      "setInputStream");
  if (!count) {
    count = inputStream.available();
  }
  return new BinaryInputStream(inputStream).readBytes(count);
}

/*
 * Represent a WBO on the server
 */
function ServerWBO(id, initialPayload) {
  if (!id) {
    throw "No ID for ServerWBO!";
  }
  this.id = id;
  if (!initialPayload) {
    return;
  }

  if (typeof initialPayload == "object") {
    initialPayload = JSON.stringify(initialPayload);
  }
  this.payload = initialPayload;
  this.modified = new_timestamp();
}
ServerWBO.prototype = {

  get data() {
    return JSON.parse(this.payload);
  },

  get: function() {
    return JSON.stringify(this, ["id", "modified", "payload"]);
  },

  put: function(input) {
    input = JSON.parse(input);
    this.payload = input.payload;
    this.modified = new_timestamp();
  },

  delete: function() {
    delete this.payload;
    delete this.modified;
  },

  // This handler sets `newModified` on the response body if the collection
  // timestamp has changed. This allows wrapper handlers to extract information
  // that otherwise would exist only in the body stream.
  handler: function() {
    let self = this;

    return function(request, response) {
      var statusCode = 200;
      var status = "OK";
      var body;

      switch(request.method) {
        case "GET":
          if (self.payload) {
            body = self.get();
          } else {
            statusCode = 404;
            status = "Not Found";
            body = "Not Found";
          }
          break;

        case "PUT":
          self.put(readBytesFromInputStream(request.bodyInputStream));
          body = JSON.stringify(self.modified);
          response.setHeader("Content-Type", "application/json");
          response.newModified = self.modified;
          break;

        case "DELETE":
          self.delete();
          let ts = new_timestamp();
          body = JSON.stringify(ts);
          response.setHeader("Content-Type", "application/json");
          response.newModified = ts;
          break;
      }
      response.setHeader("X-Weave-Timestamp", "" + new_timestamp(), false);
      response.setStatusLine(request.httpVersion, statusCode, status);
      response.bodyOutputStream.write(body, body.length);
    };
  }

};


/**
 * Represent a collection on the server. The '_wbos' attribute is a
 * mapping of id -> ServerWBO objects.
 *
 * Note that if you want these records to be accessible individually,
 * you need to register their handlers with the server separately, or use a
 * containing HTTP server that will do so on your behalf.
 *
 * @param wbos
 *        An object mapping WBO IDs to ServerWBOs.
 * @param acceptNew
 *        If true, POSTs to this collection URI will result in new WBOs being
 *        created and wired in on the fly.
 * @param timestamp
 *        An optional timestamp value to initialize the modified time of the
 *        collection. This should be in the format returned by new_timestamp().
 *
 * @return the new ServerCollection instance.
 *
 */
function ServerCollection(wbos, acceptNew, timestamp) {
  this._wbos = wbos || {};
  this.acceptNew = acceptNew || false;

  /*
   * Track modified timestamp.
   * We can't just use the timestamps of contained WBOs: an empty collection
   * has a modified time.
   */
  this.timestamp = timestamp || new_timestamp();
  this._log = Log4Moz.repository.getLogger(SYNC_HTTP_LOGGER);
}
ServerCollection.prototype = {

  /**
   * Convenience accessor for our WBO keys.
   * Excludes deleted items, of course.
   *
   * @param filter
   *        A predicate function (applied to the ID and WBO) which dictates
   *        whether to include the WBO's ID in the output.
   *
   * @return an array of IDs.
   */
  keys: function keys(filter) {
    return [id for ([id, wbo] in Iterator(this._wbos))
               if (wbo.payload &&
                   (!filter || filter(id, wbo)))];
  },

  /**
   * Convenience method to get an array of WBOs.
   * Optionally provide a filter function.
   *
   * @param filter
   *        A predicate function, applied to the WBO, which dictates whether to
   *        include the WBO in the output.
   *
   * @return an array of ServerWBOs.
   */
  wbos: function wbos(filter) {
    let os = [wbo for ([id, wbo] in Iterator(this._wbos))
              if (wbo.payload)];
    if (filter) {
      return os.filter(filter);
    }
    return os;
  },

  /**
   * Convenience method to get an array of parsed ciphertexts.
   *
   * @return an array of the payloads of each stored WBO.
   */
  payloads: function () {
    return this.wbos().map(function (wbo) {
      return JSON.parse(JSON.parse(wbo.payload).ciphertext);
    });
  },

  // Just for syntactic elegance.
  wbo: function wbo(id) {
    return this._wbos[id];
  },

  payload: function payload(id) {
    return this.wbo(id).payload;
  },

  /**
   * Insert the provided WBO under its ID.
   *
   * @return the provided WBO.
   */
  insertWBO: function insertWBO(wbo) {
    return this._wbos[wbo.id] = wbo;
  },

  /**
   * Insert the provided payload as part of a new ServerWBO with the provided
   * ID.
   *
   * @param id
   *        The GUID for the WBO.
   * @param payload
   *        The payload, as provided to the ServerWBO constructor.
   *
   * @return the inserted WBO.
   */
  insert: function insert(id, payload) {
    return this.insertWBO(new ServerWBO(id, payload));
  },

  _inResultSet: function(wbo, options) {
    return wbo.payload
           && (!options.ids || (options.ids.indexOf(wbo.id) != -1))
           && (!options.newer || (wbo.modified > options.newer));
  },

  count: function(options) {
    options = options || {};
    let c = 0;
    for (let [id, wbo] in Iterator(this._wbos)) {
      if (wbo.modified && this._inResultSet(wbo, options)) {
        c++;
      }
    }
    return c;
  },

  get: function(options) {
    let result;
    if (options.full) {
      let data = [wbo.get() for ([id, wbo] in Iterator(this._wbos))
                            // Drop deleted.
                            if (wbo.modified &&
                                this._inResultSet(wbo, options))];
      if (options.limit) {
        data = data.slice(0, options.limit);
      }
      // Our implementation of application/newlines
      result = data.join("\n") + "\n";
    } else {
      let data = [id for ([id, wbo] in Iterator(this._wbos))
                     if (this._inResultSet(wbo, options))];
      if (options.limit) {
        data = data.slice(0, options.limit);
      }
      result = JSON.stringify(data);
    }
    return result;
  },

  post: function(input) {
    input = JSON.parse(input);
    let success = [];
    let failed = {};

    // This will count records where we have an existing ServerWBO
    // registered with us as successful and all other records as failed.
    for each (let record in input) {
      let wbo = this.wbo(record.id);
      if (!wbo && this.acceptNew) {
        this._log.debug("Creating WBO " + JSON.stringify(record.id) +
                        " on the fly.");
        wbo = new ServerWBO(record.id);
        this.insertWBO(wbo);
      }
      if (wbo) {
        wbo.payload = record.payload;
        wbo.modified = new_timestamp();
        success.push(record.id);
      } else {
        failed[record.id] = "no wbo configured";
      }
    }
    return {modified: new_timestamp(),
            success: success,
            failed: failed};
  },

  delete: function(options) {
    let deleted = [];
    for (let [id, wbo] in Iterator(this._wbos)) {
      if (this._inResultSet(wbo, options)) {
        this._log.debug("Deleting " + JSON.stringify(wbo));
        deleted.push(wbo.id);
        wbo.delete();
      }
    }
    return deleted;
  },

  // This handler sets `newModified` on the response body if the collection
  // timestamp has changed.
  handler: function() {
    let self = this;

    return function(request, response) {
      var statusCode = 200;
      var status = "OK";
      var body;

      // Parse queryString
      let options = {};
      for each (let chunk in request.queryString.split("&")) {
        if (!chunk) {
          continue;
        }
        chunk = chunk.split("=");
        if (chunk.length == 1) {
          options[chunk[0]] = "";
        } else {
          options[chunk[0]] = chunk[1];
        }
      }
      if (options.ids) {
        options.ids = options.ids.split(",");
      }
      if (options.newer) {
        options.newer = parseFloat(options.newer);
      }
      if (options.limit) {
        options.limit = parseInt(options.limit, 10);
      }

      switch(request.method) {
        case "GET":
          body = self.get(options);
          break;

        case "POST":
          let res = self.post(readBytesFromInputStream(request.bodyInputStream));
          body = JSON.stringify(res);
          response.newModified = res.modified;
          break;

        case "DELETE":
          self._log.debug("Invoking ServerCollection.DELETE.");
          let deleted = self.delete(options);
          let ts = new_timestamp();
          body = JSON.stringify(ts);
          response.newModified = ts;
          response.deleted = deleted;
          break;
      }
      response.setHeader("X-Weave-Timestamp",
                         "" + new_timestamp(),
                         false);
      response.setStatusLine(request.httpVersion, statusCode, status);
      response.bodyOutputStream.write(body, body.length);

      // Update the collection timestamp to the appropriate modified time.
      // This is either a value set by the handler, or the current time.
      if (request.method != "GET") {
        this.timestamp = (response.newModified >= 0) ?
                         response.newModified :
                         new_timestamp();
      }
    };
  }

};

/*
 * Test setup helpers.
 */
function sync_httpd_setup(handlers) {
  handlers["/1.1/foo/storage/meta/global"]
      = (new ServerWBO("global", {})).handler();
  return httpd_setup(handlers);
}

/*
 * Track collection modified times. Return closures.
 */
function track_collections_helper() {
  
  /*
   * Our tracking object.
   */
  let collections = {};

  /*
   * Update the timestamp of a collection.
   */
  function update_collection(coll, ts) {
    _("Updating collection " + coll + " to " + ts);
    let timestamp = ts || new_timestamp();
    collections[coll] = timestamp;
  }

  /*
   * Invoke a handler, updating the collection's modified timestamp unless
   * it's a GET request.
   */
  function with_updated_collection(coll, f) {
    return function(request, response) {
      f.call(this, request, response);

      // Update the collection timestamp to the appropriate modified time.
      // This is either a value set by the handler, or the current time.
      if (request.method != "GET") {
        update_collection(coll, response.newModified)
      }
    };
  }

  /*
   * Return the info/collections object.
   */
  function info_collections(request, response) {
    let body = "Error.";
    switch(request.method) {
      case "GET":
        body = JSON.stringify(collections);
        break;
      default:
        throw "Non-GET on info_collections.";
    }
        
    response.setHeader("X-Weave-Timestamp",
                       "" + new_timestamp(),
                       false);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(body, body.length);
  }
  
  return {"collections": collections,
          "handler": info_collections,
          "with_updated_collection": with_updated_collection,
          "update_collection": update_collection};
}
