function httpd_setup (handlers) {
  let server = new nsHttpServer();
  for (let path in handlers) {
    server.registerPathHandler(path, handlers[path]);
  }
  server.start(8080);
  return server;
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
 * Create and upload public + private key pair. You probably want to enable
 * FakeCryptoService first, otherwise this will be very expensive.
 */
function createAndUploadKeypair() {
  let storageURL = Svc.Prefs.get("clusterURL") + Svc.Prefs.get("storageAPI")
                   + "/" + ID.get("WeaveID").username + "/storage/";

  PubKeys.defaultKeyUri = storageURL + "keys/pubkey";
  PrivKeys.defaultKeyUri = storageURL + "keys/privkey";
  let keys = PubKeys.createKeypair(ID.get("WeaveCryptoID"),
                                   PubKeys.defaultKeyUri,
                                   PrivKeys.defaultKeyUri);
  PubKeys.uploadKeypair(keys);
}

/*
 * Create and upload an engine's symmetric key.
 */
function createAndUploadSymKey(url) {
  let symkey = Svc.Crypto.generateRandomKey();
  let pubkey = PubKeys.getDefaultKey();
  let meta = new CryptoMeta(url);
  meta.addUnwrappedKey(pubkey, symkey);
  let res = new Resource(meta.uri);
  res.put(meta);
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
 */
function ServerCollection(wbos) {
  this.wbos = wbos || {};
}
ServerCollection.prototype = {

  _inResultSet: function(wbo, options) {
    return ((!options.ids || (options.ids.indexOf(wbo.id) != -1))
            && (!options.newer || (wbo.modified > options.newer)));
  },

  get: function(options) {
    let result;
    if (options.full) {
      let data = [wbo.get() for ([id, wbo] in Iterator(this.wbos))
                            if (this._inResultSet(wbo, options))];
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
