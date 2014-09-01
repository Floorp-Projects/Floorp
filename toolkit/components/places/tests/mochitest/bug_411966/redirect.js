/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cu = Components.utils;

var PlacesUtils = Cu.import("resource://gre/modules/PlacesUtils.jsm").PlacesUtils;

// Get Services.
var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
              getService(Ci.nsINavHistoryService);
ok(histsvc != null, "Could not get History Service");
var bhist = histsvc.QueryInterface(Ci.nsIBrowserHistory);
ok(bhist != null, "Could not get Browser History Service");
var ios = Cc["@mozilla.org/network/io-service;1"].
          getService(Components.interfaces.nsIIOService);
ok(ios != null, "Could not get IO Service");
var storage = Cc["@mozilla.org/storage/service;1"].
              getService(Ci.mozIStorageService);
ok(storage != null, "Could not get Storage Service");

// Get database connection.
var mDBConn = histsvc.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;
ok(mDBConn != null, "Could not get Database Connection");

function uri(URIString) {
  return ios.newURI(URIString, null, null);
}

var typedURI = uri("http://localhost:8888/tests/toolkit/components/places/tests/mochitest/bug_411966/TypedPage.htm");
var clickedLinkURI = uri("http://localhost:8888/tests/toolkit/components/places/tests/mochitest/bug_411966/ClickedPage.htm");
var temporaryRedirectURI = uri("http://localhost:8888/tests/toolkit/components/places/tests/mochitest/bug_411966/TempRedirectPage.htm");
var permanentRedirectURI = uri("http://localhost:8888/tests/toolkit/components/places/tests/mochitest/bug_411966/PermRedirectPage.htm");

// Stream Listener
function StreamListener(aChannel, aCallbackFunc) {
  this.mChannel = aChannel;
  this.mCallbackFunc = aCallbackFunc;
}

StreamListener.prototype = {
  mData: "",
  mChannel: null,

  // nsIStreamListener
  onStartRequest: function (aRequest, aContext) {
    this.mData = "";
  },

  onDataAvailable: function (aRequest, aContext, aStream, aSourceOffset, aLength) {
    // We actually don't need received data
    var scriptableInputStream =
      Components.classes["@mozilla.org/scriptableinputstream;1"]
                .createInstance(Components.interfaces.nsIScriptableInputStream);
    scriptableInputStream.init(aStream);

    this.mData += scriptableInputStream.read(aLength);
  },

  onStopRequest: function (aRequest, aContext, aStatus) {
    if (Components.isSuccessCode(aStatus))
      this.mCallbackFunc(this.mData);
    else
      throw("Could not get page.");

    this.mChannel = null;
  },

  // nsIChannelEventSink
  asyncOnChannelRedirect: function (aOldChannel, aNewChannel, aFlags, callback) {
    // If redirecting, store the new channel
    this.mChannel = aNewChannel;
    callback.onRedirectVerifyCallback(Components.results.NS_OK);
  },

  // nsIInterfaceRequestor
  getInterface: function (aIID) {
    try {
      return this.QueryInterface(aIID);
    } catch (e) {
      throw Components.results.NS_NOINTERFACE;
    }
  },

  // nsIProgressEventSink (not implementing will cause annoying exceptions)
  onProgress : function (aRequest, aContext, aProgress, aProgressMax) { },
  onStatus : function (aRequest, aContext, aStatus, aStatusArg) { },

  // nsIHttpEventSink (not implementing will cause annoying exceptions)
  onRedirect : function (aOldChannel, aNewChannel) { },

  // we are faking an XPCOM interface, so we need to implement QI
  QueryInterface : function(aIID) {
    if (aIID.equals(Components.interfaces.nsISupports) ||
        aIID.equals(Components.interfaces.nsIInterfaceRequestor) ||
        aIID.equals(Components.interfaces.nsIChannelEventSink) ||
        aIID.equals(Components.interfaces.nsIProgressEventSink) ||
        aIID.equals(Components.interfaces.nsIHttpEventSink) ||
        aIID.equals(Components.interfaces.nsIStreamListener))
      return this;

    throw Components.results.NS_NOINTERFACE;
  }
};

// Check Callback.
function checkDB(data){
  var referrer = this.mChannel.QueryInterface(Ci.nsIHttpChannel).referrer;

  addVisits(
    {uri: this.mChannel.URI,
      transition: Ci.nsINavHistoryService.TRANSITION_REDIRECT_PERMANENT,
      referrer: referrer},
    function() {
      // Get all pages visited from the original typed one
      var sql = `SELECT url FROM moz_historyvisits
                 JOIN moz_places h ON h.id = place_id
                 WHERE from_visit IN
                    (SELECT v.id FROM moz_historyvisits v
                    JOIN moz_places p ON p.id = v.place_id
                    WHERE p.url = ?1)`;
      var stmt = mDBConn.createStatement(sql);
      stmt.bindByIndex(0, typedURI.spec);

      var empty = true;
      while (stmt.executeStep()) {
        empty = false;
        var visitedURI = stmt.getUTF8String(0);
        // Check that redirect from_visit is not from the original typed one
        ok(visitedURI == clickedLinkURI.spec, "Got wrong referrer for " + visitedURI);
      }
      // Ensure that we got some result
      ok(!empty, "empty table");

      SimpleTest.finish();
    }
  );
}

// Asynchronously adds visits to a page, invoking a callback function when done.
function addVisits(aPlaceInfo, aCallback) {
  var places = [];
  if (aPlaceInfo instanceof Ci.nsIURI) {
    places.push({ uri: aPlaceInfo });
  } else if (Array.isArray(aPlaceInfo)) {
    places = places.concat(aPlaceInfo);
  } else {
    places.push(aPlaceInfo);
  }

  // Create mozIVisitInfo for each entry.
  var now = Date.now();
  for (var i = 0; i < places.length; i++) {
    if (!places[i].title) {
      places[i].title = "test visit for " + places[i].uri.spec;
    }
    places[i].visits = [{
      transitionType: places[i].transition === undefined ? Ci.nsINavHistoryService.TRANSITION_LINK
                                                         : places[i].transition,
      visitDate: places[i].visitDate || (now++) * 1000,
      referrerURI: places[i].referrer
    }];
  }

  PlacesUtils.asyncHistory.updatePlaces(
    places,
    {
      handleError: function AAV_handleError() {
        throw("Unexpected error in adding visit.");
      },
      handleResult: function () {},
      handleCompletion: function UP_handleCompletion() {
        if (aCallback)
          aCallback();
      }
    }
  );
}

