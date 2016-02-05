# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

// A simple class that encapsulates a request. You'll notice the
// style here is different from the rest of the extension; that's
// because this was re-used from really old code we had. At some
// point it might be nice to replace this with something better
// (e.g., something that has explicit onerror handler, ability
// to set headers, and so on).

/**
 * Because we might be in a component, we can't just assume that
 * XMLHttpRequest exists. So we use this tiny factory function to wrap the
 * XPCOM version.
 *
 * @return XMLHttpRequest object
 */
this.PROT_NewXMLHttpRequest = function PROT_NewXMLHttpRequest() {
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
 * Note, that XMLFetcher is only used for SafeBrowsing, therefore
 * we inherit from nsILoadContext, so we can use the callbacks on the
 * channel to separate the safebrowsing cookie based on a reserved
 * appId.
 * @constructor
 */
this.PROT_XMLFetcher = function PROT_XMLFetcher() {
  this.debugZone = "xmlfetcher";
  this._request = PROT_NewXMLHttpRequest();
  // implements nsILoadContext
  this.appId = Ci.nsIScriptSecurityManager.SAFEBROWSING_APP_ID;
  this.isInIsolatedMozBrowserElement = false;
  this.usePrivateBrowsing = false;
  this.isContent = false;
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
    this._request.loadInfo.originAttributes = {
      appId: this.appId,
      inIsolatedMozBrowser: this.isInIsolatedMozBrowserElement
    };
    this._request.open("GET", page, asynchronous);
    this._request.channel.notificationCallbacks = this;

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

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIInterfaceRequestor,
                                         Ci.nsISupports,
                                         Ci.nsILoadContext])
};
