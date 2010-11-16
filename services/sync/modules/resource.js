/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Bookmarks Sync.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Dan Mills <thunder@mozilla.com>
 *  Anant Narayanan <anant@kix.in>
 *  Philipp von Weitershausen <philipp@weitershausen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const EXPORTED_SYMBOLS = ["Resource", "AsyncResource"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://services-sync/auth.js");
Cu.import("resource://services-sync/constants.js");
Cu.import("resource://services-sync/ext/Observers.js");
Cu.import("resource://services-sync/ext/Preferences.js");
Cu.import("resource://services-sync/ext/Sync.js");
Cu.import("resource://services-sync/log4moz.js");
Cu.import("resource://services-sync/util.js");

/*
 * AsyncResource represents a remote network resource, identified by a URI.
 * Create an instance like so:
 * 
 *   let resource = new AsyncResource("http://foobar.com/path/to/resource");
 * 
 * The 'resource' object has the following methods to issue HTTP requests
 * of the corresponding HTTP methods:
 * 
 *   get(callback)
 *   put(data, callback)
 *   post(data, callback)
 *   delete(callback)
 * 
 * 'callback' is a function with the following signature:
 * 
 *   function callback(error, result) {...}
 * 
 * 'error' will be null on successful requests. Likewise, result will not be
 * passes (=undefined) when an error occurs. Note that this is independent of
 * the status of the HTTP response.
 */
function AsyncResource(uri) {
  this._log = Log4Moz.repository.getLogger(this._logName);
  this._log.level =
    Log4Moz.Level[Utils.prefs.getCharPref("log.logger.network.resources")];
  this.uri = uri;
  this._headers = {};
  this._onComplete = Utils.bind2(this, this._onComplete);
}
AsyncResource.prototype = {
  _logName: "Net.Resource",

  // ** {{{ Resource.authenticator }}} **
  //
  // Getter and setter for the authenticator module
  // responsible for this particular resource. The authenticator
  // module may modify the headers to perform authentication
  // while performing a request for the resource, for example.
  get authenticator() {
    if (this._authenticator)
      return this._authenticator;
    else
      return Auth.lookupAuthenticator(this.spec);
  },
  set authenticator(value) {
    this._authenticator = value;
  },

  // ** {{{ Resource.headers }}} **
  //
  // Headers to be included when making a request for the resource.
  // Note: Header names should be all lower case, there's no explicit
  // check for duplicates due to case!
  get headers() {
    return this.authenticator.onRequest(this._headers);
  },
  set headers(value) {
    this._headers = value;
  },
  setHeader: function Res_setHeader() {
    if (arguments.length % 2)
      throw "setHeader only accepts arguments in multiples of 2";
    for (let i = 0; i < arguments.length; i += 2) {
      this._headers[arguments[i].toLowerCase()] = arguments[i + 1];
    }
  },

  // ** {{{ Resource.uri }}} **
  //
  // URI representing this resource.
  get uri() {
    return this._uri;
  },
  set uri(value) {
    if (typeof value == 'string')
      this._uri = Utils.makeURI(value);
    else
      this._uri = value;
  },

  // ** {{{ Resource.spec }}} **
  //
  // Get the string representation of the URI.
  get spec() {
    if (this._uri)
      return this._uri.spec;
    return null;
  },

  // ** {{{ Resource.data }}} **
  //
  // Get and set the data encapulated in the resource.
  _data: null,
  get data() this._data,
  set data(value) {
    this._data = value;
  },

  // ** {{{ Resource._createRequest }}} **
  //
  // This method returns a new IO Channel for requests to be made
  // through. It is never called directly, only {{{_doRequest}}} uses it
  // to obtain a request channel.
  //
  _createRequest: function Res__createRequest() {
    let channel = Svc.IO.newChannel(this.spec, null, null).
      QueryInterface(Ci.nsIRequest).QueryInterface(Ci.nsIHttpChannel);

    // Always validate the cache:
    channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
    channel.loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;

    // Setup a callback to handle bad HTTPS certificates.
    channel.notificationCallbacks = new BadCertListener();

    // Avoid calling the authorizer more than once
    let headers = this.headers;
    for (let key in headers) {
      if (key == 'Authorization')
        this._log.trace("HTTP Header " + key + ": ***** (suppressed)");
      else
        this._log.trace("HTTP Header " + key + ": " + headers[key]);
      channel.setRequestHeader(key, headers[key], false);
    }
    return channel;
  },

  _onProgress: function Res__onProgress(channel) {},

  _doRequest: function _doRequest(action, data, callback) {
    this._callback = callback;
    let channel = this._channel = this._createRequest();

    if ("undefined" != typeof(data))
      this._data = data;

    // PUT and POST are trreated differently because
    // they have payload data.
    if ("PUT" == action || "POST" == action) {
      // Convert non-string bodies into JSON
      if (this._data.constructor.toString() != String)
        this._data = JSON.stringify(this._data);

      this._log.debug(action + " Length: " + this._data.length);
      this._log.trace(action + " Body: " + this._data);

      let type = ('content-type' in this._headers) ?
        this._headers['content-type'] : 'text/plain';

      let stream = Cc["@mozilla.org/io/string-input-stream;1"].
        createInstance(Ci.nsIStringInputStream);
      stream.setData(this._data, this._data.length);

      channel.QueryInterface(Ci.nsIUploadChannel);
      channel.setUploadStream(stream, type, this._data.length);
    }

    // Setup a channel listener so that the actual network operation
    // is performed asynchronously.
    let listener = new ChannelListener(this._onComplete, this._onProgress,
                                       this._log);
    channel.requestMethod = action;
    channel.asyncOpen(listener, null);
  },

  _onComplete: function _onComplete(error, data) {
    if (error) {
      this._callback(error);
      return;
    }

    this._data = data;
    let channel = this._channel;
    let action = channel.requestMethod;

    // Set some default values in-case there's no response header
    let headers = {};
    let status = 0;
    let success = true;
    try {
      // Read out the response headers if available
      channel.visitResponseHeaders({
        visitHeader: function visitHeader(header, value) {
          headers[header.toLowerCase()] = value;
        }
      });
      status = channel.responseStatus;
      success = channel.requestSucceeded;

      // Log the status of the request
      let mesg = [action, success ? "success" : "fail", status,
                  channel.URI.spec].join(" ");
      if (mesg.length > 200)
        mesg = mesg.substr(0, 200) + "…";
      this._log.debug(mesg);
      // Additionally give the full response body when Trace logging
      if (this._log.level <= Log4Moz.Level.Trace)
        this._log.trace(action + " body: " + data);

      // This is a server-side safety valve to allow slowing down
      // clients without hurting performance.
      if (headers["x-weave-backoff"])
        Observers.notify("weave:service:backoff:interval",
                         parseInt(headers["x-weave-backoff"], 10));

      if (success && headers["x-weave-quota-remaining"])
        Observers.notify("weave:service:quota:remaining",
                         parseInt(headers["x-weave-quota-remaining"], 10));
    }
    // Got a response but no header; must be cached (use default values)
    catch(ex) {
      this._log.debug(action + " cached: " + status);
    }

    let ret = new String(data);
    ret.headers = headers;
    ret.status = status;
    ret.success = success;

    // Make a lazy getter to convert the json response into an object
    Utils.lazy2(ret, "obj", function() JSON.parse(ret));

    // Notify if we get a 401 to maybe try again with a new uri
    if (status == 401) {
      // Create an object to allow observers to decide if we should try again
      let subject = {
        newUri: "",
        resource: this,
        response: ret
      }
      Observers.notify("weave:resource:status:401", subject);

      // Do the same type of request but with the new uri
      if (subject.newUri != "") {
        this.uri = subject.newUri;
        this._doRequest(action, this._data, this._callback);
        return;
      }
    }

    this._callback(null, ret);
  },

  get: function get(callback) {
    this._doRequest("GET", undefined, callback);
  },

  put: function put(data, callback) {
    if (typeof data == "function")
      [data, callback] = [undefined, data];
    this._doRequest("PUT", data, callback);
  },

  post: function post(data, callback) {
    if (typeof data == "function")
      [data, callback] = [undefined, data];
    this._doRequest("POST", data, callback);
  },

  delete: function delete(callback) {
    this._doRequest("DELETE", undefined, callback);
  }
};


/*
 * Represent a remote network resource, identified by a URI, with a
 * synchronous API.
 * 
 * 'Resource' is not recommended for new code. Use the asynchronous API of
 * 'AsyncResource' instead.
 */
function Resource(uri) {
  AsyncResource.call(this, uri);
}
Resource.prototype = {

  __proto__: AsyncResource.prototype,

  // ** {{{ Resource.serverTime }}} **
  //
  // Caches the latest server timestamp (X-Weave-Timestamp header).
  serverTime: null,

  // ** {{{ Resource._request }}} **
  //
  // Perform a particular HTTP request on the resource. This method
  // is never called directly, but is used by the high-level
  // {{{get}}}, {{{put}}}, {{{post}}} and {{delete}} methods.
  _request: function Res__request(action, data) {
    let [doRequest, cb] = Sync.withCb(this._doRequest, this);
    function callback(error, ret) {
      if (error)
        cb.throw(error);
      cb(ret);
    }

    // The channel listener might get a failure code
    try {
      return doRequest(action, data, callback);
    } catch(ex) {
      // Combine the channel stack with this request stack
      let error = Error(ex.message);
      let chanStack = [];
      if (ex.stack)
        chanStack = ex.stack.trim().split(/\n/).slice(1);
      let requestStack = error.stack.split(/\n/).slice(1);

      // Strip out the args for the last 2 frames because they're usually HUGE!
      for (let i = 0; i <= 1; i++)
        requestStack[i] = requestStack[i].replace(/\(".*"\)@/, "(...)@");

      error.stack = chanStack.concat(requestStack).join("\n");
      throw error;
    }
  },

  // ** {{{ Resource.get }}} **
  //
  // Perform an asynchronous HTTP GET for this resource.
  get: function Res_get() {
    return this._request("GET");
  },

  // ** {{{ Resource.put }}} **
  //
  // Perform a HTTP PUT for this resource.
  put: function Res_put(data) {
    return this._request("PUT", data);
  },

  // ** {{{ Resource.post }}} **
  //
  // Perform a HTTP POST for this resource.
  post: function Res_post(data) {
    return this._request("POST", data);
  },

  // ** {{{ Resource.delete }}} **
  //
  // Perform a HTTP DELETE for this resource.
  delete: function Res_delete() {
    return this._request("DELETE");
  }
};

// = ChannelListener =
//
// This object implements the {{{nsIStreamListener}}} interface
// and is called as the network operation proceeds.
function ChannelListener(onComplete, onProgress, logger) {
  this._onComplete = onComplete;
  this._onProgress = onProgress;
  this._log = logger;
  this.delayAbort();
}
ChannelListener.prototype = {
  // Wait 5 minutes before killing a request
  ABORT_TIMEOUT: 300000,

  onStartRequest: function Channel_onStartRequest(channel) {
    channel.QueryInterface(Ci.nsIHttpChannel);

    // Save the latest server timestamp when possible
    try {
      Resource.serverTime = channel.getResponseHeader("X-Weave-Timestamp") - 0;
    }
    catch(ex) {}

    this._log.trace(channel.requestMethod + " " + channel.URI.spec);
    this._data = '';
    this.delayAbort();
  },

  onStopRequest: function Channel_onStopRequest(channel, context, status) {
    // Clear the abort timer now that the channel is done
    this.abortTimer.clear();

    if (this._data == '')
      this._data = null;

    // Throw the failure code name (and stop execution)
    if (!Components.isSuccessCode(status)) {
      this._onComplete(Error(Components.Exception("", status).name));
      return;
    }

    this._onComplete(null, this._data);
  },

  onDataAvailable: function Channel_onDataAvail(req, cb, stream, off, count) {
    let siStream = Cc["@mozilla.org/scriptableinputstream;1"].
      createInstance(Ci.nsIScriptableInputStream);
    siStream.init(stream);

    this._data += siStream.read(count);
    this._onProgress();
    this.delayAbort();
  },

  /**
   * Create or push back the abort timer that kills this request
   */
  delayAbort: function delayAbort() {
    Utils.delay(this.abortRequest, this.ABORT_TIMEOUT, this, "abortTimer");
  },

  abortRequest: function abortRequest() {
    // Ignore any callbacks if we happen to get any now
    this.onStopRequest = function() {};
    this.onDataAvailable = function() {};
    this._onComplete(Error("Aborting due to channel inactivity."));
  }
};

// = BadCertListener =
//
// We use this listener to ignore bad HTTPS
// certificates and continue a request on a network
// channel. Probably not a very smart thing to do,
// but greatly simplifies debugging and is just very
// convenient.
function BadCertListener() {
}
BadCertListener.prototype = {
  getInterface: function(aIID) {
    return this.QueryInterface(aIID);
  },

  QueryInterface: function(aIID) {
    if (aIID.equals(Components.interfaces.nsIBadCertListener2) ||
        aIID.equals(Components.interfaces.nsIInterfaceRequestor) ||
        aIID.equals(Components.interfaces.nsISupports))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  notifyCertProblem: function certProblem(socketInfo, sslStatus, targetHost) {
    // Silently ignore?
    let log = Log4Moz.repository.getLogger("Service.CertListener");
    log.level =
      Log4Moz.Level[Utils.prefs.getCharPref("log.logger.network.resources")];
    log.debug("Invalid HTTPS certificate encountered, ignoring!");

    return true;
  }
};
