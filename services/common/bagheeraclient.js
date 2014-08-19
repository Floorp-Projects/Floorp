/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains a client API for the Bagheera data storage service.
 *
 * Information about Bagheera is available at
 * https://github.com/mozilla-metrics/bagheera
 */

#ifndef MERGED_COMPARTMENT

"use strict";

this.EXPORTED_SYMBOLS = [
  "BagheeraClient",
  "BagheeraClientRequestResult",
];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

#endif

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Log.jsm");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-common/utils.js");


/**
 * Represents the result of a Bagheera request.
 */
this.BagheeraClientRequestResult = function BagheeraClientRequestResult() {
  this.transportSuccess = false;
  this.serverSuccess = false;
  this.request = null;
};

Object.freeze(BagheeraClientRequestResult.prototype);


/**
 * Wrapper around RESTRequest so logging is sane.
 */
function BagheeraRequest(uri) {
  RESTRequest.call(this, uri);

  this._log = Log.repository.getLogger("Services.BagheeraClient");
  this._log.level = Log.Level.Debug;
}

BagheeraRequest.prototype = Object.freeze({
  __proto__: RESTRequest.prototype,
});


/**
 * Create a new Bagheera client instance.
 *
 * Each client is associated with a specific Bagheera HTTP URI endpoint.
 *
 * @param baseURI
 *        (string) The base URI of the Bagheera HTTP endpoint.
 */
this.BagheeraClient = function BagheeraClient(baseURI) {
  if (!baseURI) {
    throw new Error("baseURI argument must be defined.");
  }

  this._log = Log.repository.getLogger("Services.BagheeraClient");
  this._log.level = Log.Level.Debug;

  this.baseURI = baseURI;

  if (!baseURI.endsWith("/")) {
    this.baseURI += "/";
  }
};

BagheeraClient.prototype = Object.freeze({
  /**
   * Channel load flags for all requests.
   *
   * Caching is not applicable, so we bypass and disable it. We also
   * ignore any cookies that may be present for the domain because
   * Bagheera does not utilize cookies and the release of cookies may
   * inadvertantly constitute unncessary information disclosure.
   */
  _loadFlags: Ci.nsIRequest.LOAD_BYPASS_CACHE |
              Ci.nsIRequest.INHIBIT_CACHING |
              Ci.nsIRequest.LOAD_ANONYMOUS,

  DEFAULT_TIMEOUT_MSEC: 5 * 60 * 1000, // 5 minutes.

  _RE_URI_IDENTIFIER: /^[a-zA-Z0-9_-]+$/,

  /**
   * Upload a JSON payload to the server.
   *
   * The return value is a Promise which will be resolved with a
   * BagheeraClientRequestResult when the request has finished.
   *
   * @param namespace
   *        (string) The namespace to post this data to.
   * @param id
   *        (string) The ID of the document being uploaded. This is typically
   *        a UUID in hex form.
   * @param payload
   *        (string|object) Data to upload. Can be specified as a string (which
   *        is assumed to be JSON) or an object. If an object, it will be fed into
   *        JSON.stringify() for serialization.
   * @param options
   *        (object) Extra options to control behavior. Recognized properties:
   *
   *          deleteIDs -- (array) Old document IDs to delete as part of
   *            upload. If not specified, no old documents will be deleted as
   *            part of upload. The array values are typically UUIDs in hex
   *            form.
   *
   *          telemetryCompressed -- (string) Telemetry histogram to record
   *            compressed size of payload under. If not defined, no telemetry
   *            data for the compressed size will be recorded.
   *
   * @return Promise<BagheeraClientRequestResult>
   */
  uploadJSON: function uploadJSON(namespace, id, payload, options={}) {
    if (!namespace) {
      throw new Error("namespace argument must be defined.");
    }

    if (!id) {
      throw new Error("id argument must be defined.");
    }

    if (!payload) {
      throw new Error("payload argument must be defined.");
    }

    if (options && typeof(options) != "object") {
      throw new Error("Unexpected type for options argument. Expected object. " +
                      "Got: " + typeof(options));
    }

    let uri = this._submitURI(namespace, id);

    let data = payload;

    if (typeof(payload) == "object") {
      data = JSON.stringify(payload);
    }

    if (typeof(data) != "string") {
      throw new Error("Unknown type for payload: " + typeof(data));
    }

    this._log.info("Uploading data to " + uri);

    let request = new BagheeraRequest(uri);
    request.loadFlags = this._loadFlags;
    request.timeout = this.DEFAULT_TIMEOUT_MSEC;

    // Since API changed, throw on old API usage.
    if ("deleteID" in options) {
      throw new Error("API has changed, use (array) deleteIDs instead");
    }

    let deleteIDs;
    if (options.deleteIDs && options.deleteIDs.length > 0) {
      deleteIDs = options.deleteIDs;
      this._log.debug("Will delete " + deleteIDs.join(", "));
      request.setHeader("X-Obsolete-Document", deleteIDs.join(","));
    }

    let deferred = Promise.defer();

    // The string converter service used by CommonUtils.convertString()
    // silently throws away high bytes. We need to convert the string to
    // consist of only low bytes first.
    data = CommonUtils.encodeUTF8(data);
    data = CommonUtils.convertString(data, "uncompressed", "deflate");
    if (options.telemetryCompressed) {
      try {
        let h = Services.telemetry.getHistogramById(options.telemetryCompressed);
        h.add(data.length);
      } catch (ex) {
        this._log.warn("Unable to record telemetry for compressed payload size: " +
                       CommonUtils.exceptionStr(ex));
      }
    }

    // TODO proper header per bug 807134.
    request.setHeader("Content-Type", "application/json+zlib; charset=utf-8");

    this._log.info("Request body length: " + data.length);

    let result = new BagheeraClientRequestResult();
    result.namespace = namespace;
    result.id = id;
    result.deleteIDs = deleteIDs ? deleteIDs.slice(0) : null;

    request.onComplete = this._onComplete.bind(this, request, deferred, result);
    request.post(data);

    return deferred.promise;
  },

  /**
   * Delete the specified document.
   *
   * @param namespace
   *        (string) Namespace from which to delete the document.
   * @param id
   *        (string) ID of document to delete.
   *
   * @return Promise<BagheeraClientRequestResult>
   */
  deleteDocument: function deleteDocument(namespace, id) {
    let uri = this._submitURI(namespace, id);

    let request = new BagheeraRequest(uri);
    request.loadFlags = this._loadFlags;
    request.timeout = this.DEFAULT_TIMEOUT_MSEC;

    let result = new BagheeraClientRequestResult();
    result.namespace = namespace;
    result.id = id;
    let deferred = Promise.defer();

    request.onComplete = this._onComplete.bind(this, request, deferred, result);
    request.delete();

    return deferred.promise;
  },

  _submitURI: function _submitURI(namespace, id) {
    if (!this._RE_URI_IDENTIFIER.test(namespace)) {
      throw new Error("Illegal namespace name. Must be alphanumeric + [_-]: " +
                      namespace);
    }

    if (!this._RE_URI_IDENTIFIER.test(id)) {
      throw new Error("Illegal id value. Must be alphanumeric + [_-]: " + id);
    }

    return this.baseURI + "1.0/submit/" + namespace + "/" + id;
  },

  _onComplete: function _onComplete(request, deferred, result, error) {
    result.request = request;

    if (error) {
      this._log.info("Transport failure on request: " +
                     CommonUtils.exceptionStr(error));
      result.transportSuccess = false;
      deferred.resolve(result);
      return;
    }

    result.transportSuccess = true;

    let response = request.response;

    switch (response.status) {
      case 200:
      case 201:
        result.serverSuccess = true;
        break;

      default:
        result.serverSuccess = false;

        this._log.info("Received unexpected status code: " + response.status);
        this._log.debug("Response body: " + response.body);
    }

    deferred.resolve(result);
  },
});

