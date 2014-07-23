/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

Cu.import("resource://testing-common/httpd.js");

var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
var nzp = Cc["@mozilla.org/network/networkzonepolicy;1"]
          .createInstance(Ci.nsINetworkZonePolicy);
// HTTP Server for 'network' requests.
var httpServ;
// URI Base for requests.
var uriBase;

// Listener implements nsIStreamListener.
//
// @param expectSuccess  If true, Listener will check for request success.
//                       If false, Listener will check for failure and ensure
//                       no onDataAvailable calls are made.
// @param loadGroupAllows
//                       Indicates if loadGroup should allow or forbid private
//                       loads AFTER the response is received. This may be
//                       changed by the channel based on the type of channel
//                       load.
function Listener(expectSuccess, loadGroupAllows) {
  this._expectSuccess = expectSuccess;
  this._loadGroupAllows = loadGroupAllows;
}

Listener.prototype = {
  _expectSuccess: false,
  _loadGroupAllows: true,
  _buffer: null,

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIStreamListener) ||
        iid.equals(Components.interfaces.nsIRequestObserver) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest: function(request, ctx) {
    do_check_true(request instanceof Ci.nsIHttpChannel);
    if (this._expectSuccess) {
      do_check_true(Components.isSuccessCode(request.status));
      do_check_eq(request.requestSucceeded, true);
      do_check_eq(request.responseStatus, 200);
      request.visitResponseHeaders({ visitHeader: function(aHeader, aValue) {
        do_print(aHeader + ": " + aValue);
      }});
      do_print(request.responseStatus + ": " + request.responseStatusText);
      this._buffer = "";
    } else {
      do_check_false(Components.isSuccessCode(request.status));
    }
  },

  onDataAvailable: function(request, ctx, stream, off, cnt) {
    do_check_true(request instanceof Ci.nsIHttpChannel);
    if (!this._expectSuccess) {
      do_throw("Should not get data; private load forbidden!");
    }
    this._buffer = this._buffer.concat(read_stream(stream, cnt));
  },

  onStopRequest: function(request, ctx, status) {
    do_check_true(request instanceof Ci.nsIHttpChannel);
    // Check loadgroup permission has not changed.
    do_check_eq(request.loadGroup.allowLoadsFromPrivateNetworks,
                this._loadGroupAllows);

    if (this._expectSuccess) {
      do_check_true(Components.isSuccessCode(status));
    } else {
      do_check_false(Components.isSuccessCode(status));
    }
    run_next_test();
  }
};

//
// Test Functions
//

// Ensure that the pref enables and disables private load restrictions.
function test_basic_NetworkZonePolicy_pref() {
  // Create loadgroup and channel for non-doc load.
  // Use /failme because we're not going to load this channel.
  var loadGroup = Cc["@mozilla.org/network/load-group;1"]
                  .createInstance(Ci.nsILoadGroup);
  var chan = ios.newChannel("http://localhost/failme/", null, null)
             .QueryInterface(Ci.nsIHttpChannel);
  chan.loadGroup = loadGroup;

  // Verify that we're starting with the pref enabled. This should have been set
  // during this scripts setup phase.
  var prefs = Cc["@mozilla.org/preferences-service;1"]
              .getService(Ci.nsIPrefBranch);
  var nzpEnabled = prefs.getBoolPref("network.zonepolicy.enabled");
  do_check_true(nzpEnabled);

  // Verify defaults
  do_check_eq(loadGroup.allowLoadsFromPrivateNetworks, true);
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), true);

  // Set permission for doc load; verify permission changed.
  // Use /failme because we're not going to load this channel.
  var docChan = ios.newChannel("http://localhost/failme/", null, null)
             .QueryInterface(Ci.nsIHttpChannel);
  docChan.loadGroup = loadGroup;
  docChan.loadFlags |= Ci.nsIChannel.LOAD_DOCUMENT_URI;

  nzp.setPrivateNetworkPermission(docChan, false);
  // Loadgroup should be set to forbid private loads.
  do_check_eq(loadGroup.allowLoadsFromPrivateNetworks, false);
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), false);
  // Since there is no loadgroup hierarchy, simulating a top-level doc load,
  // the doc channel should have permission for private loads.
  do_check_eq(nzp.checkPrivateNetworkPermission(docChan), true);

  // Disable pref; ensure restrictions lifted.
  prefs.setBoolPref("network.zonepolicy.enabled", false);
  // Loadgroup permission will still be "forbid private loads".
  do_check_eq(loadGroup.allowLoadsFromPrivateNetworks, false);
  // NZP will report that private loads are ok.
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), true);
  do_check_eq(nzp.checkPrivateNetworkPermission(docChan), true);

  // Enable pref again; ensure restrictions are again in play.
  prefs.setBoolPref("network.zonepolicy.enabled", true);
  do_check_eq(loadGroup.allowLoadsFromPrivateNetworks, false);
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), false);
  do_check_eq(nzp.checkPrivateNetworkPermission(docChan), true);

  // Leaving pref on for the remainder of the tests.

  run_next_test();
}

// Ensure that NetworkZonePolicy can manage permissions for a channel's
// loadgroup; no loadgroup ancestors.
function test_basic_NetworkZonePolicy_and_loadGroup() {
  // Create loadgroup and channel for non-doc load.
  var loadGroup = Cc["@mozilla.org/network/load-group;1"]
                  .createInstance(Ci.nsILoadGroup);
  var chan = ios.newChannel("http://localhost/failme/", null, null)
             .QueryInterface(Ci.nsIHttpChannel);
  chan.loadGroup = loadGroup;

  // Verify defaults
  do_check_eq(loadGroup.allowLoadsFromPrivateNetworks, true);
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), true);

  // Set permission for non-doc load; verify no changes.
  nzp.setPrivateNetworkPermission(chan, false);
  do_check_eq(loadGroup.allowLoadsFromPrivateNetworks, true);
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), true);

  // Set permission for doc load; verify permission changed.
  // Use /failme because we're not going to load this channel.
  var docChan = ios.newChannel("http://localhost/failme/", null, null)
             .QueryInterface(Ci.nsIHttpChannel);
  docChan.loadGroup = loadGroup;
  docChan.loadFlags |= Ci.nsIChannel.LOAD_DOCUMENT_URI;

  nzp.setPrivateNetworkPermission(docChan, false);
  do_check_eq(loadGroup.allowLoadsFromPrivateNetworks, false);
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), false);
  // Since there is no loadgroup hierarchy, simulating a top-level doc load,
  // the doc channel should have permission for private loads.
  do_check_eq(nzp.checkPrivateNetworkPermission(docChan), true);

  run_next_test();
}

// Ensure that NetworkZonePolicy can manage permissions for a channel's
// loadgroup and one of its ancestors. Ancestor is specified by calling
// function.
function test_loadGroup_and_ancestor(loadGroup, ancestor, chan, docChan) {
  // Verify permission defaults
  do_check_eq(loadGroup.allowLoadsFromPrivateNetworks, true);
  do_check_eq(ancestor.allowLoadsFromPrivateNetworks, true);
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), true);
  do_check_eq(nzp.checkPrivateNetworkPermission(docChan), true);

  // Set permission for non-doc load; verify no changes.
  nzp.setPrivateNetworkPermission(chan, false);
  do_check_eq(loadGroup.allowLoadsFromPrivateNetworks, true);
  do_check_eq(ancestor.allowLoadsFromPrivateNetworks, true);
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), true);
  do_check_eq(nzp.checkPrivateNetworkPermission(docChan), true);

  // Set permission for doc load; verify permission changed for loadgroup, but
  // not for ancestor loadgroup.
  nzp.setPrivateNetworkPermission(docChan, false);
  do_check_eq(loadGroup.allowLoadsFromPrivateNetworks, false);
  do_check_eq(ancestor.allowLoadsFromPrivateNetworks, true);
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), false);
  do_check_eq(nzp.checkPrivateNetworkPermission(docChan), true);

  // Verify we can set permission allowed again.
  nzp.setPrivateNetworkPermission(docChan, true);
  do_check_eq(loadGroup.allowLoadsFromPrivateNetworks, true);
  do_check_eq(ancestor.allowLoadsFromPrivateNetworks, true);
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), true);
  do_check_eq(nzp.checkPrivateNetworkPermission(docChan), true);

  // Set ancestor to forbid private loads; verify chan permission forbidden.
  ancestor.allowLoadsFromPrivateNetworks = false;
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), false);
  // ... loadgroup's own persmission should still be allow.
  do_check_eq(loadGroup.allowLoadsFromPrivateNetworks, true);
  // ... nzp should not be able to set permission to true.
  nzp.setPrivateNetworkPermission(docChan, true);
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), false);
  // ... and docChan should no longer have permission to load privately.
  do_check_eq(nzp.checkPrivateNetworkPermission(docChan), false);

  // Reset ancestor and verify.
  ancestor.allowLoadsFromPrivateNetworks = true;
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), true);
  nzp.setPrivateNetworkPermission(docChan, false);
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), false);
  do_check_eq(nzp.checkPrivateNetworkPermission(docChan), true);
}

// Ensure that NetworkZonePolicy can manage permissions for a channel's
// loadgroup; loadgroup has a parent loadgroup.
function test_basic_NetworkZonePolicy_loadGroup_and_parent() {
  // Create loadgroup, parent loadgroup and channel for non-doc load.
  var loadGroup = Cc["@mozilla.org/network/load-group;1"]
                  .createInstance(Ci.nsILoadGroup);
  var loadGroupAsChild = loadGroup.QueryInterface(Ci.nsILoadGroupChild)
  var chan = ios.newChannel("http://localhost/failme/", null, null)
             .QueryInterface(Ci.nsIHttpChannel);
  chan.loadGroup = loadGroup;

  var docChan = ios.newChannel("http://localhost/failme/", null, null)
             .QueryInterface(Ci.nsIHttpChannel);
  docChan.loadGroup = loadGroup;
  docChan.loadFlags |= Ci.nsIChannel.LOAD_DOCUMENT_URI;

  var parent = Cc["@mozilla.org/network/load-group;1"]
               .createInstance(Ci.nsILoadGroup);
  loadGroupAsChild.parentLoadGroup = parent;
  do_check_eq(parent, loadGroupAsChild.parentLoadGroup);

  test_loadGroup_and_ancestor(loadGroup, parent, chan, docChan);

  run_next_test();
}

// Ensure that NetworkZonePolicy can manage permissions for a channel's
// loadgroup; loadgroup is member of another loadgroup.
function test_basic_NetworkZonePolicy_loadGroup_and_owner() {
  // Create loadgroup, parent loadgroup and channel for non-doc load.
  var loadGroup = Cc["@mozilla.org/network/load-group;1"]
                  .createInstance(Ci.nsILoadGroup);
  var chan = ios.newChannel("http://localhost/failme/", null, null)
             .QueryInterface(Ci.nsIHttpChannel);
  chan.loadGroup = loadGroup;

  var docChan = ios.newChannel("http://localhost/failme/", null, null)
             .QueryInterface(Ci.nsIHttpChannel);
  docChan.loadGroup = loadGroup;
  docChan.loadFlags |= Ci.nsIChannel.LOAD_DOCUMENT_URI;

  var owner = Cc["@mozilla.org/network/load-group;1"]
              .createInstance(Ci.nsILoadGroup);
  loadGroup.loadGroup = owner;
  do_check_eq(owner, loadGroup.loadGroup);

  test_loadGroup_and_ancestor(loadGroup, owner, chan, docChan);

  run_next_test();
}

// Ensure that NetworkZonePolicy can manage permissions for a channel's
// loadgroup; loadgroup is a docshell loadgroup that has a parent docshell.
function test_basic_NetworkZonePolicy_loadGroup_and_docshell() {
  // Create docshell and docshell parent, and get their loadgroups.
  var docShell = Cc["@mozilla.org/docshell;1"].createInstance(Ci.nsIDocShell);

  var docShellParent = Cc["@mozilla.org/docshell;1"]
                       .createInstance(Ci.nsIDocShell);
  docShellParent.addChild(docShell);

  var loadGroup = docShell.QueryInterface(Ci.nsIDocumentLoader).loadGroup;
  var dsParent = docShellParent.QueryInterface(Ci.nsIDocumentLoader).loadGroup;

  // Create a channel for non-doc load.
  var chan = ios.newChannel("http://localhost/failme/", null, null)
             .QueryInterface(Ci.nsIHttpChannel);
  chan.loadGroup = loadGroup;

  // Create a channel for a document load.
  var docChan = ios.newChannel("http://localhost/failme/", null, null)
             .QueryInterface(Ci.nsIHttpChannel);
  docChan.loadGroup = loadGroup;
  docChan.loadFlags |= Ci.nsIChannel.LOAD_DOCUMENT_URI;

  test_loadGroup_and_ancestor(loadGroup, dsParent, chan, docChan);

  run_next_test();
}

// Ensure that a loadgroup's immediate ancestors dictate its private load
// permissions.
function test_loadGroup_immediate_ancestors(isInitialDocLoad) {
  // Create docshell and docshell parent, and get their loadgroups.
  var docShell = Cc["@mozilla.org/docshell;1"].createInstance(Ci.nsIDocShell);
  var docShellParent = Cc["@mozilla.org/docshell;1"]
                       .createInstance(Ci.nsIDocShell);
  docShellParent.addChild(docShell);

  var loadGroup = docShell.QueryInterface(Ci.nsIDocumentLoader).loadGroup;
  var dsParent = docShellParent.QueryInterface(Ci.nsIDocumentLoader).loadGroup;

  // Add owning loadgroup.
  var owner = Cc["@mozilla.org/network/load-group;1"]
              .createInstance(Ci.nsILoadGroup);
  loadGroup.loadGroup = owner;
  do_check_eq(owner, loadGroup.loadGroup);

  // Add parent loadgroup.
  var loadGroupAsChild = loadGroup.QueryInterface(Ci.nsILoadGroupChild)
  var parent = Cc["@mozilla.org/network/load-group;1"]
               .createInstance(Ci.nsILoadGroup);
  loadGroupAsChild.parentLoadGroup = parent;
  do_check_eq(parent, loadGroupAsChild.parentLoadGroup);

  // Create a channel for non-doc load.
  var chan = ios.newChannel("http://localhost/failme/", null, null)
             .QueryInterface(Ci.nsIHttpChannel);
  chan.loadGroup = loadGroup;

  // Create a channel for a document load.
  var docChan = ios.newChannel("http://localhost/failme/", null, null)
             .QueryInterface(Ci.nsIHttpChannel);
  docChan.loadGroup = loadGroup;
  docChan.loadFlags |= Ci.nsIChannel.LOAD_DOCUMENT_URI;

  // Verify permission defaults
  do_check_eq(loadGroup.allowLoadsFromPrivateNetworks, true);
  do_check_eq(dsParent.allowLoadsFromPrivateNetworks, true);
  do_check_eq(owner.allowLoadsFromPrivateNetworks, true);
  do_check_eq(parent.allowLoadsFromPrivateNetworks, true);
  do_check_eq(nzp.checkPrivateNetworkPermission(chan), true);
  do_check_eq(nzp.checkPrivateNetworkPermission(docChan), true);

  // Set ancestors to forbid.
  for (var i = 0; i < 8; i++) {
    dsParent.allowLoadsFromPrivateNetworks = !!(i & 1);
    owner.allowLoadsFromPrivateNetworks = !!(i & 2);
    parent.allowLoadsFromPrivateNetworks = !!(i & 4);
    // Channel's loadgroup should not have changed.
    do_check_eq(loadGroup.allowLoadsFromPrivateNetworks, true);
    // Permission allowed only when all ancestors allow private loads.
    do_check_eq(nzp.checkPrivateNetworkPermission(chan), (i == 7));
    do_check_eq(nzp.checkPrivateNetworkPermission(docChan), (i == 7));
  }

  run_next_test();
}

// Checks a channel load based on its loadgroup private load permissions.
//
// @param allowPrivateLoads  Indicates if private loads should be allowed on
//                           the channel's loadgroup or not.
// @param expectSuccessfulResponse
//                           Indicates if the nsIStreamListener for the channel
//                           load should expect a response with success or
//                           failure.
// @param urlStr             The url that should be loaded by the channel.
//
function test_single_loadGroup(allowPrivateLoads,
                               expectSuccessfulResponse,
                               urlStr) {
  // Create loadgroup and channel for non-doc load.
  var loadGroup = Cc["@mozilla.org/network/load-group;1"]
                  .createInstance(Ci.nsILoadGroup);
  var chan = ios.newChannel(urlStr, null, null)
             .QueryInterface(Ci.nsIHttpChannel);
  chan.loadGroup = loadGroup;

  do_print("Setting loadgroup permission: " +
           (allowPrivateLoads ? "Allowing" : "Forbidding") +
           " private loads for " + urlStr + ".");
  loadGroup.allowLoadsFromPrivateNetworks = allowPrivateLoads;

  // Try to load channel with IP literal. For non-doc loads like this, load
  // group permissions should not change after the response is received.
  var listener = new Listener(expectSuccessfulResponse, allowPrivateLoads);
  chan.asyncOpen(listener, null);
}

// Same as test_single_loadGroup but for a document load.
function test_single_loadGroup_doc_load(allowPrivateLoads,
                                        loadGroupAllowsAfter,
                                        urlStr) {
  // Create loadgroup and channel for document load.
  var loadGroup = Cc["@mozilla.org/network/load-group;1"]
                  .createInstance(Ci.nsILoadGroup);
  var chan = ios.newChannel(urlStr, null, null)
             .QueryInterface(Ci.nsIHttpChannel);
  chan.loadGroup = loadGroup;
  chan.loadFlags |= Ci.nsIChannel.LOAD_DOCUMENT_URI;

  do_print("Setting loadgroup permission: " +
           (allowPrivateLoads ? "Allowing" : "Forbidding") +
           " private loads for doc load " + urlStr + ".");
  loadGroup.allowLoadsFromPrivateNetworks = allowPrivateLoads;

  // Note: No loadgroup hierarchy so simulated top-level doc load will succeed.
  var listener = new Listener(true, loadGroupAllowsAfter);
  chan.asyncOpen(listener, null);
}

function prime_cache_entry(uri, isPrivate, networkID, onEntryPrimed) {
  do_print("Priming cache with a " + (isPrivate ? "private" : "public") +
           " entry.");

  var fakeResponseHead = "HTTP/1.1 200 OK\r\n" +
                         "Content-Type: text/plain\r\n" +
                         "Server: httpd.js\r\n" +
                         "Date: " + (new Date()).toString() + "\r\n";

  asyncOpenCacheEntry(uri, "disk", Ci.nsICacheStorage.OPEN_TRUNCATE, null,
    new OpenCallback(NEW, "a1m", "a1d", function(entry) {
      do_print("Created " + (isPrivate ? "private" : "public") + " entry");
      entry.setMetaDataElement("request-method", "GET");
      entry.setMetaDataElement("response-head", fakeResponseHead);
      if (isPrivate) {
        entry.setMetaDataElement("loaded-from-private-network", networkID);
      }
      do_print("Added metadata");
      asyncOpenCacheEntry(uri, "disk", Ci.nsICacheStorage.OPEN_NORMALLY, null,
        new OpenCallback(NORMAL, "a1m", "a1d", function(entry) {
          do_print("Verifying " + (isPrivate ? "private" : "public") +
                   " entry created");
          if (isPrivate) {
            do_check_eq(entry.getMetaDataElement("loaded-from-private-network"),
                        networkID);
          }

          do_print((isPrivate ? "Private" : "Public") + " cache entry primed.");
          onEntryPrimed();
        })
      );
    })
  );
}

// Create a private cache entry; if private loads are allowed, the entry will
// be accepted. If not, the entry will be rejected, but since the server is
// localhost the network load should also be rejected.
function test_private_cached_entry_same_network(privateLoadAllowed) {
  var uri = uriBase + "/failme/";
  prime_cache_entry(uri, true, ios.networkLinkID, function() {
    test_single_loadGroup(privateLoadAllowed, privateLoadAllowed, uri);
  });
}

// Create a private cache entry of a doc load; it should always be allowed.
// be accepted. The load should not go to the network.
// LoadGroup permissions should be set to allow after the response is received.
function test_private_cached_entry_same_network_doc_load(privateLoadAllowed) {
  var uri = uriBase + "/failme/";
  prime_cache_entry(uri, true, ios.networkLinkID, function() {
    test_single_loadGroup_doc_load(privateLoadAllowed, true, uri);
  });
}

// UUID to fake a load on a different private network.
var fakeNetworkID = "{86437A10-658B-4637-8D41-9B3693F72762}";

// Ensure that privately cached entries from different networks than the current
// one are not loaded.
function test_private_cached_entry_diff_network(privateLoadAllowed) {
  // We should bypass the cache entry since it was created on a different
  // network. As such, use /passme for private loads allowed, and /failme for
  // private loads forbidden.
  var uri = uriBase + (privateLoadAllowed ? "/passme/" : "/failme/");
  prime_cache_entry(uri, true, fakeNetworkID, function() {
    test_single_loadGroup(privateLoadAllowed, privateLoadAllowed, uri);
  });
}

// Ensure that privately cached entries from different networks than the current
// one are not loaded; doc load.
function test_private_cached_entry_diff_network_doc_load(privateLoadAllowed) {
  // We should bypass the cache entry since it was created on a different
  // network. As such, use /passme for private loads allowed, and /failme for
  // private loads forbidden.
  // LoadGroup permissions should allow private loads after the response.
  var uri = uriBase + "/passme/";
  prime_cache_entry(uri, true, fakeNetworkID, function() {
    test_single_loadGroup_doc_load(privateLoadAllowed, true, uri);
  });
}

// Ensure that publicly cached entries are always loaded.
function test_public_cached_entry(privateCacheEntryAllowed) {
  var uri = uriBase + "/failme/";
  prime_cache_entry(uri, false, null, function() {
    test_single_loadGroup(privateCacheEntryAllowed, true, uri);
  });
}

// Ensure that publicly cached documents are always loaded.
function test_public_cached_entry_doc_load(privateCacheEntryAllowed) {
  // Create a public cache entry for /failme; the entry should be accepted and
  // we should not go to the network.
  // LoadGroup permissions should forbid private loads after response.
  var uri = uriBase + "/failme/";
  prime_cache_entry(uri, false, null, function() {
    test_single_loadGroup_doc_load(privateCacheEntryAllowed, false, uri);
  });
}

function test_public_to_private_navigation() {

}

//
// Initialization
//

// Setup HTTP server to handle for http://localhost/passme/ and
// http://localhost/failme/. Requests received by /failme/ will cause the test
// script to fail. Thus, it is used to verify that specific request do not go
// to the network.
function setup_http_server() {
  httpServ = new HttpServer();

  httpServ.registerPathHandler("/passme/", function (metadata, response) {
    do_print("Received request on http://localhost/passme/");
    var httpbody = "0123456789";
    response.setStatusLine(metadata.httpVersion, 200, "OK");
    response.setHeader("Content-Type", "text/plain", false);
    response.bodyOutputStream.write(httpbody, httpbody.length);
  });

  httpServ.registerPathHandler("/failme/", function (metadata, response) {
    do_throw("http://localhost/failme/ should not not receive requests!");
  });
  httpServ.start(-1);

  do_register_cleanup(function() {
    httpServ.stop(function() {});
  });

  do_print("Started HTTP Server on " +
           httpServ.identity.primaryScheme + "://" +
           httpServ.identity.primaryHost + ":" +
           httpServ.identity.primaryPort);
}

function setup_and_add_tests() {
  // Get profile for disk caching.
  do_get_profile();

  // Run tests with NetworkZonePolicy enabled; set pref back to default after
  // tests complete.
  {
    var prefs = Cc["@mozilla.org/preferences-service;1"]
                .getService(Ci.nsIPrefBranch);
    if (!prefs.getBoolPref("network.zonepolicy.enabled")) {
      prefs.setBoolPref("network.zonepolicy.enabled", true);
      do_register_cleanup(function() {
        prefs.setBoolPref("network.zonepolicy.enabled", false);
      });
    }
  }

  uriBase = httpServ.identity.primaryScheme + "://" +
            httpServ.identity.primaryHost + ":" +
            httpServ.identity.primaryPort;

  var tests = [
    // Basic pref test.
    test_basic_NetworkZonePolicy_pref,

    // Verify NetworkZonePolicy can manage loadgroup permisssions.
    function test_basic_NetworkZonePolicy_and_loadGroup_non_init_doc() {
      test_basic_NetworkZonePolicy_and_loadGroup(false);
    },
    function test_basic_NetworkZonePolicy_and_loadGroup_init_doc() {
      test_basic_NetworkZonePolicy_and_loadGroup(true);
    },
    test_basic_NetworkZonePolicy_loadGroup_and_parent,
    test_basic_NetworkZonePolicy_loadGroup_and_owner,
    test_basic_NetworkZonePolicy_loadGroup_and_docshell,
    function test_loadGroup_immediate_ancestors_non_init_doc() {
      test_loadGroup_immediate_ancestors(false);
    },
    function test_loadGroup_immediate_ancestors_init_doc() {
      test_loadGroup_immediate_ancestors(true);
    },

    // Private (localhost) network requests.
    function test_network_private_allowed() {
      test_single_loadGroup(true, true, uriBase + "/passme/"); },
    function test_network_private_forbidden() {
      test_single_loadGroup(false, false, uriBase + "/failme/"); },

    // Private (localhost) network requests for document loads.
    function test_network_private_allowed_doc_load() {
      test_single_loadGroup_doc_load(true, true, uriBase + "/passme/"); },
    function test_network_private_forbidden_doc_load() {
      // Simulates top-level doc load; should be allowed.
      test_single_loadGroup_doc_load(false, true, uriBase + "/passme/"); },

    // Private cache entries; same network.
    function test_private_cache_same_network_private_allowed() {
      test_private_cached_entry_same_network(true); },
    function test_private_cache_same_network_private_forbidden() {
      test_private_cached_entry_same_network(false); },

    // Private cache entries; same network; doc load.
    function test_private_cache_same_network_private_allowed_doc_load() {
      test_private_cached_entry_same_network_doc_load(true); },
    function test_private_cache_same_network_private_forbidden_doc_load() {
      test_private_cached_entry_same_network_doc_load(false); },

    // Private cache entries, different network.
    function test_private_cache_diff_network_private_allowed() {
      test_private_cached_entry_diff_network(true); },
    function test_private_cache_diff_network_private_forbidden() {
      test_private_cached_entry_diff_network(false); },

    // Private cache entries, different network; doc load.
    function test_private_cache_diff_network_private_allowed_doc_load() {
      test_private_cached_entry_diff_network_doc_load(true); },
    function test_private_cache_diff_network_private_forbidden_doc_load() {
      test_private_cached_entry_diff_network_doc_load(false); },

    // Public cache entries.
    function test_public_cache_private_allowed() {
      test_public_cached_entry(true); },
    function test_public_cache_private_forbidden() {
      test_public_cached_entry(false); },

    // Public cache entries for document loads.
    function test_public_cache_private_allowed_doc_load() {
      test_public_cached_entry_doc_load(true); },
    function test_public_cache_private_forbidden_doc_load() {
      test_public_cached_entry_doc_load(false); }
  ];

  tests.forEach(function(test) {
    add_test(test);
  });

  do_print("Added tests.");
}

function run_test() {
  setup_http_server();

  setup_and_add_tests();

  run_next_test();
}
