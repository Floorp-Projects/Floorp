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
    return "Could not " + this._action + " resource " + this._resource.path +
      " (" + this._request.status + ")";
  }
};

function Resource(uri, authenticator) {
  this._init(uri, authenticator);
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
  set spec(value) {
    this._dirty = true;
    this._downloaded = false;
    this._uri.spec = value;
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

  _init: function Res__init(uri, authenticator) {
    this._log = Log4Moz.repository.getLogger(this._logName);
    this.uri = uri;
    this._authenticator = authenticator;
    this._headers = {'Content-type': 'text/plain'};
    this._filters = [];
  },

  _createRequest: function Res__createRequest(op, onRequestFinished) {
    let request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
      createInstance(Ci.nsIXMLHttpRequest);
    request.onload = onRequestFinished;
    request.onerror = onRequestFinished;
    request.onprogress = Utils.bind2(this, this._onProgress);
    request.mozBackgroundRequest = true;
    request.open(op, this.spec, true);

    let headers = this.headers; // avoid calling the authorizer more than once
    for (let key in headers) {
      if (key == 'Authorization')
        this._log.trace("HTTP Header " + key + ": ***** (suppressed)");
      else
        this._log.trace("HTTP Header " + key + ": " + headers[key]);
      request.setRequestHeader(key, headers[key]);
    }
    return request;
  },

  _onProgress: function Res__onProgress(event) {
    this._lastProgress = Date.now();
  },

  _setupTimeout: function Res__setupTimeout(request, callback) {
    let _request = request;
    let _callback = callback;
    let onTimer = function() {
      if (Date.now() - this._lastProgress > CONNECTION_TIMEOUT) {
        this._log.warn("Connection timed out");
        _request.abort();
        _callback({target:{status:-1}});
      }
    };
    let listener = new Utils.EventListener(onTimer);
    let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    timer.initWithCallback(listener, CONNECTION_TIMEOUT,
                           timer.TYPE_REPEATING_SLACK);
    this._lastProgress = Date.now();
    return timer;
  },

  _request: function Res__request(action, data) {
    let self = yield;
    let listener, wait_timer;
    let iter = 0;

    this._log.debug(action + " request for " + this.spec);

    if ("PUT" == action) {
      for each (let filter in this._filters) {
        data = yield filter.beforePUT.async(filter, self.cb, data, this);
      }
    }

    while (iter < Preferences.get(PREFS_BRANCH + "network.numRetries")) {
      let cb = self.cb; // to avoid warning because we use it twice
      let request = this._createRequest(action, cb);
      let timeout_timer = this._setupTimeout(request, cb);
      let event = yield request.send(data);
      timeout_timer.cancel();
      this._lastRequest = event.target;

      if (action == "DELETE" &&
          Utils.checkStatus(this._lastRequest.status, null, [[200,300],404])) {
        this._dirty = false;
        this._data = null;
        break;

      } else if (Utils.checkStatus(this._lastRequest.status)) {
        this._log.debug(action + " request successful");
        this._dirty = false;

        if ("GET" == action) {
          this._data = this._lastRequest.responseText;
          let filters = this._filters.slice(); // reverse() mutates, so we copy
          for each (let filter in filters.reverse()) {
            this._data = yield filter.afterGET.async(filter, self.cb, this._data, this);
          }
        }
        break;

      // FIXME: this should really check a variety of permanent errors, rather than just 404s
      } else if (action == "GET" && this._lastRequest.status == 404) {
        this._log.debug(action + " request failed (404)");
        throw new RequestException(this, action, this._lastRequest);

      } else {
        // wait for a bit and try again
        this._log.debug(action + " request failed (" +
                        this._lastRequest.status + "), retrying...");
        listener = new Utils.EventListener(self.cb);
        wait_timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        yield wait_timer.initWithCallback(listener, iter * iter * 1000,
                                          wait_timer.TYPE_ONE_SHOT);
        iter++;
      }
    }

    if (iter >= Preferences.get(PREFS_BRANCH + "network.numRetries")) {
      this._log.debug(action + " request failed (too many errors)");
      throw new RequestException(this, action, this._lastRequest);
    }

    self.done(this._data);
  },

  get: function Res_get(onComplete) {
    this._request.async(this, onComplete, "GET");
  },

  put: function Res_put(onComplete, data) {
    if ("undefined" == typeof(data))
      data = this._data;
    this._request.async(this, onComplete, "PUT", data);
  },

  delete: function Res_delete(onComplete) {
    this._request.async(this, onComplete, "DELETE");
  }
};


function JsonFilter() {
  this._log = Log4Moz.repository.getLogger("Net.JsonFilter");
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
