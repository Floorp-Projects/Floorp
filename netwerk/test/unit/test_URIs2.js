/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
Components.utils.import("resource://gre/modules/NetUtil.jsm");

var gIoService = Components.classes["@mozilla.org/network/io-service;1"]
                           .getService(Components.interfaces.nsIIOService);


// Run by: cd objdir;  make -C netwerk/test/ xpcshell-tests    
// or: cd objdir; make SOLO_FILE="test_URIs2.js" -C netwerk/test/ check-one

// This is a clone of test_URIs.js, with a different set of test data in gTests.
// The original test data in test_URIs.js was split between test_URIs and test_URIs2.js
// because test_URIs.js was running for too long on slow platforms, causing 
// intermittent timeouts.

// Relevant RFCs: 1738, 1808, 2396, 3986 (newer than the code)
// http://greenbytes.de/tech/webdav/rfc3986.html#rfc.section.5.4
// http://greenbytes.de/tech/tc/uris/

// TEST DATA
// ---------
var gTests = [
  // relative URL testcases from http://greenbytes.de/tech/webdav/rfc3986.html#rfc.section.5.4
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "g:h",
    scheme:  "g",
    prePath: "g:",
    path:    "h",
    ref:     "",
    nsIURL:  false, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "g",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/g",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "./g",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/g",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "g/",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/g/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "/g",
    scheme:  "http",
    prePath: "http://a",
    path:    "/g",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "?y",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/d;p?y",
    ref:     "",// fix
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "g?y",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/g?y",
    ref:     "",// fix
    specIgnoringRef: "http://a/b/c/g?y",
    hasRef:  false,
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "#s",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/d;p?q#s",
    ref:     "s",// fix
    specIgnoringRef: "http://a/b/c/d;p?q",
    hasRef:  true,
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "g#s",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/g#s",
    ref:     "s",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "g?y#s",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/g?y#s",
    ref:     "s",
    nsIURL:  true, nsINestedURI: false },
  /*
    Bug xxxxxx - we return a path of b/c/;x
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: ";x",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/d;x",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  */
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "g;x",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/g;x",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "g;x?y#s",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/g;x?y#s",
    ref:     "s",
    nsIURL:  true, nsINestedURI: false },
  /*
    Can't easily specify a relative URI of "" to the test code
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/d",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  */
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: ".",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "./",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "..",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "../",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "../g",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/g",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "../..",
    scheme:  "http",
    prePath: "http://a",
    path:    "/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "../../",
    scheme:  "http",
    prePath: "http://a",
    path:    "/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "../../g",
    scheme:  "http",
    prePath: "http://a",
    path:    "/g",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },

  // abnormal examples
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "../../../g",
    scheme:  "http",
    prePath: "http://a",
    path:    "/g",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "../../../../g",
    scheme:  "http",
    prePath: "http://a",
    path:    "/g",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },

  // coalesce 
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "/./g",
    scheme:  "http",
    prePath: "http://a",
    path:    "/g",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "/../g",
    scheme:  "http",
    prePath: "http://a",
    path:    "/g",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "g.",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/g.",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: ".g",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/.g",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "g..",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/g..",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "..g",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/..g",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: ".",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "./../g",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/g",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "./g/.",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/g/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "g/./h",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/g/h",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "g/../h",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/h",
    ref:     "",// fix
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "g;x=1/./y",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/g;x=1/y",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://a/b/c/d;p?q",
    relativeURI: "g;x=1/../y",
    scheme:  "http",
    prePath: "http://a",
    path:    "/b/c/y",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  // protocol-relative http://tools.ietf.org/html/rfc3986#section-4.2
  { spec:    "http://www2.example.com/",
    relativeURI: "//www3.example2.com/bar",
    scheme:  "http",
    prePath: "http://www3.example2.com",
    path:    "/bar",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "https://www2.example.com/",
    relativeURI: "//www3.example2.com/bar",
    scheme:  "https",
    prePath: "https://www3.example2.com",
    path:    "/bar",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
];

var gHashSuffixes = [
  "#",
  "#myRef",
  "#myRef?a=b",
  "#myRef#",
  "#myRef#x:yz"
];

// TEST HELPER FUNCTIONS
// ---------------------
function do_info(text, stack) {
  if (!stack)
    stack = Components.stack.caller;

  dump( "\n" +
       "TEST-INFO | " + stack.filename + " | [" + stack.name + " : " +
       stack.lineNumber + "] " + text + "\n");
}

// Checks that the URIs satisfy equals(), in both possible orderings.
// Also checks URI.equalsExceptRef(), because equal URIs should also be equal
// when we ignore the ref.
// 
// The third argument is optional. If the client passes a third argument
// (e.g. todo_check_true), we'll use that in lieu of do_check_true.
function do_check_uri_eq(aURI1, aURI2, aCheckTrueFunc) {
  if (!aCheckTrueFunc) {
    aCheckTrueFunc = do_check_true;
  }

  do_info("(uri equals check: '" + aURI1.spec + "' == '" + aURI2.spec + "')");
  aCheckTrueFunc(aURI1.equals(aURI2));
  do_info("(uri equals check: '" + aURI2.spec + "' == '" + aURI1.spec + "')");
  aCheckTrueFunc(aURI2.equals(aURI1));

  // (Only take the extra step of testing 'equalsExceptRef' when we expect the
  // URIs to really be equal.  In 'todo' cases, the URIs may or may not be
  // equal when refs are ignored - there's no way of knowing in general.)
  if (aCheckTrueFunc == do_check_true) {
    do_check_uri_eqExceptRef(aURI1, aURI2, aCheckTrueFunc);
  }
}

// Checks that the URIs satisfy equalsExceptRef(), in both possible orderings.
//
// The third argument is optional. If the client passes a third argument
// (e.g. todo_check_true), we'll use that in lieu of do_check_true.
function do_check_uri_eqExceptRef(aURI1, aURI2, aCheckTrueFunc) {
  if (!aCheckTrueFunc) {
    aCheckTrueFunc = do_check_true;
  }

  do_info("(uri equalsExceptRef check: '" +
          aURI1.spec + "' == '" + aURI2.spec + "')");
  aCheckTrueFunc(aURI1.equalsExceptRef(aURI2));
  do_info("(uri equalsExceptRef check: '" +
          aURI2.spec + "' == '" + aURI1.spec + "')");
  aCheckTrueFunc(aURI2.equalsExceptRef(aURI1));
}

// Checks that the given property on aURI matches the corresponding property
// in the test bundle (or matches some function of that corresponding property,
// if aTestFunctor is passed in).
function do_check_property(aTest, aURI, aPropertyName, aTestFunctor) {
  if (aTest[aPropertyName]) {
    var expectedVal = aTestFunctor ?
                      aTestFunctor(aTest[aPropertyName]) :
                      aTest[aPropertyName];

    do_info("testing " + aPropertyName + " of " +
            (aTestFunctor ? "modified '" : "'" ) + aTest.spec +
            "' is '" + expectedVal + "'");
    do_check_eq(aURI[aPropertyName], expectedVal);
  }
}

// Test that a given URI parses correctly into its various components.
function do_test_uri_basic(aTest) {
  var URI;

  do_info("Basic tests for " + aTest.spec + " relative URI: " + aTest.relativeURI);

  try {
    URI = NetUtil.newURI(aTest.spec);
  } catch(e) {
    do_info("Caught error on parse of" + aTest.spec + " Error: " + e.result);
    if (aTest.fail) {
      do_check_eq(e.result, aTest.result);
      return;
    }
    do_throw(e.result);
  }

  if (aTest.relativeURI) {
    var relURI;

    try {
      relURI = gIoService.newURI(aTest.relativeURI, null, URI);
    } catch (e) {
      do_info("Caught error on Relative parse of " + aTest.spec + " + " + aTest.relativeURI +" Error: " + e.result);
      if (aTest.relativeFail) {
        do_check_eq(e.result, aTest.relativeFail);
        return;
      }
      do_throw(e.result);
    }
    do_info("relURI.path = " + relURI.path + ", was " + URI.path);
    URI = relURI;
    do_info("URI.path now = " + URI.path);
  }

  // Sanity-check
  do_info("testing " + aTest.spec + " equals a clone of itself");
  do_check_uri_eq(URI, URI.clone());
  do_check_uri_eqExceptRef(URI, URI.cloneIgnoringRef());
  do_info("testing " + aTest.spec + " instanceof nsIURL");
  do_check_eq(URI instanceof Ci.nsIURL, aTest.nsIURL);
  do_info("testing " + aTest.spec + " instanceof nsINestedURI");
  do_check_eq(URI instanceof Ci.nsINestedURI,
              aTest.nsINestedURI);

  do_info("testing that " + aTest.spec + " throws or returns false " +
          "from equals(null)");
  // XXXdholbert At some point it'd probably be worth making this behavior
  // (throwing vs. returning false) consistent across URI implementations.
  var threw = false;
  var isEqualToNull;
  try {
    isEqualToNull = URI.equals(null);
  } catch(e) {
    threw = true;
  }
  do_check_true(threw || !isEqualToNull);


  // Check the various components
  do_check_property(aTest, URI, "scheme");
  do_check_property(aTest, URI, "prePath");
  do_check_property(aTest, URI, "path");
  do_check_property(aTest, URI, "ref");
  do_check_property(aTest, URI, "port");
  do_check_property(aTest, URI, "username");
  do_check_property(aTest, URI, "password");
  do_check_property(aTest, URI, "host");
  do_check_property(aTest, URI, "specIgnoringRef");
  if ("hasRef" in aTest) {
    do_info("testing hasref: " + aTest.hasRef + " vs " + URI.hasRef);
    do_check_eq(aTest.hasRef, URI.hasRef);
  }
}

// Test that a given URI parses correctly when we add a given ref to the end
function do_test_uri_with_hash_suffix(aTest, aSuffix) {
  do_info("making sure caller is using suffix that starts with '#'");
  do_check_eq(aSuffix[0], "#");

  var origURI = NetUtil.newURI(aTest.spec);
  var testURI;

  if (aTest.relativeURI) {
    try {
      origURI = gIoService.newURI(aTest.relativeURI, null, origURI);
    } catch (e) {
      do_info("Caught error on Relative parse of " + aTest.spec + " + " + aTest.relativeURI +" Error: " + e.result);
      return;
    }
    try {
      testURI = gIoService.newURI(aSuffix, null, origURI);
    } catch (e) {
      do_info("Caught error adding suffix to " + aTest.spec + " + " + aTest.relativeURI + ", suffix " + aSuffix + " Error: " + e.result);
      return;
    }
  } else {
    testURI = NetUtil.newURI(aTest.spec + aSuffix);
  }

  do_info("testing " + aTest.spec + " with '" + aSuffix + "' appended " +
           "equals a clone of itself");
  do_check_uri_eq(testURI, testURI.clone());

  do_info("testing " + aTest.spec +
          " doesn't equal self with '" + aSuffix + "' appended");

  do_check_false(origURI.equals(testURI));

  do_info("testing " + aTest.spec +
          " is equalExceptRef to self with '" + aSuffix + "' appended");
  do_check_uri_eqExceptRef(origURI, testURI);

  do_check_eq(testURI.hasRef, true);

  if (!origURI.ref) {
    // These tests fail if origURI has a ref
    do_info("testing cloneIgnoringRef on " + testURI.spec +
            " is equal to no-ref version but not equal to ref version");
    var cloneNoRef = testURI.cloneIgnoringRef();
    do_check_uri_eq(cloneNoRef, origURI);
    do_check_false(cloneNoRef.equals(testURI));
  }

  do_check_property(aTest, testURI, "scheme");
  do_check_property(aTest, testURI, "prePath");
  if (!origURI.ref) {
    // These don't work if it's a ref already because '+' doesn't give the right result
    do_check_property(aTest, testURI, "path",
                      function(aStr) { return aStr + aSuffix; });
    do_check_property(aTest, testURI, "ref",
                      function(aStr) { return aSuffix.substr(1); });
  }
}

// Tests various ways of setting & clearing a ref on a URI.
function do_test_mutate_ref(aTest, aSuffix) {
  do_info("making sure caller is using suffix that starts with '#'");
  do_check_eq(aSuffix[0], "#");

  var refURIWithSuffix    = NetUtil.newURI(aTest.spec + aSuffix);
  var refURIWithoutSuffix = NetUtil.newURI(aTest.spec);

  var testURI             = NetUtil.newURI(aTest.spec);

  // First: Try setting .ref to our suffix
  do_info("testing that setting .ref on " + aTest.spec +
          " to '" + aSuffix + "' does what we expect");
  testURI.ref = aSuffix;
  do_check_uri_eq(testURI, refURIWithSuffix);
  do_check_uri_eqExceptRef(testURI, refURIWithoutSuffix);

  // Now try setting .ref but leave off the initial hash (expect same result)
  var suffixLackingHash = aSuffix.substr(1);
  if (suffixLackingHash) { // (skip this our suffix was *just* a #)
    do_info("testing that setting .ref on " + aTest.spec +
            " to '" + suffixLackingHash + "' does what we expect");
    testURI.ref = suffixLackingHash;
    do_check_uri_eq(testURI, refURIWithSuffix);
    do_check_uri_eqExceptRef(testURI, refURIWithoutSuffix);
  }

  // Now, clear .ref (should get us back the original spec)
  do_info("testing that clearing .ref on " + testURI.spec +
          " does what we expect");
  testURI.ref = "";
  do_check_uri_eq(testURI, refURIWithoutSuffix);
  do_check_uri_eqExceptRef(testURI, refURIWithSuffix);

  if (!aTest.relativeURI) {
    // TODO: These tests don't work as-is for relative URIs.

    // Now try setting .spec directly (including suffix) and then clearing .ref
    var specWithSuffix = aTest.spec + aSuffix;
    do_info("testing that setting spec to " +
            specWithSuffix + " and then clearing ref does what we expect");
    testURI.spec = specWithSuffix;
    testURI.ref = "";
    do_check_uri_eq(testURI, refURIWithoutSuffix);
    do_check_uri_eqExceptRef(testURI, refURIWithSuffix);

    // XXX nsIJARURI throws an exception in SetPath(), so skip it for next part.
    if (!(testURI instanceof Ci.nsIJARURI)) {
      // Now try setting .path directly (including suffix) and then clearing .ref
      // (same as above, but with now with .path instead of .spec)
      testURI = NetUtil.newURI(aTest.spec);

      var pathWithSuffix = aTest.path + aSuffix;
      do_info("testing that setting path to " +
              pathWithSuffix + " and then clearing ref does what we expect");
      testURI.path = pathWithSuffix;
      testURI.ref = "";
      do_check_uri_eq(testURI, refURIWithoutSuffix);
      do_check_uri_eqExceptRef(testURI, refURIWithSuffix);

      // Also: make sure that clearing .path also clears .ref
      testURI.path = pathWithSuffix;
      do_info("testing that clearing path from " + 
              pathWithSuffix + " also clears .ref");
      testURI.path = "";
      do_check_eq(testURI.ref, "");
    }
  }
}

// Tests that normally-mutable properties can't be modified on
// special URIs that are known to be immutable.
function do_test_immutable(aTest) {
  do_check_true(aTest.immutable);

  var URI = NetUtil.newURI(aTest.spec);
  // All the non-readonly attributes on nsIURI.idl:
  var propertiesToCheck = ["spec", "scheme", "userPass", "username", "password",
                           "hostPort", "host", "port", "path", "ref"];

  propertiesToCheck.forEach(function(aProperty) {
    var threw = false;
    try {
      URI[aProperty] = "anothervalue";
    } catch(e) {
      threw = true;
    }

    do_info("testing that setting '" + aProperty +
            "' on immutable URI '" + aTest.spec + "' will throw");
    do_check_true(threw);
  });
}


// TEST MAIN FUNCTION
// ------------------
function run_test()
{
  // UTF-8 check - From bug 622981
  // ASCII
  let base = gIoService.newURI("http://example.org/xenia?", null, null);
  let resolved = gIoService.newURI("?x", null, base);
  let expected = gIoService.newURI("http://example.org/xenia?x",
                                  null, null);
  do_info("Bug 662981: ACSII - comparing " + resolved.spec + " and " + expected.spec);
  do_check_true(resolved.equals(expected));

  // UTF-8 character "è"
  // Bug 622981 was triggered by an empty query string
  base = gIoService.newURI("http://example.org/xènia?", null, null);
  resolved = gIoService.newURI("?x", null, base);
  expected = gIoService.newURI("http://example.org/xènia?x",
                              null, null);
  do_info("Bug 662981: UTF8 - comparing " + resolved.spec + " and " + expected.spec);
  do_check_true(resolved.equals(expected));

  gTests.forEach(function(aTest) {
    // Check basic URI functionality
    do_test_uri_basic(aTest);

    if (!aTest.fail) {
      // Try adding various #-prefixed strings to the ends of the URIs
      gHashSuffixes.forEach(function(aSuffix) {
          do_test_uri_with_hash_suffix(aTest, aSuffix);
          if (!aTest.immutable) {
            do_test_mutate_ref(aTest, aSuffix);
          }
        });

      // For URIs that we couldn't mutate above due to them being immutable:
      // Now we check that they're actually immutable.
      if (aTest.immutable) {
        do_test_immutable(aTest);
      }
    }
  });
}
