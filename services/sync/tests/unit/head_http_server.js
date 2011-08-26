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
          response.newModified = self.modified;
          break;

        case "DELETE":
          self.delete();
          let ts = new_timestamp();
          body = JSON.stringify(ts);
          response.newModified = ts;
          break;
      }
      response.setHeader("X-Weave-Timestamp", "" + new_timestamp(), false);
      response.setStatusLine(request.httpVersion, statusCode, status);
      response.bodyOutputStream.write(body, body.length);
    };
  }

};


/*
 * Represent a collection on the server.  The 'wbo' attribute is a
 * mapping of id -> ServerWBO objects.
 * 
 * Note that if you want these records to be accessible individually,
 * you need to register their handlers with the server separately!
 * 
 * Passing `true` for acceptNew will allow POSTs of new WBOs to this
 * collection. New WBOs will be created and wired in on the fly.
 */
function ServerCollection(wbos, acceptNew) {
  this.wbos = wbos || {};
  this.acceptNew = acceptNew || false;
}
ServerCollection.prototype = {

  _inResultSet: function(wbo, options) {
    return wbo.payload
           && (!options.ids || (options.ids.indexOf(wbo.id) != -1))
           && (!options.newer || (wbo.modified > options.newer));
  },

  count: function(options) {
    options = options || {};
    let c = 0;
    for (let [id, wbo] in Iterator(this.wbos)) {
      if (wbo.modified && this._inResultSet(wbo, options)) {
        c++;
      }
    }
    return c;
  },

  get: function(options) {
    let result;
    if (options.full) {
      let data = [wbo.get() for ([id, wbo] in Iterator(this.wbos))
                            // Drop deleted.
                            if (wbo.modified &&
                                this._inResultSet(wbo, options))];
      if (options.limit) {
        data = data.slice(0, options.limit);
      }
      // Our implementation of application/newlines
      result = data.join("\n") + "\n";
    } else {
      let data = [id for ([id, wbo] in Iterator(this.wbos))
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
      let wbo = this.wbos[record.id];
      if (!wbo && this.acceptNew) {
        _("Creating WBO " + JSON.stringify(record.id) + " on the fly.");
        wbo = new ServerWBO(record.id);
        this.wbos[record.id] = wbo;
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
    for (let [id, wbo] in Iterator(this.wbos)) {
      if (this._inResultSet(wbo, options)) {
        _("Deleting " + JSON.stringify(wbo));
        wbo.delete();
      }
    }
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
          self.delete(options);
          let ts = new_timestamp();
          body = JSON.stringify(ts);
          response.newModified = ts;
          break;
      }
      response.setHeader("X-Weave-Timestamp",
                         "" + new_timestamp(),
                         false);
      response.setStatusLine(request.httpVersion, statusCode, status);
      response.bodyOutputStream.write(body, body.length);
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
