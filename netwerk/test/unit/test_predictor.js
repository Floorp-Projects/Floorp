var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/FileUtils.jsm");

var predictor = null;
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
    if (iid.equals(Ci.nsINetworkPredictorVerifier) ||
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
  complete: false,
  verifying: null,
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
    if (this.expected_preconnects.length === 0 &&
        this.expected_preresolves.length === 0 &&
        !this.complete) {
      this.complete = true;
      do_check_true(true, "Well this is unexpected...");
      // This kicks off the ability to run the next test
      reset_predictor();
    }
  },

  onPredictPreconnect: function verifier_onPredictPreconnect(uri) {
    var origin = extract_origin(uri);
    var index = this.expected_preconnects.indexOf(origin);
    if (index == -1 && !this.complete) {
      do_check_true(false, "Got preconnect for unexpected uri " + origin);
    } else {
      this.expected_preconnects.splice(index, 1);
    }
    this.maybe_run_next_test();
  },

  onPredictDNS: function verifier_onPredictDNS(uri) {
    var origin = extract_origin(uri);
    var index = this.expected_preresolves.indexOf(origin);
    if (index == -1 && !this.complete) {
      do_check_true(false, "Got preresolve for unexpected uri " + origin);
    } else {
      this.expected_preresolves.splice(index, 1);
    }
    this.maybe_run_next_test();
  }
};

function reset_predictor() {
  predictor.reset();
}

function newURI(s) {
  return ios.newURI(s, null, null);
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
    do_check_eq(result, Cr.NS_OK);
    entry.setMetaDataElement("predictor_test", "1");
    entry.metaDataReady();
    this.numEntriesOpened++;
    if (this.numEntriesToOpen == this.numEntriesOpened) {
      this.continueCallback();
    }
  }
};

function open_and_continue(uris, continueCallback) {
  var lci = {
    QueryInterface: function (iid) {
      if (iid.equals(Ci.nsILoadContextInfo)) {
        return this;
      }
      throw Cr.NS_ERROR_NO_INTERFACE;
    },

    isPrivate: false,
    appId: Ci.nsILoadContextInfo.NO_APP_ID,
    isInBrowserElement: false,
    isAnonymous: false
  };

  var css = Cc["@mozilla.org/netwerk/cache-storage-service;1"]
    .getService(Ci.nsICacheStorageService);
  var ds = css.diskCacheStorage(lci, false);

  prepListener.init(uris.length, continueCallback);
  for (var i = 0; i < uris.length; ++i) {
    ds.asyncOpenURI(uris[i], "", Ci.nsICacheStorage.OPEN_NORMALLY,
                    prepListener);
  }
}

function test_link_hover() {
  var uri = newURI("http://localhost:4444/foo/bar");
  var referrer = newURI("http://localhost:4444/foo");
  var preconns = ["http://localhost:4444"];

  var verifier = new Verifier("hover", preconns, []);
  predictor.predict(uri, referrer, predictor.PREDICT_LINK, load_context, verifier);
}

function test_pageload() {
  var toplevel = "http://localhost:4444/index.html";
  var subresources = [
    "http://localhost:4444/style.css",
    "http://localhost:4443/jquery.js",
    "http://localhost:4444/image.png"
  ];

  var tluri = newURI(toplevel);
  open_and_continue([tluri], function () {
    // This is necessary to learn the origin stuff
    predictor.learn(tluri, null, predictor.LEARN_LOAD_TOPLEVEL, load_context);
    var preconns = [];
    for (var i = 0; i < subresources.length; i++) {
      var sruri = newURI(subresources[i]);
      predictor.learn(sruri, tluri, predictor.LEARN_LOAD_SUBRESOURCE, load_context);
      preconns.push(extract_origin(sruri));
    }

    var verifier = new Verifier("pageload", preconns, []);
    predictor.predict(tluri, null, predictor.PREDICT_LOAD, load_context, verifier);
  });
}

function test_redirect() {
  var initial = "http://localhost:4443/redirect";
  var target = "http://localhost:4444/index.html";
  var subresources = [
    "http://localhost:4444/style.css",
    "http://localhost:4443/jquery.js",
    "http://localhost:4444/image.png"
  ];

  var inituri = newURI(initial);
  var targeturi = newURI(target);
  open_and_continue([inituri, targeturi], function () {
    predictor.learn(inituri, null, predictor.LEARN_LOAD_TOPLEVEL, load_context);
    predictor.learn(targeturi, null, predictor.LEARN_LOAD_TOPLEVEL, load_context);
    predictor.learn(targeturi, inituri, predictor.LEARN_LOAD_REDIRECT, load_context);

    var preconns = [];
    preconns.push(extract_origin(targeturi));
    for (var i = 0; i < subresources.length; i++) {
      var sruri = newURI(subresources[i]);
      predictor.learn(sruri, targeturi, predictor.LEARN_LOAD_SUBRESOURCE, load_context);
      preconns.push(extract_origin(sruri));
    }

    var verifier = new Verifier("redirect", preconns, []);
    predictor.predict(inituri, null, predictor.PREDICT_LOAD, load_context, verifier);
  });
}

function test_startup() {
  var uris = [
    "http://localhost:4444/startup",
    "http://localhost:4443/startup"
  ];
  var preconns = [];
  for (var i = 0; i < uris.length; i++) {
    var uri = newURI(uris[i]);
    predictor.learn(uri, null, predictor.LEARN_STARTUP, load_context);
    preconns.push(extract_origin(uri));
  }

  var verifier = new Verifier("startup", preconns, []);
  predictor.predict(null, null, predictor.PREDICT_STARTUP, load_context, verifier);
}

function test_dns() {
  var toplevel = "http://localhost:4444/index.html";
  var subresource = "http://localhost:4443/jquery.js";

  var tluri = newURI(toplevel);
  open_and_continue([tluri], function () {
    predictor.learn(tluri, null, predictor.LEARN_LOAD_TOPLEVEL, load_context);
    var sruri = newURI(subresource);
    predictor.learn(sruri, tluri, predictor.LEARN_LOAD_SUBRESOURCE, load_context);

    // Ensure that this will do preresolves
    prefs.setIntPref("network.predictor.preconnect-min-confidence", 101);

    var preresolves = [extract_origin(sruri)];
    var verifier = new Verifier("dns", [], preresolves);
    predictor.predict(tluri, null, predictor.PREDICT_LOAD, load_context, verifier);
  });
}

function test_origin() {
  var toplevel = "http://localhost:4444/index.html";
  var subresources = [
    "http://localhost:4444/style.css",
    "http://localhost:4443/jquery.js",
    "http://localhost:4444/image.png"
  ];

  var tluri = newURI(toplevel);
  open_and_continue([tluri], function () {
    predictor.learn(tluri, null, predictor.LEARN_LOAD_TOPLEVEL, load_context);
    var preconns = [];
    for (var i = 0; i < subresources.length; i++) {
      var sruri = newURI(subresources[i]);
      predictor.learn(sruri, tluri, predictor.LEARN_LOAD_SUBRESOURCE, load_context);
      var origin = extract_origin(sruri);
      if (preconns.indexOf(origin) === -1) {
        preconns.push(origin);
      }
    }

    var loaduri = newURI("http://localhost:4444/anotherpage.html");
    var verifier = new Verifier("origin", preconns, []);
    predictor.predict(loaduri, null, predictor.PREDICT_LOAD, load_context, verifier);
  });
}

var prefs;

function cleanup() {
  observer.cleaningUp = true;
  predictor.reset();
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
  var svc = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
  svc.addObserver(observer, "predictor-reset-complete", false);
}

function unregisterObserver() {
  var svc = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
  svc.removeObserver(observer, "predictor-reset-complete");
}

function run_test() {
  tests.forEach(add_test);
  profile = do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);
  prefs.setBoolPref("network.predictor.enabled", true);
  prefs.setBoolPref("network.predictor.cleaned-up", true);
  prefs.setBoolPref("browser.cache.use_new_backend_temp", true);
  prefs.setIntPref("browser.cache.use_new_backend", 1);
  do_register_cleanup(() => {
      prefs.clearUserPref("network.predictor.preconnect-min-confidence");
      prefs.clearUserPref("network.predictor.enabled");
      prefs.clearUserPref("network.predictor.cleaned-up");
      prefs.clearUserPref("browser.cache.use_new_backend_temp");
      prefs.clearUserPref("browser.cache.use_new_backend");
  });
  predictor = Cc["@mozilla.org/network/predictor;1"].getService(Ci.nsINetworkPredictor);
  registerObserver();
  run_next_test();
}
