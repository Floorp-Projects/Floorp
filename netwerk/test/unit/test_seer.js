var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

var seer = null;
var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
var profile = null;

function extract_origin(uri) {
  var o = uri.scheme + "://" + uri.asciiHost;
  if (uri.port !== -1) {
    o = o + ":" + uri.port;
  }
  return o;
}

var LoadContext = function _loadContext() {
};

LoadContext.prototype = {
  usePrivateBrowsing: false,

  getInterface: function loadContext_getInterface(iid) {
    return this.QueryInterface(iid);
  },

  QueryInterface: function loadContext_QueryInterface(iid) {
    if (iid.equals(Ci.nsINetworkSeerVerifier) ||
        iid.equals(Ci.nsILoadContext)) {
      return this;
    }

    throw Cr.NS_ERROR_NO_INTERFACE;
  }
};

var load_context = new LoadContext();

var Verifier = function _verifier(testing, expected_preconnects, expected_preresolves) {
  this.verifying = testing;
  this.expected_preconnects = expected_preconnects;
  this.expected_preresolves = expected_preresolves;
};

Verifier.prototype = {
  verifying: null,
  expected_preconnects: null,
  expected_preresolves: null,

  getInterface: function verifier_getInterface(iid) {
    return this.QueryInterface(iid);
  },

  QueryInterface: function verifier_QueryInterface(iid) {
    if (iid.equals(Ci.nsINetworkSeerVerifier) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  maybe_run_next_test: function verifier_maybe_run_next_test() {
    if (this.expected_preconnects.length === 0 &&
        this.expected_preresolves.length === 0) {
      do_check_true(true, "Well this is unexpected...");
      run_next_test();
    }
  },

  onPredictPreconnect: function verifier_onPredictPreconnect(uri) {
    var origin = extract_origin(uri);
    var index = this.expected_preconnects.indexOf(origin);
    if (index == -1) {
      do_check_true(false, "Got preconnect for unexpected uri " + origin);
    } else {
      this.expected_preconnects.splice(index, 1);
    }
    this.maybe_run_next_test();
  },

  onPredictDNS: function verifier_onPredictDNS(uri) {
    var origin = extract_origin(uri);
    var index = this.expected_preresolves.indexOf(origin);
    if (index == -1) {
      do_check_true(false, "Got preresolve for unexpected uri " + origin);
    } else {
      this.expected_preresolves.splice(index, 1);
    }
    this.maybe_run_next_test();
  }
};

function reset_seer() {
  seer.reset();
}

function newURI(s) {
  return ios.newURI(s, null, null);
}

function test_link_hover() {
  reset_seer();
  var uri = newURI("http://localhost:4444/foo/bar");
  var referrer = newURI("http://localhost:4444/foo");
  var preconns = ["http://localhost:4444"];

  var verifier = new Verifier("hover", preconns, []);
  seer.predict(uri, referrer, seer.PREDICT_LINK, load_context, verifier);
}

function test_pageload() {
  reset_seer();
  var toplevel = "http://localhost:4444/index.html";
  var subresources = [
    "http://localhost:4444/style.css",
    "http://localhost:4443/jquery.js",
    "http://localhost:4444/image.png"
  ];

  var tluri = newURI(toplevel);
  seer.learn(tluri, null, seer.LEARN_LOAD_TOPLEVEL, load_context);
  var preconns = [];
  for (var i = 0; i < subresources.length; i++) {
    var sruri = newURI(subresources[i]);
    seer.learn(sruri, tluri, seer.LEARN_LOAD_SUBRESOURCE, load_context);
    preconns.push(extract_origin(sruri));
  }

  var verifier = new Verifier("pageload", preconns, []);
  seer.predict(tluri, null, seer.PREDICT_LOAD, load_context, verifier);
}

function test_redirect() {
  reset_seer();
  var initial = "http://localhost:4443/redirect";
  var target = "http://localhost:4444/index.html";
  var subresources = [
    "http://localhost:4444/style.css",
    "http://localhost:4443/jquery.js",
    "http://localhost:4444/image.png"
  ];

  var inituri = newURI(initial);
  var targeturi = newURI(target);
  seer.learn(inituri, null, seer.LEARN_LOAD_TOPLEVEL, load_context);
  seer.learn(targeturi, inituri, seer.LEARN_LOAD_REDIRECT, load_context);
  seer.learn(targeturi, null, seer.LEARN_LOAD_TOPLEVEL, load_context);

  var preconns = [];
  preconns.push(extract_origin(targeturi));
  for (var i = 0; i < subresources.length; i++) {
    var sruri = newURI(subresources[i]);
    seer.learn(sruri, targeturi, seer.LEARN_LOAD_SUBRESOURCE, load_context);
    preconns.push(extract_origin(sruri));
  }

  var verifier = new Verifier("redirect", preconns, []);
  seer.predict(inituri, null, seer.PREDICT_LOAD, load_context, verifier);
}

function test_startup() {
  reset_seer();
  var uris = [
    "http://localhost:4444/startup",
    "http://localhost:4443/startup"
  ];
  var preconns = [];
  for (var i = 0; i < uris.length; i++) {
    var uri = newURI(uris[i]);
    seer.learn(uri, null, seer.LEARN_STARTUP, load_context);
    preconns.push(extract_origin(uri));
  }

  var verifier = new Verifier("startup", preconns, []);
  seer.predict(null, null, seer.PREDICT_STARTUP, load_context, verifier);
}

// A class used to guarantee serialization of SQL queries so we can properly
// update last hit times on subresources to ensure the seer tries to do DNS
// preresolve on them instead of preconnecting
var DnsContinueVerifier = function _dnsContinueVerifier(subresource, tluri, preresolves) {
  this.subresource = subresource;
  this.tluri = tluri;
  this.preresolves = preresolves;
};

DnsContinueVerifier.prototype = {
  subresource: null,
  tluri: null,
  preresolves: null,

  getInterface: function _dnsContinueVerifier_getInterface(iid) {
    return this.QueryInterface(iid);
  },

  QueryInterface: function _dnsContinueVerifier_QueryInterface(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsINetworkSeerVerifier)) {
      return this;
    }

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  onPredictPreconnect: function _dnsContinueVerifier_onPredictPreconnect() {
    // This means that the seer has learned and done our "checkpoint" prediction
    // Now we can get on with the prediction we actually want to test

    // tstamp is 10 days older than now - just over 1 week, which will ensure we
    // hit our cutoff for dns vs. preconnect. This is all in usec, hence the
    // x1000 on the Date object value.
    var tstamp = (new Date().valueOf() * 1000) - (10 * 86400 * 1000000);

    seer.prepareForDnsTest(tstamp, this.subresource);

    var verifier = new Verifier("dns", [], this.preresolves);
    seer.predict(this.tluri, null, seer.PREDICT_LOAD, load_context, verifier);
  },

  onPredictDNS: function _dnsContinueVerifier_onPredictDNS() {
    do_check_true(false, "Shouldn't have gotten a preresolve prediction here!");
  }
};

function test_dns() {
  reset_seer();
  var toplevel = "http://localhost:4444/index.html";
  var subresource = "http://localhost:4443/jquery.js";

  var tluri = newURI(toplevel);
  seer.learn(tluri, null, seer.LEARN_LOAD_TOPLEVEL, load_context);
  var sruri = newURI(subresource);
  seer.learn(sruri, tluri, seer.LEARN_LOAD_SUBRESOURCE, load_context);

  var preresolves = [extract_origin(sruri)];
  var continue_verifier = new DnsContinueVerifier(subresource, tluri, preresolves);
  // Fire off a prediction that will do preconnects so we know when the seer
  // thread has gotten to the point where we can update the database manually
  seer.predict(tluri, null, seer.PREDICT_LOAD, load_context, continue_verifier);
}

function test_origin() {
  reset_seer();
  var toplevel = "http://localhost:4444/index.html";
  var subresources = [
    "http://localhost:4444/style.css",
    "http://localhost:4443/jquery.js",
    "http://localhost:4444/image.png"
  ];

  var tluri = newURI(toplevel);
  seer.learn(tluri, null, seer.LEARN_LOAD_TOPLEVEL, load_context);
  var preconns = [];
  for (var i = 0; i < subresources.length; i++) {
    var sruri = newURI(subresources[i]);
    seer.learn(sruri, tluri, seer.LEARN_LOAD_SUBRESOURCE, load_context);
    var origin = extract_origin(sruri);
    if (preconns.indexOf(origin) === -1) {
      preconns.push(origin);
    }
  }

  var loaduri = newURI("http://localhost:4444/anotherpage.html");
  var verifier = new Verifier("origin", preconns, []);
  seer.predict(loaduri, null, seer.PREDICT_LOAD, load_context, verifier);
}

var tests = [
  test_link_hover,
  test_pageload,
  test_redirect,
  test_startup,
  test_dns,
  test_origin
];

function run_test() {
  tests.forEach(add_test);
  profile = do_get_profile();
  seer = Cc["@mozilla.org/network/seer;1"].getService(Ci.nsINetworkSeer);
  do_register_cleanup(reset_seer);
  run_next_test();
}
