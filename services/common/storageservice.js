/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains APIs for interacting with the Storage Service API.
 *
 * The specification for the service is available at.
 * http://docs.services.mozilla.com/storage/index.html
 *
 * Nothing about the spec or the service is Sync-specific. And, that is how
 * these APIs are implemented. Instead, it is expected that consumers will
 * create a new type inheriting or wrapping those provided by this file.
 *
 * STORAGE SERVICE OVERVIEW
 *
 * The storage service is effectively a key-value store where each value is a
 * well-defined envelope that stores specific metadata along with a payload.
 * These values are called Basic Storage Objects, or BSOs. BSOs are organized
 * into named groups called collections.
 *
 * The service also provides ancillary APIs not related to storage, such as
 * looking up the set of stored collections, current quota usage, etc.
 */

"use strict";

const EXPORTED_SYMBOLS = [
  "BasicStorageObject",
  "StorageServiceClient",
  "StorageServiceRequestError",
];

const {classes: Cc, interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://services-common/async.js");
Cu.import("resource://services-common/log4moz.js");
Cu.import("resource://services-common/preferences.js");
Cu.import("resource://services-common/rest.js");
Cu.import("resource://services-common/utils.js");

const Prefs = new Preferences("services.common.storageservice.");

/**
 * The data type stored in the storage service.
 *
 * A Basic Storage Object (BSO) is the primitive type stored in the storage
 * service. BSO's are simply maps with a well-defined set of keys.
 *
 * BSOs belong to named collections.
 *
 * A single BSO consists of the following fields:
 *
 *   id - An identifying string. This is how a BSO is uniquely identified within
 *     a single collection.
 *   modified - Integer milliseconds since Unix epoch BSO was modified.
 *   payload - String contents of BSO. The format of the string is undefined
 *     (although JSON is typically used).
 *   ttl - The number of seconds to keep this record.
 *   sortindex - Integer indicating relative importance of record within the
 *     collection.
 *
 * The constructor simply creates an empty BSO having the specified ID (which
 * can be null or undefined). It also takes an optional collection. This is
 * purely for convenience.
 *
 * This type is meant to be a dumb container and little more.
 *
 * @param id
 *        (string) ID of BSO. Can be null.
 *        (string) Collection BSO belongs to. Can be null;
 */
function BasicStorageObject(id=null, collection=null) {
  this.data       = {};
  this.id         = id;
  this.collection = collection;
}
BasicStorageObject.prototype = {
  id: null,
  collection: null,
  data: null,

  // At the time this was written, the convention for constructor arguments
  // was not adopted by Harmony. It could break in the future. We have test
  // coverage that will break if SpiderMonkey changes, just in case.
  _validKeys: new Set(["id", "payload", "modified", "sortindex", "ttl"]),

  /**
   * Get the string payload as-is.
   */
  get payload() {
    return this.data.payload;
  },

  /**
   * Set the string payload to a new value.
   */
  set payload(value) {
    this.data.payload = value;
  },

  /**
   * Get the modified time of the BSO in milliseconds since Unix epoch.
   *
   * You can convert this to a native JS Date instance easily:
   *
   *   let date = new Date(bso.modified);
   */
  get modified() {
    return this.data.modified;
  },

  /**
   * Sets the modified time of the BSO in milliseconds since Unix epoch.
   *
   * Please note that if this value is sent to the server it will be ignored.
   * The server will use its time at the time of the operation when storing the
   * BSO.
   */
  set modified(value) {
    this.data.modified = value;
  },

  get sortindex() {
    if (this.data.sortindex) {
      return this.data.sortindex || 0;
    }

    return 0;
  },

  set sortindex(value) {
    if (!value && value !== 0) {
      delete this.data.sortindex;
      return;
    }

    this.data.sortindex = value;
  },

  get ttl() {
    return this.data.ttl;
  },

  set ttl(value) {
    if (!value && value !== 0) {
      delete this.data.ttl;
      return;
    }

    this.data.ttl = value;
  },

  /**
   * Deserialize JSON or another object into this instance.
   *
   * The argument can be a string containing serialized JSON or an object.
   *
   * If the JSON is invalid or if the object contains unknown fields, an
   * exception will be thrown.
   *
   * @param json
   *        (string|object) Value to construct BSO from.
   */
  deserialize: function deserialize(input) {
    let data;

    if (typeof(input) == "string") {
      data = JSON.parse(input);
      if (typeof(data) != "object") {
        throw new Error("Supplied JSON is valid but is not a JS-Object.");
      }
    }
    else if (typeof(input) == "object") {
      data = input;
    } else {
      throw new Error("Argument must be a JSON string or object: " +
                      typeof(input));
    }

    for each (let key in Object.keys(data)) {
      if (key == "id") {
        this.id = data.id;
        continue;
      }

      if (!this._validKeys.has(key)) {
        throw new Error("Invalid key in object: " + key);
      }

      this.data[key] = data[key];
    }
  },

  /**
   * Serialize the current BSO to JSON.
   *
   * @return string
   *         The JSON representation of this BSO.
   */
  toJSON: function toJSON() {
    let obj = {};

    for (let [k, v] in Iterator(this.data)) {
      obj[k] = v;
    }

    if (this.id) {
      obj.id = this.id;
    }

    return obj;
  },

  toString: function toString() {
    return "{ " +
      "id: "       + this.id        + " " +
      "modified: " + this.modified  + " " +
      "ttl: "      + this.ttl       + " " +
      "index: "    + this.sortindex + " " +
      "payload: "  + this.payload   +
      " }";
  },
};

/**
 * Represents an error encountered during a StorageServiceRequest request.
 *
 * Instances of this will be passed to the onComplete callback for any request
 * that did not succeed.
 *
 * This type effectively wraps other error conditions. It is up to the client
 * to determine the appropriate course of action for each error type
 * encountered.
 *
 * The following error "classes" are defined by properties on each instance:
 *
 *   serverModified - True if the request to modify data was conditional and
 *     the server rejected the request because it has newer data than the
 *     client.
 *
 *   notFound - True if the requested URI or resource does not exist.
 *
 *   conflict - True if the server reported that a resource being operated on
 *     was in conflict. If this occurs, the client should typically wait a
 *     little and try the request again.
 *
 *   requestTooLarge - True if the request was too large for the server. If
 *     this happens on batch requests, the client should retry the request with
 *     smaller batches.
 *
 *   network - A network error prevented this request from succeeding. If set,
 *     it will be an Error thrown by the Gecko network stack. If set, it could
 *     mean that the request could not be performed or that an error occurred
 *     when the request was in flight. It is also possible the request
 *     succeeded on the server but the response was lost in transit.
 *
 *   authentication - If defined, an authentication error has occurred. If
 *     defined, it will be an Error instance. If seen, the client should not
 *     retry the request without first correcting the authentication issue.
 *
 *   client - An error occurred which was the client's fault. This typically
 *     means the code in this file is buggy.
 *
 *   server - An error occurred on the server. In the ideal world, this should
 *     never happen. But, it does. If set, this will be an Error which
 *     describes the error as reported by the server.
 */
function StorageServiceRequestError() {
  this.serverModified  = false;
  this.notFound        = false;
  this.conflict        = false;
  this.requestToolarge = false;
  this.network         = null;
  this.authentication  = null;
  this.client          = null;
  this.server          = null;
}

/**
 * Represents a single request to the storage service.
 *
 * Instances of this type are returned by the APIs on StorageServiceClient.
 * They should not be created outside of StorageServiceClient.
 *
 * This type encapsulates common storage API request and response handling.
 * Metadata required to perform the request is stored inside each instance and
 * should be treated as invisible by consumers.
 *
 * A number of "public" properties are exposed to allow clients to further
 * customize behavior. These are documented below.
 *
 * Some APIs in StorageServiceClient define their own types which inherit from
 * this one. Read the API documentation to see which types those are and when
 * they apply.
 *
 * This type wraps RESTRequest rather than extending it. The reason is mainly
 * to avoid the fragile base class problem. We implement considerable extra
 * functionality on top of RESTRequest and don't want this to accidentally
 * trample on RESTRequest's members.
 *
 * If this were a C++ class, it and StorageServiceClient would be friend
 * classes. Each touches "protected" APIs of the other. Thus, each should be
 * considered when making changes to the other.
 *
 * Usage
 * =====
 *
 * When you obtain a request instance, it is waiting to be dispatched. It may
 * have additional settings available for tuning. See the documentation in
 * StorageServiceClient for more.
 *
 * There are essentially two types of requests: "basic" and "streaming."
 * "Basic" requests encapsulate the traditional request-response paradigm:
 * a request is issued and we get a response later once the full response
 * is available. Most of the APIs in StorageServiceClient issue these "basic"
 * requests. Streaming requests typically involve the transport of multiple
 * BasicStorageObject instances. When a new BSO instance is available, a
 * callback is fired.
 *
 * For basic requests, the general flow looks something like:
 *
 *   // Obtain a new request instance.
 *   let request = client.getCollectionInfo();
 *
 *   // Install a handler which provides callbacks for request events. The most
 *   // important is `onComplete`, which is called when the request has
 *   // finished and the response is completely received.
 *   request.handler = {
 *     onComplete: function onComplete(error, request) {
 *       // Do something.
 *     }
 *   };
 *
 *   // Send the request.
 *   request.dispatch();
 *
 * Alternatively, we can install the onComplete handler when calling dispatch:
 *
 *   let request = client.getCollectionInfo();
 *   request.dispatch(function onComplete(error, request) {
 *     // Handle response.
 *   });
 *
 * Please note that installing an `onComplete` handler as the argument to
 * `dispatch()` will overwrite an existing `handler`.
 *
 * In both of the above example, the two `request` variables are identical. The
 * original `StorageServiceRequest` is passed into the callback so callers
 * don't need to rely on closures.
 *
 * Most of the complexity for onComplete handlers is error checking.
 *
 * The first thing you do in your onComplete handler is ensure no error was
 * seen:
 *
 *   function onComplete(error, request) {
 *     if (error) {
 *       // Handle error.
 *     }
 *   }
 *
 * If `error` is defined, it will be an instance of
 * `StorageServiceRequestError`. An error will be set if the request didn't
 * complete successfully. This means the transport layer must have succeeded
 * and the application protocol (HTTP) must have returned a successful status
 * code (2xx and some 3xx). Please see the documentation for
 * `StorageServiceRequestError` for more.
 *
 * A robust error handler would look something like:
 *
 *   function onComplete(error, request) {
 *     if (error) {
 *       if (error.network) {
 *         // Network error encountered!
 *       } else if (error.server) {
 *         // Something went wrong on the server (HTTP 5xx).
 *       } else if (error.authentication) {
 *         // Server rejected request due to bad credentials.
 *       } else if (error.serverModified) {
 *         // The conditional request was rejected because the server has newer
 *         // data than what the client reported.
 *       } else if (error.conflict) {
 *         // The server reported that the operation could not be completed
 *         // because another client is also updating it.
 *       } else if (error.requestTooLarge) {
 *         // The server rejected the request because it was too large.
 *       } else if (error.notFound) {
 *         // The requested resource was not found.
 *       } else if (error.client) {
 *         // Something is wrong with the client's request. You should *never*
 *         // see this, as it means this client is likely buggy. It could also
 *         // mean the server is buggy or misconfigured. Either way, something
 *         // is buggy.
 *       }
 *
 *       return;
 *     }
 *
 *     // Handle successful case.
 *   }
 *
 * If `error` is null, the request completed successfully. There may or may not
 * be additional data available on the request instance.
 *
 * For requests that obtain data, this data is typically made available through
 * the `resultObj` property on the request instance. The API that was called
 * will install its own response hander and ensure this property is decoded to
 * what you expect.
 *
 * Conditional Requests
 * --------------------
 *
 * Many of the APIs on `StorageServiceClient` support conditional requests.
 * That is, the client defines the last version of data it has (the version
 * comes from a previous response from the server) and sends this as part of
 * the request.
 *
 * For query requests, if the server hasn't changed, no new data will be
 * returned. If issuing a conditional query request, the caller should check
 * the `notModified` property on the request in the response callback. If this
 * property is true, the server has no new data and there is obviously no data
 * on the response.
 *
 * For example:
 *
 *   let request = client.getCollectionInfo();
 *   request.locallyModifiedVersion = Date.now() - 60000;
 *   request.dispatch(function onComplete(error, request) {
 *     if (error) {
 *       // Handle error.
 *       return;
 *     }
 *
 *     if (request.notModified) {
 *       return;
 *     }
 *
 *     let info = request.resultObj;
 *     // Do stuff.
 *   });
 *
 * For modification requests, if the server has changed, the request will be
 * rejected. When this happens, `error`will be defined and the `serverModified`
 * property on it will be true.
 *
 * For example:
 *
 *   let request = client.setBSO(bso);
 *   request.locallyModifiedVersion = bso.modified;
 *   request.dispatch(function onComplete(error, request) {
 *     if (error) {
 *       if (error.serverModified) {
 *         // Server data is newer! We should probably fetch it and apply
 *         // locally.
 *       }
 *
 *       return;
 *     }
 *
 *     // Handle success.
 *   });
 *
 * Future Features
 * ---------------
 *
 * The current implementation does not support true streaming for things like
 * multi-BSO retrieval. However, the API supports it, so we should be able
 * to implement it transparently.
 */
function StorageServiceRequest() {
  this._log = Log4Moz.repository.getLogger("Sync.StorageService.Request");
  this._log.level = Log4Moz.Level[Prefs.get("log.level")];

  this.notModified = false;

  this._client                 = null;
  this._request                = null;
  this._method                 = null;
  this._handler                = {};
  this._data                   = null;
  this._error                  = null;
  this._resultObj              = null;
  this._locallyModifiedVersion = null;
  this._allowIfModified        = false;
  this._allowIfUnmodified      = false;
}
StorageServiceRequest.prototype = {
  /**
   * The StorageServiceClient this request came from.
   */
  get client() {
    return this._client;
  },

  /**
   * The underlying RESTRequest instance.
   *
   * This should be treated as read only and should not be modified
   * directly by external callers. While modification would probably work, this
   * would defeat the purpose of the API and the abstractions it is meant to
   * provide.
   *
   * If a consumer needs to modify the underlying request object, it is
   * recommended for them to implement a new type that inherits from
   * StorageServiceClient and override the necessary APIs to modify the request
   * there.
   *
   * This accessor may disappear in future versions.
   */
  get request() {
    return this._request;
  },

  /**
   * The RESTResponse that resulted from the RESTRequest.
   */
  get response() {
    return this._request.response;
  },

  /**
   * HTTP status code from response.
   */
  get statusCode() {
    let response = this.response;
    return response ? response.status : null;
  },

  /**
   * Holds any error that has occurred.
   *
   * If a network error occurred, that will be returned. If no network error
   * occurred, the client error will be returned. If no error occurred (yet),
   * null will be returned.
   */
  get error() {
    return this._error;
  },

  /**
   * The result from the request.
   *
   * This stores the object returned from the server. The type of object depends
   * on the request type. See the per-API documentation in StorageServiceClient
   * for details.
   */
  get resultObj() {
    return this._resultObj;
  },

  /**
   * Define the local version of the entity the client has.
   *
   * This is used to enable conditional requests. Depending on the request
   * type, the value set here could be reflected in the X-If-Modified-Since or
   * X-If-Unmodified-Since headers.
   *
   * This attribute is not honoured on every request. See the documentation
   * in the client API to learn where it is valid.
   */
  set locallyModifiedVersion(value) {
    // Will eventually become a header, so coerce to string.
    this._locallyModifiedVersion = "" + value;
  },

  /**
   * Object which holds callbacks and state for this request.
   *
   * The handler is installed by users of this request. It is simply an object
   * containing 0 or more of the following properties:
   *
   *   onComplete - A function called when the request has completed and all
   *     data has been received from the server. The function receives the
   *     following arguments:
   *
   *       (StorageServiceRequestError) Error encountered during request. null
   *         if no error was encountered.
   *       (StorageServiceRequest) The request that was sent (this instance).
   *         Response information is available via properties and functions.
   *
   *     Unless the call to dispatch() throws before returning, this callback
   *     is guaranteed to be invoked.
   *
   *     Every client almost certainly wants to install this handler.
   *
   *   onDispatch - A function called immediately before the request is
   *     dispatched. This hook can be used to inspect or modify the request
   *     before it is issued.
   *
   *     The called function receives the following arguments:
   *
   *       (StorageServiceRequest) The request being issued (this request).
   *
   *   onBSORecord - When retrieving multiple BSOs from the server, this
   *     function is invoked when a new BSO record has been read. This function
   *     will be invoked 0 to N times before onComplete is invoked. onComplete
   *     signals that the last BSO has been processed or that an error
   *     occurred. The function receives the following arguments:
   *
   *       (StorageServiceRequest) The request that was sent (this instance).
   *       (BasicStorageObject|string) The received BSO instance (when in full
   *         mode) or the string ID of the BSO (when not in full mode).
   *
   * Callers are free to (and encouraged) to store extra state in the supplied
   * handler.
   */
  set handler(value) {
    if (typeof(value) != "object") {
      throw new Error("Invalid handler. Must be an Object.");
    }

    this._handler = value;

    if (!value.onComplete) {
      this._log.warn("Handler does not contain an onComplete callback!");
    }
  },

  get handler() {
    return this._handler;
  },

  //---------------
  // General APIs |
  //---------------

  /**
   * Start the request.
   *
   * The request is dispatched asynchronously. The installed handler will have
   * one or more of its callbacks invoked as the state of the request changes.
   *
   * The `onComplete` argument is optional. If provided, the supplied function
   * will be installed on a *new* handler before the request is dispatched. This
   * is equivalent to calling:
   *
   *   request.handler = {onComplete: value};
   *   request.dispatch();
   *
   * Please note that any existing handler will be replaced if onComplete is
   * provided.
   *
   * @param onComplete
   *        (function) Callback to be invoked when request has completed.
   */
  dispatch: function dispatch(onComplete) {
    if (onComplete) {
      this.handler = {onComplete: onComplete};
    }

    // Installing the dummy callback makes implementation easier in _onComplete
    // because we can then blindly call.
    this._dispatch(function _internalOnComplete(error) {
      this._onComplete(error);
      this.completed = true;
    }.bind(this));
  },

  /**
   * This is a synchronous version of dispatch().
   *
   * THIS IS AN EVIL FUNCTION AND SHOULD NOT BE CALLED. It is provided for
   * legacy reasons to support evil, synchronous clients.
   *
   * Please note that onComplete callbacks are executed from this JS thread.
   * We dispatch the request, spin the event loop until it comes back. Then,
   * we execute callbacks ourselves then return. In other words, there is no
   * potential for spinning between callback execution and this function
   * returning.
   *
   * The `onComplete` argument has the same behavior as for `dispatch()`.
   *
   * @param onComplete
   *        (function) Callback to be invoked when request has completed.
   */
  dispatchSynchronous: function dispatchSynchronous(onComplete) {
    if (onComplete) {
      this.handler = {onComplete: onComplete};
    }

    let cb = Async.makeSyncCallback();
    this._dispatch(cb);
    let error = Async.waitForSyncCallback(cb);

    this._onComplete(error);
    this.completed = true;
  },

  //-------------------------------------------------------------------------
  // HIDDEN APIS. DO NOT CHANGE ANYTHING UNDER HERE FROM OUTSIDE THIS TYPE. |
  //-------------------------------------------------------------------------

  /**
   * Data to include in HTTP request body.
   */
  _data: null,

  /**
   * StorageServiceRequestError encountered during dispatchy.
   */
  _error: null,

  /**
   * Handler to parse response body into another object.
   *
   * This is installed by the client API. It should return the value the body
   * parses to on success. If a failure is encountered, an exception should be
   * thrown.
   */
  _completeParser: null,

  /**
   * Dispatch the request.
   *
   * This contains common functionality for dispatching requests. It should
   * ideally be part of dispatch, but since dispatchSynchronous exists, we
   * factor out common code.
   */
  _dispatch: function _dispatch(onComplete) {
    // RESTRequest throws if the request has already been dispatched, so we
    // need not bother checking.

    // Inject conditional headers into request if they are allowed and if a
    // value is set. Note that _locallyModifiedVersion is always a string and
    // if("0") is true.
    if (this._allowIfModified && this._locallyModifiedVersion) {
      this._log.trace("Making request conditional.");
      this._request.setHeader("X-If-Modified-Since",
                              this._locallyModifiedVersion);
    } else if (this._allowIfUnmodified && this._locallyModifiedVersion) {
      this._log.trace("Making request conditional.");
      this._request.setHeader("X-If-Unmodified-Since",
                              this._locallyModifiedVersion);
    }

    // We have both an internal and public hook.
    // If these throw, it is OK since we are not in a callback.
    if (this._onDispatch) {
      this._onDispatch();
    }

    if (this._handler.onDispatch) {
      this._handler.onDispatch(this);
    }

    this._client.runListeners("onDispatch", this);

    this._log.info("Dispatching request: " + this._method + " " +
                   this._request.uri.asciiSpec);

    this._request.dispatch(this._method, this._data, onComplete);
  },

  /**
   * RESTRequest onComplete handler for all requests.
   *
   * This provides common logic for all response handling.
   */
  _onComplete: function(error) {
    let onCompleteCalled = false;

    let callOnComplete = function callOnComplete() {
      onCompleteCalled = true;

      if (!this._handler.onComplete) {
        this._log.warn("No onComplete installed in handler!");
        return;
      }

      try {
        this._handler.onComplete(this._error, this);
      } catch (ex) {
        this._log.warn("Exception when invoking handler's onComplete: " +
                       CommonUtils.exceptionStr(ex));
        throw ex;
      }
    }.bind(this);

    try {
      if (error) {
        this._error = new StorageServiceRequestError();
        this._error.network = error;
        this._log.info("Network error during request: " + error);
        this._client.runListeners("onNetworkError", this._client, this, error);
        callOnComplete();
        return;
      }

      let response = this._request.response;
      this._log.info(response.status + " " + this._request.uri.asciiSpec);

      this._processHeaders();

      if (response.status == 200) {
        this._resultObj = this._completeParser(response);
        callOnComplete();
        return;
      }

      if (response.status == 201) {
        callOnComplete();
        return;
      }

      if (response.status == 204) {
        callOnComplete();
        return;
      }

      if (response.status == 304) {
        this.notModified = true;
        callOnComplete();
        return;
      }

      // TODO handle numeric response code from server.
      if (response.status == 400) {
        this._error = new StorageServiceRequestError();
        this._error.client = new Error("Client error!");
        callOnComplete();
        return;
      }

      if (response.status == 401) {
        this._error = new StorageServiceRequestError();
        this._error.authentication = new Error("401 Received.");
        this._client.runListeners("onAuthFailure", this._error.authentication,
                                  this);
        callOnComplete();
        return;
      }

      if (response.status == 404) {
        this._error = new StorageServiceRequestError();
        this._error.notFound = true;
        callOnComplete();
        return;
      }

      if (response.status == 409) {
        this._error = new StorageServiceRequestError();
        this._error.conflict = true;
        callOnComplete();
        return;
      }

      if (response.status == 412) {
        this._error = new StorageServiceRequestError();
        this._error.serverModified = true;
        callOnComplete();
        return;
      }

      if (response.status == 413) {
        this._error = new StorageServiceRequestError();
        this._error.requestTooLarge = true;
        callOnComplete();
        return;
      }

      // If we see this, either the client or the server is buggy. We should
      // never see this.
      if (response.status == 415) {
        this._log.error("415 HTTP response seen from server! This should " +
                        "never happen!");
        this._error = new StorageServiceRequestError();
        this._error.client = new Error("415 Unsupported Media Type received!");
        callOnComplete();
        return;
      }

      if (response.status == 503) {
        this._error = new StorageServiceRequestError();
        this._error.server = new Error("503 Received.");
      }

      callOnComplete();

    } catch (ex) {
      this._clientError = ex;
      this._log.info("Exception when processing _onComplete: " + ex);

      if (!onCompleteCalled) {
        this._log.warn("Exception in internal response handling logic!");
        try {
          callOnComplete();
        } catch (ex) {
          this._log.warn("An additional exception was encountered when " +
                         "calling the handler's onComplete: " + ex);
        }
      }
    }
  },

  _processHeaders: function _processHeaders() {
    let headers = this._request.response.headers;

    if (headers["x-timestamp"]) {
      this.serverTime = parseFloat(headers["x-timestamp"]);
    }

    if (headers["x-backoff"]) {
      this.backoffInterval = 1000 * parseInt(headers["x-backoff"], 10);
    }

    if (headers["retry-after"]) {
      this.backoffInterval = 1000 * parseInt(headers["retry-after"], 10);
    }

    if (this.backoffInterval) {
      let failure = this._request.response.status == 503;
      this._client.runListeners("onBackoffReceived", this._client, this,
                               this.backoffInterval, !failure);
    }

    if (headers["x-quota-remaining"]) {
      this.quotaRemaining = parseInt(headers["x-quota-remaining"], 10);
      this._client.runListeners("onQuotaRemaining", this._client, this,
                               this.quotaRemaining);
    }
  },
};

/**
 * Represents a request to fetch from a collection.
 *
 * These requests are highly configurable so they are given their own type.
 * This type inherits from StorageServiceRequest and provides additional
 * controllable parameters.
 *
 * By default, requests are issued in "streaming" mode. As the client receives
 * data from the server, it will invoke the caller-supplied onBSORecord
 * callback for each record as it is ready. When all records have been received,
 * it will invoke onComplete as normal. To change this behavior, modify the
 * "streaming" property before the request is dispatched.
 */
function StorageCollectionGetRequest() {
  StorageServiceRequest.call(this);
}
StorageCollectionGetRequest.prototype = {
  __proto__: StorageServiceRequest.prototype,

  _namedArgs: {},

  _streaming: true,

  /**
   * Control whether streaming mode is in effect.
   *
   * Read the type documentation above for more details.
   */
  set streaming(value) {
    this._streaming = !!value;
  },

  /**
   * Define the set of IDs to fetch from the server.
   */
  set ids(value) {
    this._namedArgs.ids = value.join(",");
  },

  /**
   * Only retrieve BSOs that were modified strictly before this time.
   *
   * Defined in milliseconds since UNIX epoch.
   */
  set older(value) {
    this._namedArgs.older = value;
  },

  /**
   * Only retrieve BSOs that were modified strictly after this time.
   *
   * Defined in milliseconds since UNIX epoch.
   */
  set newer(value) {
    this._namedArgs.newer = value;
  },

  /**
   * If set to a truthy value, return full BSO information.
   *
   * If not set (the default), the request will only return the set of BSO
   * ids.
   */
  set full(value) {
    if (value) {
      this._namedArgs.full = "1";
    } else {
      delete this._namedArgs["full"];
    }
  },

  /**
   * Only retrieve BSOs whose sortindex is higher than this integer value.
   */
  set index_above(value) {
    this._namedArgs.index_above = value;
  },

  /**
   * Only retrieve BSOs whose sortindex is lower than this integer value.
   */
  set index_below(value) {
    this._namedArgs.index_below = value;
  },

  /**
   * Limit the max number of returned BSOs to this integer number.
   */
  set limit(value) {
    this._namedArgs.limit = value;
  },

  /**
   * If set with any value, sort the results based on modification time, oldest
   * first.
   */
  set sortOldest(value) {
    this._namedArgs.sort = "oldest";
  },

  /**
   * If set with any value, sort the results based on modification time, newest
   * first.
   */
  set sortNewest(value) {
    this._namedArgs.sort = "newest";
  },

  /**
   * If set with any value, sort the results based on sortindex value, highest
   * first.
   */
  set sortIndex(value) {
    this._namedArgs.sort = "index";
  },

  _onDispatch: function _onDispatch() {
    let qs = this._getQueryString();
    if (!qs.length) {
      return;
    }

    this._request.uri = CommonUtils.makeURI(this._request.uri.asciiSpec + "?" +
                                            qs);
  },

  _getQueryString: function _getQueryString() {
    let args = [];
    for (let [k, v] in Iterator(this._namedArgs)) {
      args.push(encodeURIComponent(k) + "=" + encodeURIComponent(v));
    }

    return args.join("&");
  },

  _completeParser: function _completeParser(response) {
    let obj = JSON.parse(response.body);
    let items = obj.items;

    if (!Array.isArray(items)) {
      throw new Error("Unexpected JSON response. items is missing or not an " +
                      "array!");
    }

    if (!this.handler.onBSORecord) {
      return;
    }

    for (let bso of items) {
      this.handler.onBSORecord(this, bso);
    }
  },
};

/**
 * Represents a request that sets data in a collection
 *
 * Instances of this type are returned by StorageServiceClient.setBSOs().
 */
function StorageCollectionSetRequest() {
  StorageServiceRequest.call(this);

  this._lines = [];
  this._size  = 0;

  this.successfulIDs = new Set();
  this.failures      = new Map();
}
StorageCollectionSetRequest.prototype = {
  __proto__: StorageServiceRequest.prototype,

  /**
   * Add a BasicStorageObject to this request.
   *
   * Please note that the BSO content is retrieved when the BSO is added to
   * the request. If the BSO changes after it is added to a request, those
   * changes will not be reflected in the request.
   *
   * @param bso
   *        (BasicStorageObject) BSO to add to the request.
   */
  addBSO: function addBSO(bso) {
    if (!bso instanceof BasicStorageObject) {
      throw new Error("argument must be a BasicStorageObject instance.");
    }

    if (!bso.id) {
      throw new Error("Passed BSO must have id defined.");
    }

    let line = JSON.stringify(bso).replace("\n", "\u000a");

    // This is off by 1 in the larger direction. We don't care.
    this._size += line.length + "\n".length;
    this._lines.push(line);
  },

  _onDispatch: function _onDispatch() {
    this._data = this._lines.join("\n");
  },

  _completeParser: function _completeParser(response) {
    let result = JSON.parse(response.body);

    for (let id of result.success) {
      this.successfulIDs.add(id);
    }

    this.allSucceeded = true;

    for (let [id, reasons] in result.failed) {
      this.failures[id] = reasons;
      this.allSucceeded = false;
    }
  },
};

/**
 * Construct a new client for the SyncStorage API, version 2.0.
 *
 * Clients are constructed against a base URI. This URI is typically obtained
 * from the token server via the endpoint component of a successful token
 * response.
 *
 * The purpose of this type is to serve as a middleware between a client's core
 * logic and the HTTP API. It hides the details of how the storage API is
 * implemented but exposes important events, such as when auth goes bad or the
 * server requests the client to back off.
 *
 * All request APIs operate by returning a StorageServiceRequest instance. The
 * caller then installs the appropriate callbacks on each instance and then
 * dispatches the request.
 *
 * Each client instance also serves as a controller and coordinator for
 * associated requests. Callers can install listeners for common events on the
 * client and take the appropriate action whenever any associated request
 * observes them. For example, you will only need to register one listener for
 * backoff observation as opposed to one on each request.
 *
 * While not currently supported, a future goal of this type is to support
 * more advanced transport channels - such as SPDY - to allow for faster and
 * more efficient API calls. The API is thus designed to abstract transport
 * specifics away from the caller.
 *
 * Storage API consumers almost certainly have added functionality on top of the
 * storage service. It is encouraged to create a child type which adds
 * functionality to this layer.
 *
 * @param baseURI
 *        (string) Base URI for all requests.
 */
function StorageServiceClient(baseURI) {
  this._log = Log4Moz.repository.getLogger("Services.Common.StorageServiceClient");
  this._log.level = Log4Moz.Level[Prefs.get("log.level")];

  this._baseURI = baseURI;

  if (this._baseURI[this._baseURI.length-1] != "/") {
    this._baseURI += "/";
  }

  this._log.info("Creating new StorageServiceClient under " + this._baseURI);

  this._listeners = [];
}
StorageServiceClient.prototype = {
  /**
   * The user agent sent with every request.
   *
   * You probably want to change this.
   */
  userAgent: "StorageServiceClient",

  _baseURI: null,
  _log: null,

  _listeners: null,

  //----------------------------
  // Event Listener Management |
  //----------------------------

  /**
   * Adds a listener to this client instance.
   *
   * Listeners allow other parties to react to and influence execution of the
   * client instance.
   *
   * An event listener is simply an object that exposes functions which get
   * executed during client execution. Objects can expose 0 or more of the
   * following keys:
   *
   *   onDispatch - Callback notified immediately before a request is
   *     dispatched. This gets called for every outgoing request. The function
   *     receives as its arguments the client instance and the outgoing
   *     StorageServiceRequest. This listener is useful for global
   *     authentication handlers, which can modify the request before it is
   *     sent.
   *
   *   onAuthFailure - This is called when any request has experienced an
   *     authentication failure.
   *
   *     This callback receives the following arguments:
   *
   *       (StorageServiceClient) Client that encountered the auth failure.
   *       (StorageServiceRequest) Request that encountered the auth failure.
   *
   *   onBackoffReceived - This is called when a backoff request is issued by
   *     the server. Backoffs are issued either when the service is completely
   *     unavailable (and the client should abort all activity) or if the server
   *     is under heavy load (and has completed the current request but is
   *     asking clients to be kind and stop issuing requests for a while).
   *
   *     This callback receives the following arguments:
   *
   *       (StorageServiceClient) Client that encountered the backoff.
   *       (StorageServiceRequest) Request that received the backoff.
   *       (number) Integer milliseconds the server is requesting us to back off
   *         for.
   *       (bool) Whether the request completed successfully. If false, the
   *         client should cease sending additional requests immediately, as
   *         they will likely fail. If true, the client is allowed to continue
   *         to put the server in a proper state. But, it should stop and heed
   *         the backoff as soon as possible.
   *
   *   onNetworkError - This is called for every network error that is
   *     encountered.
   *
   *     This callback receives the following arguments:
   *
   *       (StorageServiceClient) Client that encountered the network error.
   *       (StorageServiceRequest) Request that encountered the error.
   *       (Error) Error passed in to RESTRequest's onComplete handler. It has
   *         a result property, which is a Components.Results enumeration.
   *
   *   onQuotaRemaining - This is called if any request sees updated quota
   *     information from the server. This provides an update mechanism so
   *     listeners can immediately find out quota changes as soon as they
   *     are made.
   *
   *     This callback receives the following arguments:
   *
   *       (StorageServiceClient) Client that encountered the quota change.
   *       (StorageServiceRequest) Request that received the quota change.
   *       (number) Integer number of kilobytes remaining for the user.
   */
  addListener: function addListener(listener) {
    if (!listener) {
      throw new Error("listener argument must be an object.");
    }

    if (this._listeners.indexOf(listener) != -1) {
      return;
    }

    this._listeners.push(listener);
  },

  /**
   * Remove a previously-installed listener.
   */
  removeListener: function removeListener(listener) {
    this._listeners = this._listeners.filter(function(a) {
      return a != listener;
    });
  },

  /**
   * Invoke listeners for a specific event.
   *
   * @param name
   *        (string) The name of the listener to invoke.
   * @param args
   *        (array) Arguments to pass to listener functions.
   */
  runListeners: function runListeners(name, ...args) {
    for (let listener of this._listeners) {
      try {
        if (name in listener) {
          listener[name].apply(listener, args);
        }
      } catch (ex) {
        this._log.warn("Listener threw an exception during " + name + ": "
                       + ex);
      }
    }
  },

  //-----------------------------
  // Information/Metadata APIs  |
  //-----------------------------

  /**
   * Obtain a request that fetches collection info.
   *
   * On successful response, the result is placed in the resultObj property
   * of the request object.
   *
   * The result value is a map of strings to numbers. The string keys represent
   * collection names. The number values are integer milliseconds since Unix
   * epoch that hte collection was last modified.
   *
   * This request can be made conditional by defining `locallyModifiedVersion`
   * on the returned object to the last known version on the client.
   *
   * Example Usage:
   *
   *   let request = client.getCollectionInfo();
   *   request.dispatch(function onComplete(error, request) {
   *     if (!error) {
   *       return;
   *     }
   *
   *     for (let [collection, milliseconds] in Iterator(this.resultObj)) {
   *       // ...
   *     }
   *   });
   */
  getCollectionInfo: function getCollectionInfo() {
    return this._getJSONGETRequest("info/collections");
  },

  /**
   * Fetch quota information.
   *
   * The result in the callback upon success is a map containing quota
   * metadata. It will have the following keys:
   *
   *   usage - Number of bytes currently utilized.
   *   quota - Number of bytes available to account.
   *
   * The request can be made conditional by populating `locallyModifiedVersion`
   * on the returned request instance with the most recently known version of
   * server data.
   */
  getQuota: function getQuota() {
    return this._getJSONGETRequest("info/quota");
  },

  /**
   * Fetch information on how much data each collection uses.
   *
   * The result on success is a map of strings to numbers. The string keys
   * are collection names. The values are numbers corresponding to the number
   * of kilobytes used by that collection.
   */
  getCollectionUsage: function getCollectionUsage() {
    return this._getJSONGETRequest("info/collection_usage");
  },

  /**
   * Fetch the number of records in each collection.
   *
   * The result on success is a map of strings to numbers. The string keys are
   * collection names. The values are numbers corresponding to the integer
   * number of items in that collection.
   */
  getCollectionCounts: function getCollectionCounts() {
    return this._getJSONGETRequest("info/collection_counts");
  },

  //--------------------------
  // Collection Interaction  |
  // -------------------------

  /**
   * Obtain a request to fetch collection information.
   *
   * The returned request instance is a StorageCollectionGetRequest instance.
   * This is a sub-type of StorageServiceRequest and offers a number of setters
   * to control how the request is performed. See the documentation for that
   * type for more.
   *
   * The request can be made conditional by setting `locallyModifiedVersion`
   * on the returned request instance.
   *
   * Example usage:
   *
   *   let request = client.getCollection("testcoll");
   *
   *   // Obtain full BSOs rather than just IDs.
   *   request.full = true;
   *
   *   // Only obtain BSOs modified in the last minute.
   *   request.newer = Date.now() - 60000;
   *
   *   // Install handler.
   *   request.handler = {
   *     onBSORecord: function onBSORecord(request, bso) {
   *       let id = bso.id;
   *       let payload = bso.payload;
   *
   *       // Do something with BSO.
   *     },
   *
   *     onComplete: function onComplete(error, req) {
   *       if (error) {
   *         // Handle error.
   *         return;
   *       }
   *
   *       // Your onBSORecord handler has processed everything. Now is where
   *       // you typically signal that everything has been processed and to move
   *       // on.
   *     }
   *   };
   *
   *   request.dispatch();
   *
   * @param collection
   *        (string) Name of collection to operate on.
   */
  getCollection: function getCollection(collection) {
    if (!collection) {
      throw new Error("collection argument must be defined.");
    }

    let uri = this._baseURI + "storage/" + collection;

    let request = this._getRequest(uri, "GET", {
      accept:          "application/json",
      allowIfModified: true,
      requestType:     StorageCollectionGetRequest
    });

    return request;
  },

  /**
   * Fetch a single Basic Storage Object (BSO).
   *
   * On success, the BSO may be available in the resultObj property of the
   * request as a BasicStorageObject instance.
   *
   * The request can be made conditional by setting `locallyModifiedVersion`
   * on the returned request instance.*
   *
   * Example usage:
   *
   *   let request = client.getBSO("meta", "global");
   *   request.dispatch(function onComplete(error, request) {
   *     if (!error) {
   *       return;
   *     }
   *
   *     if (request.notModified) {
   *       return;
   *     }
   *
   *     let bso = request.bso;
   *     let payload = bso.payload;
   *
   *     ...
   *   };
   *
   * @param collection
   *        (string) Collection to fetch from
   * @param id
   *        (string) ID of BSO to retrieve.
   * @param type
   *        (constructor) Constructor to call to create returned object. This
   *        is optional and defaults to BasicStorageObject.
   */
  getBSO: function fetchBSO(collection, id, type=BasicStorageObject) {
    if (!collection) {
      throw new Error("collection argument must be defined.");
    }

    if (!id) {
      throw new Error("id argument must be defined.");
    }

    let uri = this._baseURI + "storage/" + collection + "/" + id;

    return this._getRequest(uri, "GET", {
      accept: "application/json",
      allowIfModified: true,
      completeParser: function completeParser(response) {
        let record = new type(id, collection);
        record.deserialize(response.body);

        return record;
      },
    });
  },

  /**
   * Add or update a BSO in a collection.
   *
   * To make the request conditional (i.e. don't allow server changes if the
   * server has a newer version), set request.locallyModifiedVersion to the
   * last known version of the BSO. While this could be done automatically by
   * this API, it is intentionally omitted because there are valid conditions
   * where a client may wish to forcefully update the server.
   *
   * If a conditional request fails because the server has newer data, the
   * StorageServiceRequestError passed to the callback will have the
   * `serverModified` property set to true.
   *
   * Example usage:
   *
   *   let bso = new BasicStorageObject("foo", "coll");
   *   bso.payload = "payload";
   *   bso.modified = Date.now();
   *
   *   let request = client.setBSO(bso);
   *   request.locallyModifiedVersion = bso.modified;
   *
   *   request.dispatch(function onComplete(error, req) {
   *     if (error) {
   *       if (error.serverModified) {
   *         // Handle conditional set failure.
   *         return;
   *       }
   *
   *       // Handle other errors.
   *       return;
   *     }
   *
   *     // Record that set worked.
   *   });
   *
   * @param bso
   *        (BasicStorageObject) BSO to upload. The BSO instance must have the
   *        `collection` and `id` properties defined.
   */
  setBSO: function setBSO(bso) {
    if (!bso) {
      throw new Error("bso argument must be defined.");
    }

    if (!bso.collection) {
      throw new Error("BSO instance does not have collection defined.");
    }

    if (!bso.id) {
      throw new Error("BSO instance does not have ID defined.");
    }

    let uri = this._baseURI + "storage/" + bso.collection + "/" + bso.id;
    let request = this._getRequest(uri, "PUT", {
      contentType:       "application/json",
      allowIfUnmodified: true,
      data:              JSON.stringify(bso),
    });

    return request;
  },

  /**
   * Add or update multiple BSOs.
   *
   * This is roughly equivalent to calling setBSO multiple times except it is
   * much more effecient because there is only 1 round trip to the server.
   *
   * The request can be made conditional by setting `locallyModifiedVersion`
   * on the returned request instance.
   *
   * This function returns a StorageCollectionSetRequest instance. This type
   * has additional functions and properties specific to this operation. See
   * its documentation for more.
   *
   * Future improvement: support streaming of uploaded records. Currently, data
   * is buffered in the client before going over the wire. Ideally, we'd support
   * sending over the wire as soon as data is available. This will require
   * support in RESTRequest, which doesn't support streaming on requests, only
   * responses.
   *
   * Example usage:
   *
   *   let request = client.setBSOs("collection0");
   *   let bso0 = new BasicStorageObject("id0");
   *   bso0.payload = "payload0";
   *
   *   let bso1 = new BasicStorageObject("id1");
   *   bso1.payload = "payload1";
   *
   *   request.addBSO(bso0);
   *   request.addBSO(bso1);
   *
   *   request.dispatch(function onComplete(error, req) {
   *     if (error) {
   *       // Handle error.
   *       return;
   *     }
   *
   *     let successful = req.successfulIDs;
   *     let failed = req.failed;
   *
   *     // Do additional processing.
   *   });
   *
   * @param collection
   *        (string) Collection to operate on.
   * @return
   *        (StorageCollectionSetRequest) Request instance.
   */
  setBSOs: function setBSOs(collection) {
    if (!collection) {
      throw new Error("collection argument must be defined.");
    }

    let uri = this._baseURI + "storage/" + collection;
    let request = this._getRequest(uri, "POST", {
      requestType:       StorageCollectionSetRequest,
      contentType:       "application/newlines",
      accept:            "application/json",
      allowIfUnmodified: true,
    });

    return request;
  },

  /**
   * Deletes a single BSO from a collection.
   *
   * The request can be made conditional by setting `locallyModifiedVersion`
   * on the returned request instance.
   *
   * @param collection
   *        (string) Collection to operate on.
   * @param id
   *        (string) ID of BSO to delete.
   */
  deleteBSO: function deleteBSO(collection, id) {
    if (!collection) {
      throw new Error("collection argument must be defined.");
    }

    if (!id) {
      throw new Error("id argument must be defined.");
    }

    let uri = this._baseURI + "storage/" + collection + "/" + id;
    return this._getRequest(uri, "DELETE", {
      allowIfUnmodified: true,
    });
  },

  /**
   * Delete multiple BSOs from a specific collection.
   *
   * This is functional equivalent to calling deleteBSO() for every ID but
   * much more efficient because it only results in 1 round trip to the server.
   *
   * The request can be made conditional by setting `locallyModifiedVersion`
   * on the returned request instance.
   *
   * @param collection
   *        (string) Name of collection to delete BSOs from.
   * @param ids
   *        (iterable of strings) Set of BSO IDs to delete.
   */
  deleteBSOs: function deleteBSOs(collection, ids) {
    // In theory we should URL encode. However, IDs are supposed to be URL
    // safe. If we get garbage in, we'll get garbage out and the server will
    // reject it.
    let s = ids.join(",");

    let uri = this._baseURI + "storage/" + collection + "?ids=" + s;

    return this._getRequest(uri, "DELETE", {
      allowIfUnmodified: true,
    });
  },

  /**
   * Deletes a single collection from the server.
   *
   * The request can be made conditional by setting `locallyModifiedVersion`
   * on the returned request instance.
   *
   * @param collection
   *        (string) Name of collection to delete.
   */
  deleteCollection: function deleteCollection(collection) {
    let uri = this._baseURI + "storage/" + collection;

    return this._getRequest(uri, "DELETE", {
      allowIfUnmodified: true
    });
  },

  /**
   * Deletes all collections data from the server.
   */
  deleteCollections: function deleteCollections() {
    let uri = this._baseURI + "storage";

    return this._getRequest(uri, "DELETE", {});
  },

  /**
   * Helper that wraps _getRequest for GET requests that return JSON.
   */
  _getJSONGETRequest: function _getJSONGETRequest(path) {
    let uri = this._baseURI + path;

    return this._getRequest(uri, "GET", {
      accept:          "application/json",
      allowIfModified: true,
      completeParser:  this._jsonResponseParser,
    });
  },

  /**
   * Common logic for obtaining an HTTP request instance.
   *
   * @param uri
   *        (string) URI to request.
   * @param method
   *        (string) HTTP method to issue.
   * @param options
   *        (object) Additional options to control request and response
   *          handling. Keys influencing behavior are:
   *
   *          completeParser - Function that parses a HTTP response body into a
   *            value. This function receives the RESTResponse object and
   *            returns a value that is added to a StorageResponse instance.
   *            If the response cannot be parsed or is invalid, this function
   *            should throw an exception.
   *
   *          data - Data to be sent in HTTP request body.
   *
   *          accept - Value for Accept request header.
   *
   *          contentType - Value for Content-Type request header.
   *
   *          requestType - Function constructor for request type to initialize.
   *            Defaults to StorageServiceRequest.
   *
   *          allowIfModified - Whether to populate X-If-Modified-Since if the
   *            request contains a locallyModifiedVersion.
   *
   *          allowIfUnmodified - Whether to populate X-If-Unmodified-Since if
   *            the request contains a locallyModifiedVersion.
   */
  _getRequest: function _getRequest(uri, method, options) {
    if (!options.requestType) {
      options.requestType = StorageServiceRequest;
    }

    let request = new RESTRequest(uri);

    if (Prefs.get("sendVersionInfo", true)) {
      let ua = this.userAgent + Prefs.get("client.type", "desktop");
      request.setHeader("user-agent", ua);
    }

    if (options.accept) {
      request.setHeader("accept", options.accept);
    }

    if (options.contentType) {
      request.setHeader("content-type", options.contentType);
    }

    let result = new options.requestType();
    result._request = request;
    result._method = method;
    result._client = this;
    result._data = options.data;

    if (options.completeParser) {
      result._completeParser = options.completeParser;
    }

    result._allowIfModified = !!options.allowIfModified;
    result._allowIfUnmodified = !!options.allowIfUnmodified;

    return result;
  },

  _jsonResponseParser: function _jsonResponseParser(response) {
    let ct = response.headers["content-type"];
    if (!ct) {
      throw new Error("No Content-Type response header! Misbehaving server!");
    }

    if (ct != "application/json" && ct.indexOf("application/json;") != 0) {
      throw new Error("Non-JSON media type: " + ct);
    }

    return JSON.parse(response.body);
  },
};
