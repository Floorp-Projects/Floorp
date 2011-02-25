function httpd_setup (handlers) {
  let server = new nsHttpServer();
  for (let path in handlers) {
    server.registerPathHandler(path, handlers[path]);
  }
  server.start(8080);
  return server;
}

function httpd_handler(statusCode, status, body) {
  return function(request, response) {
    response.setStatusLine(request.httpVersion, statusCode, status);
    response.bodyOutputStream.write(body, body.length);
  };
}

function httpd_basic_auth_handler(body, metadata, response) {
  // no btoa() in xpcshell.  it's guest:guest
  if (metadata.hasHeader("Authorization") &&
      metadata.getHeader("Authorization") == "Basic Z3Vlc3Q6Z3Vlc3Q=") {
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
  this.id = id;
  if (!initialPayload) {
    return;
  }

  if (typeof initialPayload == "object") {
    initialPayload = JSON.stringify(initialPayload);
  }
  this.payload = initialPayload;
  this.modified = Date.now() / 1000;
}
ServerWBO.prototype = {

  get data() {
    return JSON.parse(this.payload);
  },

  get: function() {
    return JSON.stringify(this, ['id', 'modified', 'payload']);
  },

  put: function(input) {
    input = JSON.parse(input);
    this.payload = input.payload;
    this.modified = Date.now() / 1000;
  },

  delete: function() {
    delete this.payload;
    delete this.modified;
  },

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
          break;

        case "DELETE":
          self.delete();
          body = JSON.stringify(Date.now() / 1000);
          break;
      }
      response.setHeader('X-Weave-Timestamp', ''+Date.now()/1000, false);
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
        wbo.modified = Date.now() / 1000;
        success.push(record.id);
      } else {
        failed[record.id] = "no wbo configured";
      }
    }
    return {modified: Date.now() / 1000,
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

  handler: function() {
    let self = this;

    return function(request, response) {
      var statusCode = 200;
      var status = "OK";
      var body;

      // Parse queryString
      let options = {};
      for each (let chunk in request.queryString.split('&')) {
        if (!chunk) {
          continue;
        }
        chunk = chunk.split('=');
        if (chunk.length == 1) {
          options[chunk[0]] = "";
        } else {
          options[chunk[0]] = chunk[1];
        }
      }
      if (options.ids) {
        options.ids = options.ids.split(',');
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
          break;

        case "DELETE":
          self.delete(options);
          body = JSON.stringify(Date.now() / 1000);
          break;
      }
      response.setHeader('X-Weave-Timestamp', ''+Date.now()/1000, false);
      response.setStatusLine(request.httpVersion, statusCode, status);
      response.bodyOutputStream.write(body, body.length);
    };
  }

};

/*
 * Test setup helpers.
 */
function sync_httpd_setup(handlers) {
  handlers["/1.0/foo/storage/meta/global"]
      = (new ServerWBO('global', {})).handler();
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
  function update_collection(coll) {
    let timestamp = Date.now() / 1000;
    collections[coll] = timestamp;
  }

  /*
   * Invoke a handler, updating the collection's modified timestamp unless
   * it's a GET request.
   */
  function with_updated_collection(coll, f) {
    return function(request, response) {
      if (request.method != "GET")
        update_collection(coll);
      f.call(this, request, response);
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
        
    response.setHeader('X-Weave-Timestamp', ''+Date.now()/1000, false);
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.bodyOutputStream.write(body, body.length);
  }
  
  return {"collections": collections,
          "handler": info_collections,
          "with_updated_collection": with_updated_collection,
          "update_collection": update_collection};
}
