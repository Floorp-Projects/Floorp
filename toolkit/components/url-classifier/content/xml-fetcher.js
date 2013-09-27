# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

// A simple class that encapsulates a request. You'll notice the
// style here is different from the rest of the extension; that's
// because this was re-used from really old code we had. At some
// point it might be nice to replace this with something better
// (e.g., something that has explicit onerror handler, ability
// to set headers, and so on).
//
// The only interesting thing here is its ability to strip cookies
// from the request.

/**
 * Because we might be in a component, we can't just assume that
 * XMLHttpRequest exists. So we use this tiny factory function to wrap the
 * XPCOM version.
 *
 * @return XMLHttpRequest object
 */
function PROT_NewXMLHttpRequest() {
  var Cc = Components.classes;
  var Ci = Components.interfaces;
  var request = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                .createInstance(Ci.nsIXMLHttpRequest);
  // Need the following so we get onerror/load/progresschange
  request.QueryInterface(Ci.nsIJSXMLHttpRequest);
  return request;
}

/**
 * A helper class that does HTTP GETs and calls back a function with
 * the content it receives. Asynchronous, so uses a closure for the
 * callback.
 *
 * @param opt_stripCookies Boolean indicating whether we should strip
 *                         cookies from this request
 * 
 * @constructor
 */
function PROT_XMLFetcher(opt_stripCookies) {
  this.debugZone = "xmlfetcher";
  this._request = PROT_NewXMLHttpRequest();
  this._stripCookies = !!opt_stripCookies;
}

PROT_XMLFetcher.prototype = {
  /**
   * Function that will be called back upon fetch completion.
   */
  _callback: null,
  

  /**
   * Fetches some content.
   * 
   * @param page URL to fetch
   * @param callback Function to call back when complete.
   */
  get: function(page, callback) {
    this._request.abort();                // abort() is asynchronous, so
    this._request = PROT_NewXMLHttpRequest();
    this._callback = callback;
    var asynchronous = true;
    this._request.open("GET", page, asynchronous);
    this._request.channel.notificationCallbacks = this;

    if (this._stripCookies)
      new PROT_CookieStripper(this._request.channel);

    // Create a closure
    var self = this;
    this._request.addEventListener("readystatechange", function() {
      self.readyStateChange(self);
    }, false);

    this._request.send(null);
  },

  cancel: function() {
    this._request.abort();
    this._request = null;
  },

  /**
   * Called periodically by the request to indicate some state change. 4
   * means content has been received.
   */
  readyStateChange: function(fetcher) {
    if (fetcher._request.readyState != 4)
      return;

    // If the request fails, on trunk we get status set to
    // NS_ERROR_NOT_AVAILABLE.  On 1.8.1 branch we get an exception
    // forwarded from nsIHttpChannel::GetResponseStatus.  To be consistent
    // between branch and trunk, we send back NS_ERROR_NOT_AVAILABLE for
    // http failures.
    var responseText = null;
    var status = Components.results.NS_ERROR_NOT_AVAILABLE;
    try {
      G_Debug(this, "xml fetch status code: \"" + 
              fetcher._request.status + "\"");
      status = fetcher._request.status;
      responseText = fetcher._request.responseText;
    } catch(e) {
      G_Debug(this, "Caught exception trying to read xmlhttprequest " +
              "status/response.");
      G_Debug(this, e);
    }
    if (fetcher._callback)
      fetcher._callback(responseText, status);
  },

  // nsIInterfaceRequestor
  getInterface: function(iid) {
    return this.QueryInterface(iid);
  },

  QueryInterface: function(iid) {
    if (!iid.equals(Components.interfaces.nsIInterfaceRequestor) &&
        !iid.equals(Components.interfaces.nsISupports))
      throw Components.results.NS_ERROR_NO_INTERFACE;
    return this;
  }
};


/**
 * This class knows how to strip cookies from an HTTP request. It
 * listens for http-on-modify-request, and modifies the request
 * accordingly. We can't do this using xmlhttprequest.setHeader() or
 * nsIChannel.setRequestHeader() before send()ing because the cookie
 * service is called after send().
 * 
 * @param channel nsIChannel in which the request is happening
 * @constructor
 */
function PROT_CookieStripper(channel) {
  this.debugZone = "cookiestripper";
  this.topic_ = "http-on-modify-request";
  this.channel_ = channel;

  var Cc = Components.classes;
  var Ci = Components.interfaces;
  this.observerService_ = Cc["@mozilla.org/observer-service;1"]
                          .getService(Ci.nsIObserverService);
  this.observerService_.addObserver(this, this.topic_, false);

  // If the request doesn't issue, don't hang around forever
  var twentySeconds = 20 * 1000;
  this.alarm_ = new G_Alarm(BindToObject(this.stopObserving, this), 
                            twentySeconds);
}

/**
 * Invoked by the observerservice. See nsIObserve.
 */
PROT_CookieStripper.prototype.observe = function(subject, topic, data) {
  if (topic != this.topic_ || subject != this.channel_)
    return;

  G_Debug(this, "Stripping cookies for channel.");

  this.channel_.QueryInterface(Components.interfaces.nsIHttpChannel);
  this.channel_.setRequestHeader("Cookie", "", false /* replace, not add */);
  this.alarm_.cancel();
  this.stopObserving();
}

/**
 * Remove us from the observerservice
 */
PROT_CookieStripper.prototype.stopObserving = function() {
  G_Debug(this, "Removing observer");
  this.observerService_.removeObserver(this, this.topic_);
  this.channel_ = this.alarm_ = this.observerService_ = null;
}

/**
 * XPCOM cruft
 */
PROT_CookieStripper.prototype.QueryInterface = function(iid) {
  var Ci = Components.interfaces;
  if (iid.equals(Ci.nsISupports) || iid.equals(Ci.nsIObserve))
    return this;
  throw Components.results.NS_ERROR_NO_INTERFACE;
}

