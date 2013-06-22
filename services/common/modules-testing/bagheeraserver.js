/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {utils: Cu} = Components;

this.EXPORTED_SYMBOLS = ["BagheeraServer"];

Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://testing-common/httpd.js");


/**
 * This is an implementation of the Bagheera server.
 *
 * The purpose of the server is to facilitate testing of the Bagheera
 * client and the Firefox Health report. It is *not* meant to be a
 * production grade server.
 *
 * The Bagheera server is essentially a glorified document store.
 */
this.BagheeraServer = function BagheeraServer(serverURI="http://localhost") {
  this._log = Log4Moz.repository.getLogger("metrics.BagheeraServer");

  this.serverURI = serverURI;
  this.server = new HttpServer();
  this.port = 8080;
  this.namespaces = {};

  this.allowAllNamespaces = false;
}

BagheeraServer.prototype = {
  /**
   * Whether this server has a namespace defined.
   *
   * @param ns
   *        (string) Namepsace whose existence to query for.
   * @return bool
   */
  hasNamespace: function hasNamespace(ns) {
    return ns in this.namespaces;
  },

  /**
   * Whether this server has an ID in a particular namespace.
   *
   * @param ns
   *        (string) Namespace to look for item in.
   * @param id
   *        (string) ID of object to look for.
   * @return bool
   */
  hasDocument: function hasDocument(ns, id) {
    let namespace = this.namespaces[ns];

    if (!namespace) {
      return false;
    }

    return id in namespace;
  },

  /**
   * Obtain a document from the server.
   *
   * @param ns
   *        (string) Namespace to retrieve document from.
   * @param id
   *        (string) ID of document to retrieve.
   *
   * @return string The content of the document or null if the document
   *                does not exist.
   */
  getDocument: function getDocument(ns, id) {
    let namespace = this.namespaces[ns];

    if (!namespace) {
      return null;
    }

    return namespace[id];
  },

  /**
   * Set the contents of a document in the server.
   *
   * @param ns
   *        (string) Namespace to add document to.
   * @param id
   *        (string) ID of document being added.
   * @param payload
   *        (string) The content of the document.
   */
  setDocument: function setDocument(ns, id, payload) {
    let namespace = this.namespaces[ns];

    if (!namespace) {
      if (!this.allowAllNamespaces) {
        throw new Error("Namespace does not exist: " + ns);
      }

      this.createNamespace(ns);
      namespace = this.namespaces[ns];
    }

    namespace[id] = payload;
  },

  /**
   * Create a namespace in the server.
   *
   * The namespace will initially be empty.
   *
   * @param ns
   *        (string) The name of the namespace to create.
   */
  createNamespace: function createNamespace(ns) {
    if (ns in this.namespaces) {
      throw new Error("Namespace already exists: " + ns);
    }

    this.namespaces[ns] = {};
  },

  start: function start(port) {
    if (!port) {
      throw new Error("port argument must be specified.");
    }

    this.port = port;

    this.server.registerPrefixHandler("/", this._handleRequest.bind(this));
    this.server.start(port);
  },

  stop: function stop(cb) {
    let handler = {onStopped: cb};

    this.server.stop(handler);
  },

  /**
   * Our root path handler.
   */
  _handleRequest: function _handleRequest(request, response) {
    let path = request.path;
    this._log.info("Received request: " + request.method + " " + path + " " +
                   "HTTP/" + request.httpVersion);

    try {
      if (path.startsWith("/1.0/submit/")) {
        return this._handleV1Submit(request, response,
                                    path.substr("/1.0/submit/".length));
      } else {
        throw HTTP_404;
      }
    } catch (ex) {
      if (ex instanceof HttpError) {
        this._log.info("HttpError thrown: " + ex.code + " " + ex.description);
      } else {
        this._log.warn("Exception processing request: " +
                       CommonUtils.exceptionStr(ex));
      }

      throw ex;
    }
  },

  /**
   * Handles requests to /submit/*.
   */
  _handleV1Submit: function _handleV1Submit(request, response, rest) {
    if (!rest.length) {
      throw HTTP_404;
    }

    let namespace;
    let index = rest.indexOf("/");
    if (index == -1) {
      namespace = rest;
      rest = "";
    } else {
      namespace = rest.substr(0, index);
      rest = rest.substr(index + 1);
    }

    this._handleNamespaceSubmit(namespace, rest, request, response);
  },

  _handleNamespaceSubmit: function _handleNamespaceSubmit(namespace, rest,
                                                          request, response) {
    if (!this.hasNamespace(namespace)) {
      if (!this.allowAllNamespaces) {
        this._log.info("Request to unknown namespace: " + namespace);
        throw HTTP_404;
      }

      this.createNamespace(namespace);
    }

    if (!rest) {
      this._log.info("No ID defined.");
      throw HTTP_404;
    }

    let id = rest;
    if (id.contains("/")) {
      this._log.info("URI has too many components.");
      throw HTTP_404;
    }

    if (request.method == "POST") {
      return this._handleNamespaceSubmitPost(namespace, id, request, response);
    }

    if (request.method == "DELETE") {
      return this._handleNamespaceSubmitDelete(namespace, id, request, response);
    }

    this._log.info("Unsupported HTTP method on namespace handler: " +
                   request.method);
    response.setHeader("Allow", "POST,DELETE");
    throw HTTP_405;
  },

  _handleNamespaceSubmitPost:
    function _handleNamespaceSubmitPost(namespace, id, request, response) {

    this._log.info("Handling data upload for " + namespace + ":" + id);

    let requestBody = CommonUtils.readBytesFromInputStream(request.bodyInputStream);
    this._log.info("Raw body length: " + requestBody.length);

    if (!request.hasHeader("Content-Type")) {
      this._log.info("Request does not have Content-Type header.");
      throw HTTP_400;
    }

    const ALLOWED_TYPES = [
      // TODO proper content types from bug 807134.
      "application/json; charset=utf-8",
      "application/json+zlib; charset=utf-8",
    ];

    let ct = request.getHeader("Content-Type");
    if (ALLOWED_TYPES.indexOf(ct) == -1) {
      this._log.info("Unknown media type: " + ct);
      // Should generate proper HTTP response headers for this error.
      throw HTTP_415;
    }

    if (ct.startsWith("application/json+zlib")) {
      this._log.debug("Uncompressing entity body with deflate.");
      requestBody = CommonUtils.convertString(requestBody, "deflate",
                                              "uncompressed");
    }

    this._log.debug("HTTP request body: " + requestBody);

    let doc;
    try {
      doc = JSON.parse(requestBody);
    } catch(ex) {
      this._log.info("JSON parse error.");
      throw HTTP_400;
    }

    this.namespaces[namespace][id] = doc;

    if (request.hasHeader("X-Obsolete-Document")) {
      let obsolete = request.getHeader("X-Obsolete-Document");
      this._log.info("Deleting from X-Obsolete-Document header: " + obsolete);
      for (let obsolete_id of obsolete.split(",")) {
        delete this.namespaces[namespace][obsolete_id];
      }
    }

    response.setStatusLine(request.httpVersion, 201, "Created");
    response.setHeader("Content-Type", "text/plain");

    let body = id;
    response.bodyOutputStream.write(body, body.length);
  },

  _handleNamespaceSubmitDelete:
    function _handleNamespaceSubmitDelete(namespace, id, request, response) {

    delete this.namespaces[namespace][id];

    let body = id;
    response.bodyOutputStream.write(body, body.length);
  },
};

Object.freeze(BagheeraServer.prototype);
