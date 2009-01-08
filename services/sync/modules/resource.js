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

Cu.import("resource://weave/Observers.js");
Cu.import("resource://weave/Preferences.js");
Cu.import("resource://weave/log4moz.js");
Cu.import("resource://weave/constants.js");
Cu.import("resource://weave/util.js");
Cu.import("resource://weave/async.js");
Cu.import("resource://weave/auth.js");

Function.prototype.async = Async.sugar;

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

function Resource(uri) {
  this._init(uri);
}
Resource.prototype = {
  _logName: "Net.Resource",

  get authenticator() {
    if (this._authenticator)
      return this._authenticator;
    else
      return Auth.lookupAuthenticator(this.spec);
  },
  set authenticator(value) {
    this._authenticator = value;
  },

  get headers() {
    return this.authenticator.onRequest(this._headers);
  },
  set headers(value) {
    this._headers = value;
  },

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

  get spec() {
    if (this._uri)
      return this._uri.spec;
    return null;
  },

  _data: null,
  get data() this._data,
  set data(value) {
    this._dirty = true;
    this._data = value;
  },

  _lastRequest: null,
  _downloaded: false,
  _dirty: false,
  get lastRequest() this._lastRequest,
  get downloaded() this._downloaded,
  get dirty() this._dirty,

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

  _createRequest: function Res__createRequest() {
    let ios = Cc["@mozilla.org/network/io-service;1"].
      getService(Ci.nsIIOService);
    let channel = ios.newChannel(this.spec, null, null).
      QueryInterface(Ci.nsIHttpChannel);

    let headers = this.headers; // avoid calling the authorizer more than once
    for (let key in headers) {
      if (key == 'Authorization')
        this._log.trace("HTTP Header " + key + ": ***** (suppressed)");
      else
        this._log.trace("HTTP Header " + key + ": " + headers[key]);
      channel.setRequestHeader(key, headers[key], true);
    }
    return channel;
  },

  _onProgress: function Res__onProgress(event) {
    this._lastProgress = Date.now();
  },

  filterUpload: function Res_filterUpload(onComplete) {
    let fn = function() {
      let self = yield;
      for each (let filter in this._filters) {
        this._data = yield filter.beforePUT.async(filter, self.cb, this._data, this);
      }
    };
    fn.async(this, onComplete);
  },

  filterDownload: function Res_filterUpload(onComplete) {
    let fn = function() {
      let self = yield;
      let filters = this._filters.slice(); // reverse() mutates, so we copy
      for each (let filter in filters.reverse()) {
        this._data = yield filter.afterGET.async(filter, self.cb, this._data, this);
      }
    };
    fn.async(this, onComplete);
  },

  _request: function Res__request(action, data) {
    let self = yield;
    let iter = 0;
    let channel = this._createRequest();
    
    if ("undefined" != typeof(data))
      this._data = data;

    if ("PUT" == action || "POST" == action) {
      yield this.filterUpload(self.cb);
      this._log.trace(action + " Body:\n" + this._data);
      
      let upload = channel.QueryInterface(Ci.nsIUploadChannel);
      let iStream = Cc["@mozilla.org/io/string-input-stream;1"].
        createInstance(Ci.nsIStringInputStream);
      iStream.setData(this._data, this._data.length);

      upload.setUploadStream(iStream, 'text/plain', this._data.length);
    }

    let listener = new ChannelListener(self.cb, this._onProgress, this._log);
    channel.requestMethod = action;
    this._data = yield channel.asyncOpen(listener, null);

    if (Utils.checkStatus(channel.responseStatus, null, [[400,499],501,505])) {
      this._log.debug(action + " request failed (" + channel.responseStatus + ")");
      if (this._data)
        this._log.debug("Error response: \n" + this._data);
      throw new RequestException(this, action, channel);
    } else {
      this._log.debug(channel.requestMethod + " request successful (" + 
      channel.responseStatus  + ")");

      switch (action) {
      case "DELETE":
        if (Utils.checkStatus(channel.responseStatus, null, [[200,300],404])){
          this._dirty = false;
          this._data = null;
        }
        break;
      case "GET":
      case "POST":
        this._log.trace(action + " Body:\n" + this._data);
        yield this.filterDownload(self.cb);
        break;
      }
    }
    
    self.done(this._data);
  },

  get: function Res_get(onComplete) {
    this._request.async(this, onComplete, "GET");
  },

  put: function Res_put(onComplete, data) {
    this._request.async(this, onComplete, "PUT", data);
  },

  post: function Res_post(onComplete, data) {
    this._request.async(this, onComplete, "POST", data);
  },

  delete: function Res_delete(onComplete) {
    this._request.async(this, onComplete, "DELETE");
  }
};

function ChannelListener(onComplete, onProgress, logger) {
  this._onComplete = onComplete;
  this._onProgress = onProgress;
  this._log = logger;
}
ChannelListener.prototype = {
  onStartRequest: function Channel_onStartRequest(channel) {
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

/* Parses out single WBOs from a full dump */
function RecordParser(data) {
  this._data = data;
}
RecordParser.prototype = {
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
  
  append: function RecordParser_append(data) {
    this._data += data;
  }
};

function JsonFilter() {
  let level = "Debug";
  try { level = Utils.prefs.getCharPref("log.logger.network.jsonFilter"); }
  catch (e) { /* ignore unset prefs */ }
  this._log = Log4Moz.repository.getLogger("Net.JsonFilter");
  this._log.level = Log4Moz.Level[level];
}
JsonFilter.prototype = {
  get _json() {
    let json = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON);
    this.__defineGetter__("_json", function() json);
    return this._json;
  },

  beforePUT: function JsonFilter_beforePUT(data) {
    let self = yield;
    this._log.trace("Encoding data as JSON");
    Observers.notify(null, "weave:service:sync:status", "stats.encoding-json");
    self.done(this._json.encode(data));
  },

  afterGET: function JsonFilter_afterGET(data) {
    let self = yield;
    this._log.trace("Decoding JSON data");
    Observers.notify(null, "weave:service:sync:status", "stats.decoding-json");
    self.done(this._json.decode(data));
  }
};
