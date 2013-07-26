/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["httpRequest", "percentEncode"];

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

// Strictly follow RFC 3986 when encoding URI components.
// Accepts a unescaped string and returns the URI encoded string for use in
// an HTTP request.
function percentEncode(aString)
  encodeURIComponent(aString).replace(/[!'()]/g, escape).replace(/\*/g, "%2A");

/*
 * aOptions can have a variety of fields:
 *  headers, an array of headers
 *  postData, this can be:
 *    a string: send it as is
 *    an array of parameters: encode as form values
 *    null/undefined: no POST data.
 *  method, GET, POST or PUT (this is set automatically if postData exists).
 *  onLoad, a function handle to call when the load is complete, it takes two
 *          parameters: the responseText and the XHR object.
 *  onError, a function handle to call when an error occcurs, it takes three
 *          parameters: the error, the responseText and the XHR object.
 *  logger, an object that implements the debug and log methods (e.g. log4moz).
 *
 * Headers or post data are given as an array of arrays, for each each inner
 * array the first value is the key and the second is the value, e.g.
 *  [["key1", "value1"], ["key2", "value2"]].
 */
function httpRequest(aUrl, aOptions) {
  let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
              .createInstance(Ci.nsIXMLHttpRequest);
  xhr.mozBackgroundRequest = true; // no error dialogs
  let hasPostData = "postData" in aOptions && aOptions.postData;
  xhr.open("method" in aOptions ? aOptions.method :
           (hasPostData ? "POST" : "GET"), aUrl);
  xhr.channel.loadFlags = Ci.nsIChannel.LOAD_ANONYMOUS | // don't send cookies
                          Ci.nsIChannel.LOAD_BYPASS_CACHE |
                          Ci.nsIChannel.INHIBIT_CACHING;
  xhr.onerror = function(aProgressEvent) {
    if ("onError" in aOptions) {
      // adapted from toolkit/mozapps/extensions/nsBlocklistService.js
      let request = aProgressEvent.target;
      let status;
      try {
        // may throw (local file or timeout)
        status = request.status;
      }
      catch (e) {
        request = request.channel.QueryInterface(Ci.nsIRequest);
        status = request.status;
      }
      // When status is 0 we don't have a valid channel.
      let statusText = status ? request.statusText : "offline";
      aOptions.onError(statusText, null, this);
    }
  };
  xhr.onload = function (aRequest) {
    try {
      let target = aRequest.target;
      if ("logger" in aOptions)
        aOptions.logger.debug("Received response: " + target.responseText);
      if (target.status < 200 || target.status >= 300) {
        let errorText = target.responseText;
        if (!errorText || /<(ht|\?x)ml\b/i.test(errorText))
          errorText = target.statusText;
        throw target.status + " - " + errorText;
      }
      if ("onLoad" in aOptions)
        aOptions.onLoad(target.responseText, this);
    } catch (e) {
      Cu.reportError(e);
      if ("onError" in aOptions)
        aOptions.onError(e, aRequest.target.responseText, this);
    }
  };

  if ("headers" in aOptions) {
    aOptions.headers.forEach(function(header) {
      xhr.setRequestHeader(header[0], header[1]);
    });
  }

  // Handle adding postData as defined above.
  let POSTData = hasPostData ? aOptions.postData : "";
  if (Array.isArray(POSTData)) {
    xhr.setRequestHeader("Content-Type",
                         "application/x-www-form-urlencoded; charset=utf-8");
    POSTData = POSTData.map(function(p) p[0] + "=" + percentEncode(p[1]))
                       .join("&");
  }

  if ("logger" in aOptions) {
    aOptions.logger.log("sending request to " + aUrl + " (POSTData = " +
                        POSTData + ")");
  }
  xhr.send(POSTData);
  return xhr;
}
