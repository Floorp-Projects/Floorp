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

const EXPORTED_SYMBOLS = ['Resource', 'JsonFilter'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://weave/ext/Observers.js");
Cu.import("resource://weave/ext/Preferences.js");
Cu.import("resource://weave/ext/Sync.js");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/auth.js");

// = RequestException =
//
// This function raises an exception through the call
// stack for a failed network request.
function RequestException(resource, action, request) {
  this._resource = resource;
  this._action = action;
  this._request = request;
  this.location = Components.stack.caller;
}
RequestException.prototype = {
  get resource() { return this._resource; },
  get action() { return this._action; },
  get request() { return this._request; },
  get status() { return this._request.status; },
  toString: function ReqEx_toString() {
    return "Could not " + this._action + " resource " + this._resource.spec +
      " (" + this._request.responseStatus + ")";
  }
};

// = Resource =
//
// Represents a remote network resource, identified by a URI.
function Resource(uri) {
  this._init(uri);
}
Resource.prototype = {
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
  // Getter for access to received headers after the request
  // for the resource has been made, setter for headers to be included
  // while making a request for the resource.
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
      this._headers[arguments[i]] = arguments[i + 1];
    }
  },

  // ** {{{ Resource.uri }}} **
  //
  // URI representing this resource.
  get uri() {
    return this._uri;
  },
  set uri(value) {
    this._dirty = true;
    this._downloaded = false;
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
    this._dirty = true;
    this._data = value;
  },

  _lastChannel: null,
  _downloaded: false,
  _dirty: false,
  get lastChannel() this._lastChannel,
  get downloaded() this._downloaded,
  get dirty() this._dirty,

  // ** {{{ Resource.filters }}} **
  //
  // Filters are used to perform pre and post processing on 
  // requests made for resources. Use these methods to add,
  // remove and clear filters applied to the resource.
  _filters: null,
  pushFilter: function Res_pushFilter(filter) {
    this._filters.push(filter);
  },
  popFilter: function Res_popFilter() {
    return this._filters.pop();
  },
  clearFilters: function Res_clearFilters() {
    this._filters = [];
  },

  _init: function Res__init(uri) {
    this._log = Log4Moz.repository.getLogger(this._logName);
    this._log.level =
      Log4Moz.Level[Utils.prefs.getCharPref("log.logger.network.resources")];
    this.uri = uri;
    this._headers = {'Content-type': 'text/plain'};
    this._filters = [];
  },

  // ** {{{ Resource._createRequest }}} **
  //
  // This method returns a new IO Channel for requests to be made
  // through. It is never called directly, only {{{_request}}} uses it
  // to obtain a request channel.
  //
  _createRequest: function Res__createRequest() {
    this._lastChannel = Svc.IO.newChannel(this.spec, null, null).
      QueryInterface(Ci.nsIRequest);

    // Always validate the cache:
    let loadFlags = this._lastChannel.loadFlags;
    loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
    loadFlags |= Ci.nsIRequest.INHIBIT_CACHING;
    this._lastChannel.loadFlags = loadFlags;
    this._lastChannel = this._lastChannel.QueryInterface(Ci.nsIHttpChannel);
    
    // Setup a callback to handle bad HTTPS certificates.
    this._lastChannel.notificationCallbacks = new badCertListener();
    
    // Avoid calling the authorizer more than once
    let headers = this.headers; 
    for (let key in headers) {
      if (key == 'Authorization')
        this._log.trace("HTTP Header " + key + ": ***** (suppressed)");
      else
        this._log.trace("HTTP Header " + key + ": " + headers[key]);
      this._lastChannel.setRequestHeader(key, headers[key], false);
    }
    return this._lastChannel;
  },

  _onProgress: function Res__onProgress(event) {
    this._lastProgress = Date.now();
  },

  // ** {{{ Resource.filterUpload }}} **
  //
  // Apply pre-request filters. Currently, this is done before
  // any PUT request.
  filterUpload: function Resource_filterUpload() {
    this._data = this._filters.reduce(function(data, filter) {
      return filter.beforePUT(data);
    }, this._data);
  },

  // ** {{{ Resource.filterDownload }}} **
  //
  // Apply post-request filters. Currently, this done after
  // any GET request.
  filterDownload: function Resource_filterDownload() {
    this._data = this._filters.reduceRight(function(data, filter) {
      return filter.afterGET(data);
    }, this._data);
  },

  // ** {{{ Resource._request }}} **
  //
  // Perform a particular HTTP request on the resource. This method
  // is never called directly, but is used by the high-level 
  // {{{get}}}, {{{put}}}, {{{post}}} and {{delete}} methods.
  _request: function Res__request(action, data) {
    let iter = 0;
    let channel = this._createRequest();

    if ("undefined" != typeof(data))
      this._data = data;

    // PUT and POST are trreated differently because 
    // they have payload data.
    if ("PUT" == action || "POST" == action) {
      this.filterUpload();
      this._log.debug(action + " Length: " + this._data.length);
      this._log.trace(action + " Body: " + this._data);

      let type = ('Content-Type' in this._headers)?
        this._headers['Content-Type'] : 'text/plain';

      let stream = Cc["@mozilla.org/io/string-input-stream;1"].
        createInstance(Ci.nsIStringInputStream);
      stream.setData(this._data, this._data.length);

      channel.QueryInterface(Ci.nsIUploadChannel);
      channel.setUploadStream(stream, type, this._data.length);
    }

    // Setup a channel listener so that the actual network operation
    // is performed asynchronously.
    let [chanOpen, chanCb] = Sync.withCb(channel.asyncOpen, channel);
    let listener = new ChannelListener(chanCb, this._onProgress, this._log);
    channel.requestMethod = action;
    this._data = chanOpen(listener, null);

    if (!channel.requestSucceeded) {
      this._log.debug(action + " request failed (" + channel.responseStatus + ")");
      if (this._data)
        this._log.debug("Error response: " + this._data);
      throw new RequestException(this, action, channel);

    } else {
      this._log.debug(action + " request successful (" + channel.responseStatus  + ")");

      switch (action) {
      case "DELETE":
        if (Utils.checkStatus(channel.responseStatus, null, [[200,300],404])){
          this._dirty = false;
          this._data = null;
        }
        break;
      case "GET":
      case "POST":
        this._log.trace(action + " Body: " + this._data);
        this.filterDownload();
        break;
      }
    }

    return this._data;
  },

  // ** {{{ Resource.get }}} **
  //
  // Perform an asynchronous HTTP GET for this resource.
  // onComplete will be called on completion of the request.
  get: function Res_get() {
    return this._request("GET");
  },

  // ** {{{ Resource.get }}} **
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
}
ChannelListener.prototype = {
  onStartRequest: function Channel_onStartRequest(channel) {
    // XXX Bug 482179 Some reason xpconnect makes |channel| only nsIRequest
    channel.QueryInterface(Ci.nsIHttpChannel);
    this._log.debug(channel.requestMethod + " request for " +
      channel.URI.spec);
    this._data = '';
  },

  onStopRequest: function Channel_onStopRequest(channel, ctx, time) {
    if (this._data == '')
      this._data = null;

    this._onComplete(this._data);
  },

  onDataAvailable: function Channel_onDataAvail(req, cb, stream, off, count) {
    this._onProgress();

    let siStream = Cc["@mozilla.org/scriptableinputstream;1"].
      createInstance(Ci.nsIScriptableInputStream);
    siStream.init(stream);

    this._data += siStream.read(count);
  }
};

// = RecordParser =
//
// This object retrives single WBOs from a stream of incoming
// JSON. This should be useful for performance optimizations
// in cases where memory is low (on Fennec, for example).
//
// XXX: Note that this parser is currently not used because we
// are yet to figure out the best way to integrate it with the
// asynchronous nature of {{{ChannelListener}}}. Ed's work in the
// Sync module will make this easier in the future.
function RecordParser(data) {
  this._data = data;
}
RecordParser.prototype = {
  // ** {{{ RecordParser.getNextRecord }}} **
  //
  // Returns a single WBO from the stream of JSON received
  // so far.
  getNextRecord: function RecordParser_getNextRecord() {
    let start;
    let bCount = 0;
    let done = false;

    for (let i = 1; i < this._data.length; i++) {
      if (this._data[i] == '{') {
        if (bCount == 0)
          start = i;
        bCount++;
      } else if (this._data[i] == '}') {
        bCount--;
        if (bCount == 0)
          done = true;
      }

      if (done) {
        let ret = this._data.substring(start, i + 1);
        this._data = this._data.substring(i + 1);
        return ret;
      }
    }

    return false;
  },

  // ** {{{ RecordParser.append }}} **
  //
  // Appends data to the current internal buffer
  // of received data by the parser. The buffer
  // is continously processed as {{{getNextRecord}}}
  // is called, so the caller need not keep a copy
  // of the data passed to this function.
  append: function RecordParser_append(data) {
    this._data += data;
  }
};

// = JsonFilter =
//
// Currently, the only filter used in conjunction with 
// {{{Resource.filters}}}. It simply encodes outgoing records
// as JSON, and decodes incoming JSON into JS objects.
function JsonFilter() {
  let level = "Debug";
  try { level = Utils.prefs.getCharPref("log.logger.network.jsonFilter"); }
  catch (e) { /* ignore unset prefs */ }
  this._log = Log4Moz.repository.getLogger("Net.JsonFilter");
  this._log.level = Log4Moz.Level[level];
}
JsonFilter.prototype = {
  beforePUT: function JsonFilter_beforePUT(data) {
    this._log.trace("Encoding data as JSON");
    return JSON.stringify(data);
  },

  afterGET: function JsonFilter_afterGET(data) {
    this._log.trace("Decoding JSON data");
    return JSON.parse(data);
  }
};

// = badCertListener =
//
// We use this listener to ignore bad HTTPS
// certificates and continue a request on a network
// channel. Probably not a very smart thing to do,
// but greatly simplifies debugging and is just very
// convenient.
function badCertListener() {
}
badCertListener.prototype = {
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
