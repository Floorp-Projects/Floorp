const {HttpServer} = ChromeUtils.import("resource://testing-common/httpd.js");
var {Services} = ChromeUtils.import('resource://gre/modules/Services.jsm');
const ReferrerInfo = Components.Constructor("@mozilla.org/referrer-info;1",
                                            "nsIReferrerInfo",
                                            "init");

var running_single_process = false;

var predictor = null;

function is_child_process() {
  return Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime).processType == Ci.nsIXULRuntime.PROCESS_TYPE_CONTENT;
}

function extract_origin(uri) {
  var o = uri.scheme + "://" + uri.asciiHost;
  if (uri.port !== -1) {
    o = o + ":" + uri.port;
  }
  return o;
}

var origin_attributes = {};

var ValidityChecker = function(verifier, httpStatus) {
  this.verifier = verifier;
  this.httpStatus = httpStatus;
};

ValidityChecker.prototype = {
  verifier: null,
  httpStatus: 0,

  QueryInterface: function listener_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsICacheEntryOpenCallback)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  onCacheEntryCheck: function(entry, appCache)
  {
    return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
  },

  onCacheEntryAvailable: function(entry, isnew, appCache, status)
  {
    // Check if forced valid
    Assert.equal(entry.isForcedValid, this.httpStatus === 200);
    this.verifier.maybe_run_next_test();
  }
}

var Verifier = function _verifier(testing, expected_prefetches, expected_preconnects, expected_preresolves) {
  this.verifying = testing;
  this.expected_prefetches = expected_prefetches;
  this.expected_preconnects = expected_preconnects;
  this.expected_preresolves = expected_preresolves;
};

Verifier.prototype = {
  complete: false,
  verifying: null,
  expected_prefetches: null,
  expected_preconnects: null,
  expected_preresolves: null,

  getInterface: function verifier_getInterface(iid) {
    return this.QueryInterface(iid);
  },

  QueryInterface: function verifier_QueryInterface(iid) {
    if (iid.equals(Ci.nsINetworkPredictorVerifier) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  maybe_run_next_test: function verifier_maybe_run_next_test() {
    if (this.expected_prefetches.length === 0 &&
        this.expected_preconnects.length === 0 &&
        this.expected_preresolves.length === 0 &&
        !this.complete) {
      this.complete = true;
      Assert.ok(true, "Well this is unexpected...");
      // This kicks off the ability to run the next test
      reset_predictor();
    }
  },

  onPredictPrefetch: function verifier_onPredictPrefetch(uri, status) {
    var index = this.expected_prefetches.indexOf(uri.asciiSpec);
    if (index == -1 && !this.complete) {
      Assert.ok(false, "Got prefetch for unexpected uri " + uri.asciiSpec);
    } else {
      this.expected_prefetches.splice(index, 1);
    }

    dump("checking validity of entry for " + uri.spec + "\n");
    var checker = new ValidityChecker(this, status);
    asyncOpenCacheEntry(uri.spec, "disk",
        Ci.nsICacheStorage.OPEN_NORMALLY, Services.loadContextInfo.default,
        checker);
  },

  onPredictPreconnect: function verifier_onPredictPreconnect(uri) {
    var origin = extract_origin(uri);
    var index = this.expected_preconnects.indexOf(origin);
    if (index == -1 && !this.complete) {
      Assert.ok(false, "Got preconnect for unexpected uri " + origin);
    } else {
      this.expected_preconnects.splice(index, 1);
    }
    this.maybe_run_next_test();
  },

  onPredictDNS: function verifier_onPredictDNS(uri) {
    var origin = extract_origin(uri);
    var index = this.expected_preresolves.indexOf(origin);
    if (index == -1 && !this.complete) {
      Assert.ok(false, "Got preresolve for unexpected uri " + origin);
    } else {
      this.expected_preresolves.splice(index, 1);
    }
    this.maybe_run_next_test();
  }
};

function reset_predictor() {
  if (running_single_process || is_child_process()) {
    predictor.reset();
  } else {
    sendCommand("predictor.reset();");
  }
}

function newURI(s) {
  return Services.io.newURI(s);
}

var prepListener = {
  numEntriesToOpen: 0,
  numEntriesOpened: 0,
  continueCallback: null,

  QueryInterface: function (iid) {
    if (iid.equals(Ci.nsICacheEntryOpenCallback)) {
      return this;
    }
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  init: function (entriesToOpen, cb) {
    this.numEntriesOpened = 0;
    this.numEntriesToOpen = entriesToOpen;
    this.continueCallback = cb;
  },

  onCacheEntryCheck: function (entry, appCache) {
    return Ci.nsICacheEntryOpenCallback.ENTRY_WANTED;
  },

  onCacheEntryAvailable: function (entry, isNew, appCache, result) {
    Assert.equal(result, Cr.NS_OK);
    entry.setMetaDataElement("predictor_test", "1");
    entry.metaDataReady();
    this.numEntriesOpened++;
    if (this.numEntriesToOpen == this.numEntriesOpened) {
      this.continueCallback();
    }
  }
};

function open_and_continue(uris, continueCallback) {
  var ds = Services.cache2.diskCacheStorage(Services.loadContextInfo.default, false);

  prepListener.init(uris.length, continueCallback);
  for (var i = 0; i < uris.length; ++i) {
    ds.asyncOpenURI(uris[i], "", Ci.nsICacheStorage.OPEN_NORMALLY,
                    prepListener);
  }
}

function test_link_hover() {
  if (!running_single_process && !is_child_process()) {
    // This one we can just proxy to the child and be done with, no extra setup
    // is necessary.
    sendCommand("test_link_hover();");
    return;
  }

  var uri = newURI("http://localhost:4444/foo/bar");
  var referrer = newURI("http://localhost:4444/foo");
  var preconns = ["http://localhost:4444"];

  var verifier = new Verifier("hover", [], preconns, []);
  predictor.predict(uri, referrer, predictor.PREDICT_LINK, origin_attributes, verifier);
}

const pageload_toplevel = newURI("http://localhost:4444/index.html");

function continue_test_pageload() {
  var subresources = [
    "http://localhost:4444/style.css",
    "http://localhost:4443/jquery.js",
    "http://localhost:4444/image.png"
  ];

  // This is necessary to learn the origin stuff
  predictor.learn(pageload_toplevel, null, predictor.LEARN_LOAD_TOPLEVEL, origin_attributes);
  do_timeout(0, () => { // allow the learn() to run on the main thread
  var preconns = [];

  var sruri = newURI(subresources[0]);
  predictor.learn(sruri, pageload_toplevel, predictor.LEARN_LOAD_SUBRESOURCE, origin_attributes);
  do_timeout(0, () => {
  preconns.push(extract_origin(sruri));

  sruri = newURI(subresources[1]);
  predictor.learn(sruri, pageload_toplevel, predictor.LEARN_LOAD_SUBRESOURCE, origin_attributes);
  do_timeout(0, () => {
  preconns.push(extract_origin(sruri));

  sruri = newURI(subresources[2]);
  predictor.learn(sruri, pageload_toplevel, predictor.LEARN_LOAD_SUBRESOURCE, origin_attributes);
  do_timeout(0, () => {
  preconns.push(extract_origin(sruri));

  var verifier = new Verifier("pageload", [], preconns, []);
  predictor.predict(pageload_toplevel, null, predictor.PREDICT_LOAD, origin_attributes, verifier);
  });
  });
  });
  });
}

function test_pageload() {
  open_and_continue([pageload_toplevel], function () {
    if (running_single_process) {
      continue_test_pageload();
    } else {
      sendCommand("continue_test_pageload();");
    }
  });
}

const redirect_inituri = newURI("http://localhost:4443/redirect");
const redirect_targeturi = newURI("http://localhost:4444/index.html");

function continue_test_redrect() {
  var subresources = [
    "http://localhost:4444/style.css",
    "http://localhost:4443/jquery.js",
    "http://localhost:4444/image.png"
  ];

  predictor.learn(redirect_inituri, null, predictor.LEARN_LOAD_TOPLEVEL, origin_attributes);
  do_timeout(0, () => {
  predictor.learn(redirect_targeturi, null, predictor.LEARN_LOAD_TOPLEVEL, origin_attributes);
  do_timeout(0, () => {
  predictor.learn(redirect_targeturi, redirect_inituri, predictor.LEARN_LOAD_REDIRECT, origin_attributes);
  do_tiemout(0, () => {

  var preconns = [];
  preconns.push(extract_origin(redirect_targeturi));

  var sruri = newURI(subresources[0]);
  predictor.learn(sruri, redirect_targeturi, predictor.LEARN_LOAD_SUBRESOURCE, origin_attributes);
  do_timeout(0, () => {
  preconns.push(extract_origin(sruri));

  sruri = newURI(subresources[1]);
  predictor.learn(sruris[1], redirect_targeturi, predictor.LEARN_LOAD_SUBRESOURCE, origin_attributes);
  do_timeout(0, () => {
  preconns.push(extract_origin(sruri));

  sruri = newURI(subresources[2]);
  predictor.learn(sruris[2], redirect_targeturi, predictor.LEARN_LOAD_SUBRESOURCE, origin_attributes);
  do_timeout(0, () => {
  preconns.push(extract_origin(sruri));

  var verifier = new Verifier("redirect", [], preconns, []);
  predictor.predict(redirect_inituri, null, predictor.PREDICT_LOAD, origin_attributes, verifier);
  });
  });
  });
  });
  });
  });
}

function test_redirect() {
  open_and_continue([redirect_inituri, redirect_targeturi], function () {
    if (running_single_process) {
      continue_test_redirect();
    } else {
      sendCommand("continue_test_redirect();");
    }
  });
}

function test_startup() {
  if (!running_single_process && !is_child_process()) {
    // This one we can just proxy to the child and be done with, no extra setup
    // is necessary.
    sendCommand("test_startup();");
    return;
  }

  var uris = [
    "http://localhost:4444/startup",
    "http://localhost:4443/startup"
  ];
  var preconns = [];
  var uri = newURI(uris[0]);
  predictor.learn(uri, null, predictor.LEARN_STARTUP, origin_attributes);
  do_timeout(0, () => {
  preconns.push(extract_origin(uri));

  uri = newURI(uris[1]);
  predictor.learn(uri, null, predictor.LEARN_STARTUP, origin_attributes);
  do_timeout(0, () => {
  preconns.push(extract_origin(uri));

  var verifier = new Verifier("startup", [], preconns, []);
  predictor.predict(null, null, predictor.PREDICT_STARTUP, origin_attributes, verifier);
  });
  });
}

const dns_toplevel = newURI("http://localhost:4444/index.html");

function continue_test_dns() {
  var subresource = "http://localhost:4443/jquery.js";

  predictor.learn(dns_toplevel, null, predictor.LEARN_LOAD_TOPLEVEL, origin_attributes);
  do_timeout(0, () => {
  var sruri = newURI(subresource);
  predictor.learn(sruri, dns_toplevel, predictor.LEARN_LOAD_SUBRESOURCE, origin_attributes);
  do_timeout(0, () => {

  var preresolves = [extract_origin(sruri)];
  var verifier = new Verifier("dns", [], [], preresolves);
  predictor.predict(dns_toplevel, null, predictor.PREDICT_LOAD, origin_attributes, verifier);
  });
  });
}

function test_dns() {
  open_and_continue([dns_toplevel], function () {
    // Ensure that this will do preresolves
    Services.prefs.setIntPref("network.predictor.preconnect-min-confidence", 101);
    if (running_single_process) {
      continue_test_dns();
    } else {
      sendCommand("continue_test_dns();");
    }
  });
}

const origin_toplevel = newURI("http://localhost:4444/index.html");

function continue_test_origin() {
  var subresources = [
    "http://localhost:4444/style.css",
    "http://localhost:4443/jquery.js",
    "http://localhost:4444/image.png"
  ];
  predictor.learn(origin_toplevel, null, predictor.LEARN_LOAD_TOPLEVEL, origin_attributes);
  do_timeout(0, () => {
  var preconns = [];

  var sruri = newURI(subresources[0]);
  predictor.learn(sruri, origin_toplevel, predictor.LEARN_LOAD_SUBRESOURCE, origin_attributes);
  do_timeout(0, () => {
  var origin = extract_origin(sruri);
  if (!preconns.includes(origin)) {
    preconns.push(origin);
  }

  sruri = newURI(subresources[1]);
  predictor.learn(sruri, origin_toplevel, predictor.LEARN_LOAD_SUBRESOURCE, origin_attributes);
  do_timeout(0, () => {
  var origin = extract_origin(sruri);
  if (!preconns.includes(origin)) {
    preconns.push(origin);
  }

  sruri = newURI(subresources[2]);
  predictor.learn(sruri, origin_toplevel, predictor.LEARN_LOAD_SUBRESOURCE, origin_attributes);
  do_timeout(0, () => {
  var origin = extract_origin(sruri);
  if (!preconns.includes(origin)) {
    preconns.push(origin);
  }

  var loaduri = newURI("http://localhost:4444/anotherpage.html");
  var verifier = new Verifier("origin", [], preconns, []);
  predictor.predict(loaduri, null, predictor.PREDICT_LOAD, origin_attributes, verifier);
  });
  });
  });
  });
}

function test_origin() {
  open_and_continue([origin_toplevel], function () {
    if (running_single_process) {
      continue_test_origin();
    } else {
      sendCommand("continue_test_origin();");
    }
  });
}

var httpserv = null;
var prefetch_tluri;
var prefetch_sruri;

function prefetchHandler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  var body = "Success (meow meow meow).";

  response.bodyOutputStream.write(body, body.length);
}

var prefetchListener = {
  onStartRequest: function(request) {
    Assert.equal(request.status, Cr.NS_OK);
  },

  onDataAvailable: function(request, stream, offset, cnt) {
    read_stream(stream, cnt);
  },

  onStopRequest: function(request, status) {
    run_next_test();
  }
};

function test_prefetch_setup() {
  // Disable preconnects and preresolves
  Services.prefs.setIntPref("network.predictor.preconnect-min-confidence", 101);
  Services.prefs.setIntPref("network.predictor.preresolve-min-confidence", 101);

  Services.prefs.setBoolPref("network.predictor.enable-prefetch", true);

  // Makes it so we only have to call test_prefetch_prime twice to make prefetch
  // do its thing.
  Services.prefs.setIntPref("network.predictor.prefetch-rolling-load-count", 2);

  // This test does not run in e10s-mode, so we'll just go ahead and skip it.
  // We've left the e10s test code in below, just in case someone wants to try
  // to make it work at some point in the future.
  if (!running_single_process) {
    dump("skipping test_prefetch_setup due to e10s\n");
    run_next_test();
    return;
  }

  httpserv = new HttpServer();
  httpserv.registerPathHandler("/cat.jpg", prefetchHandler);
  httpserv.start(-1);

  var tluri = "http://127.0.0.1:" + httpserv.identity.primaryPort + "/index.html";
  var sruri = "http://127.0.0.1:" + httpserv.identity.primaryPort + "/cat.jpg";
  prefetch_tluri = newURI(tluri);
  prefetch_sruri = newURI(sruri);
  if (!running_single_process && !is_child_process()) {
    // Give the child process access to these values
    sendCommand("prefetch_tluri = newURI(\"" + tluri + "\");");
    sendCommand("prefetch_sruri = newURI(\"" + sruri + "\");");
  }

  run_next_test();
}

// Used to "prime the pump" for prefetch - it makes sure all our learns go
// through as expected so that prefetching will happen.
function test_prefetch_prime() {
  // This test does not run in e10s-mode, so we'll just go ahead and skip it.
  // We've left the e10s test code in below, just in case someone wants to try
  // to make it work at some point in the future.
  if (!running_single_process) {
    dump("skipping test_prefetch_prime due to e10s\n");
    run_next_test();
    return;
  }

  open_and_continue([prefetch_tluri], function() {
    if (running_single_process) {
      predictor.learn(prefetch_tluri, null, predictor.LEARN_LOAD_TOPLEVEL, origin_attributes);
      predictor.learn(prefetch_sruri, prefetch_tluri, predictor.LEARN_LOAD_SUBRESOURCE, origin_attributes);
    } else {
      sendCommand("predictor.learn(prefetch_tluri, null, predictor.LEARN_LOAD_TOPLEVEL, origin_attributes);");
      sendCommand("predictor.learn(prefetch_sruri, prefetch_tluri, predictor.LEARN_LOAD_SUBRESOURCE, origin_attributes);");
    }

    // This runs in the parent or only process
    var channel = NetUtil.newChannel({
      uri: prefetch_sruri.asciiSpec,
      loadUsingSystemPrincipal: true,
    }).QueryInterface(Ci.nsIHttpChannel);
    channel.requestMethod = "GET";
    channel.referrerInfo = new ReferrerInfo(Ci.nsIHttpChannel.REFERRER_POLICY_UNSET, true, prefetch_tluri);
    channel.asyncOpen(prefetchListener);
  });
}

function test_prefetch() {
  // This test does not run in e10s-mode, so we'll just go ahead and skip it.
  // We've left the e10s test code in below, just in case someone wants to try
  // to make it work at some point in the future.
  if (!running_single_process) {
    dump("skipping test_prefetch due to e10s\n");
    run_next_test();
    return;
  }

  // Setup for this has all been taken care of by test_prefetch_prime, so we can
  // continue on without pausing here.
  if (running_single_process) {
    continue_test_prefetch();
  } else {
    sendCommand("continue_test_prefetch();");
  }
}

function continue_test_prefetch() {
  var prefetches = [prefetch_sruri.asciiSpec];
  var verifier = new Verifier("prefetch", prefetches, [], []);
  predictor.predict(prefetch_tluri, null, predictor.PREDICT_LOAD, origin_attributes, verifier);
}

function cleanup() {
  observer.cleaningUp = true;
  if (running_single_process) {
    // The http server is required (and started) by the prefetch test, which
    // only runs in single-process mode, so don't try to shut it down if we're
    // in e10s mode.
    do_test_pending();
    httpserv.stop(do_test_finished);
  }
  reset_predictor();
}

var tests = [
  // This must ALWAYS come first, to ensure a clean slate
  reset_predictor,
  test_link_hover,
  test_pageload,
  // TODO: These are disabled until the features are re-written
  //test_redirect,
  //test_startup,
  // END DISABLED TESTS
  test_origin,
  test_dns,
  test_prefetch_setup,
  test_prefetch_prime,
  test_prefetch_prime,
  test_prefetch,
  // This must ALWAYS come last, to ensure we clean up after ourselves
  cleanup
];

var observer = {
  cleaningUp: false,

  QueryInterface: function (iid) {
    if (iid.equals(Ci.nsIObserver) ||
        iid.equals(Ci.nsISupports)) {
      return this;
    }

    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  observe: function (subject, topic, data) {
    if (topic != "predictor-reset-complete") {
      return;
    }

    if (this.cleaningUp) {
      unregisterObserver();
    }

    run_next_test();
  }
};

function registerObserver() {
  Services.obs.addObserver(observer, "predictor-reset-complete");
}

function unregisterObserver() {
  Services.obs.removeObserver(observer, "predictor-reset-complete");
}

function run_test_real() {
  tests.forEach(f => add_test(f));
  do_get_profile();

  Services.prefs.setBoolPref("network.predictor.enabled", true);
  Services.prefs.setBoolPref("network.predictor.cleaned-up", true);
  Services.prefs.setBoolPref("network.predictor.doing-tests", true);

  predictor = Cc["@mozilla.org/network/predictor;1"].getService(Ci.nsINetworkPredictor);

  registerObserver();

  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("network.predictor.preconnect-min-confidence");
    Services.prefs.clearUserPref("network.predictor.enabled");
    Services.prefs.clearUserPref("network.predictor.cleaned-up");
    Services.prefs.clearUserPref("network.predictor.preresolve-min-confidence");
    Services.prefs.clearUserPref("network.predictor.enable-prefetch");
    Services.prefs.clearUserPref("network.predictor.prefetch-rolling-load-count");
    Services.prefs.clearUserPref("network.predictor.doing-tests");
  });

  run_next_test();
}

function run_test() {
  // This indirection is necessary to make e10s tests work as expected
  running_single_process = true;
  run_test_real();
}
