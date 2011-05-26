/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
Components.utils.import("resource://gre/modules/NetUtil.jsm");
const Ci = Components.interfaces;

// TEST DATA
// ---------
var gTests = [
  { spec:    "about:blank",
    scheme:  "about",
    prePath: "about:",
    path:    "blank",
    ref:     "",
    nsIURL:  false, nsINestedURI: true, immutable: true },
  { spec:    "about:foobar",
    scheme:  "about",
    prePath: "about:",
    path:    "foobar",
    ref:     "",
    nsIURL:  false, nsINestedURI: false, immutable: true },
  { spec:    "chrome://foobar/somedir/somefile.xml",
    scheme:  "chrome",
    prePath: "chrome://foobar",
    path:    "/somedir/somefile.xml",
    ref:     "",
    nsIURL:  true, nsINestedURI: false, immutable: true },
  { spec:    "data:text/html;charset=utf-8,<html></html>",
    scheme:  "data",
    prePath: "data:",
    path:    "text/html;charset=utf-8,<html></html>",
    ref:     "",
    nsIURL:  false, nsINestedURI: false },
  { spec:    "data:text/plain,hello world",
    scheme:  "data",
    prePath: "data:",
    path:    "text/plain,hello%20world",
    ref:     "",
    nsIURL:  false, nsINestedURI: false },
  { spec:    "file://",
    scheme:  "file",
    prePath: "file://",
    path:    "/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "file:///",
    scheme:  "file",
    prePath: "file://",
    path:    "/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "file:///myFile.html",
    scheme:  "file",
    prePath: "file://",
    path:    "/myFile.html",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://",
    scheme:  "http",
    prePath: "http://",
    path:    "/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http:///",
    scheme:  "http",
    prePath: "http://",
    path:    "/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "http://www.example.com/",
    scheme:  "http",
    prePath: "http://www.example.com",
    path:    "/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "jar:resource://!/",
    scheme:  "jar",
    prePath: "jar:",
    path:    "resource:///!/",
    ref:     "",
    nsIURL:  true, nsINestedURI: true },
  { spec:    "jar:resource://gre/chrome.toolkit.jar!/",
    scheme:  "jar",
    prePath: "jar:",
    path:    "resource://gre/chrome.toolkit.jar!/",
    ref:     "",
    nsIURL:  true, nsINestedURI: true },
  { spec:    "javascript:new Date()",
    scheme:  "javascript",
    prePath: "javascript:",
    path:    "new%20Date()",
    ref:     "",
    nsIURL:  false, nsINestedURI: false },
  { spec:    "place:redirectsMode=2&sort=8&maxResults=10",
    scheme:  "place",
    prePath: "place:",
    path:    "redirectsMode=2&sort=8&maxResults=10",
    ref:     "",
    nsIURL:  false, nsINestedURI: false },
  { spec:    "resource://gre/",
    scheme:  "resource",
    prePath: "resource://gre",
    path:    "/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "resource://gre/components/",
    scheme:  "resource",
    prePath: "resource://gre",
    path:    "/components/",
    ref:     "",
    nsIURL:  true, nsINestedURI: false },
  { spec:    "view-source:about:blank",
    scheme:  "view-source",
    prePath: "view-source:",
    path:    "about:blank",
    ref:     "",
    nsIURL:  false, nsINestedURI: true, immutable: true },
  { spec:    "view-source:http://www.mozilla.org/",
    scheme:  "view-source",
    prePath: "view-source:",
    path:    "http://www.mozilla.org/",
    ref:     "",
    nsIURL:  false, nsINestedURI: true, immutable: true },
  { spec:    "x-external:",
    scheme:  "x-external",
    prePath: "x-external:",
    path:    "",
    ref:     "",
    nsIURL:  false, nsINestedURI: false },
  { spec:    "x-external:abc",
    scheme:  "x-external",
    prePath: "x-external:",
    path:    "abc",
    ref:     "",
    nsIURL:  false, nsINestedURI: false },
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
  var expectedVal = aTestFunctor ?
    aTestFunctor(aTest[aPropertyName]) :
    aTest[aPropertyName];

  do_info("testing " + aPropertyName + " of " +
          (aTestFunctor ? "modified '" : "'" ) + aTest.spec +
          "' is '" + expectedVal + "'");
  do_check_eq(aURI[aPropertyName], expectedVal);
}

// Test that a given URI parses correctly into its various components.
function do_test_uri_basic(aTest) {
  var URI = NetUtil.newURI(aTest.spec);

  // Sanity-check
  do_info("testing " + aTest.spec + " equals a clone of itself");
  do_check_uri_eq(URI, URI.clone());
  do_check_uri_eq(URI, URI.cloneIgnoringRef());
  do_info("testing " + aTest.spec + " instanceof nsIURL");
  do_check_eq(URI instanceof Ci.nsIURL, aTest.nsIURL);
  do_info("testing " + aTest.spec + " instanceof nsINestedURI");
  do_check_eq(URI instanceof Ci.nsINestedURI,
              aTest.nsINestedURI);

  // Check the various components
  do_check_property(aTest, URI, "scheme");
  do_check_property(aTest, URI, "prePath");
  do_check_property(aTest, URI, "path");
  do_check_property(aTest, URI, "ref");
}

// Test that a given URI parses correctly when we add a given ref to the end
function do_test_uri_with_hash_suffix(aTest, aSuffix) {
  do_info("making sure caller is using suffix that starts with '#'");
  do_check_eq(aSuffix[0], "#");

  var testURI = NetUtil.newURI(aTest.spec + aSuffix);
  var origURI = NetUtil.newURI(aTest.spec);

  do_info("testing " + aTest.spec + " with '" + aSuffix + "' appended " +
           "equals a clone of itself");
  do_check_uri_eq(testURI, testURI.clone());

  do_info("testing " + aTest.spec +
          " doesn't equal self with '" + aSuffix + "' appended");
  if (aTest.spec == "file://") {
    do_info("TODO: bug 656853");
    todo_check_false(origURI.equals(testURI));
    return;  // bail out early since file:// doesn't handle hash refs at all
  }

  do_check_false(origURI.equals(testURI));

  do_info("testing " + aTest.spec +
          " is equalExceptRef to self with '" + aSuffix + "' appended");
  do_check_uri_eqExceptRef(origURI, testURI);

  do_info("testing cloneIgnoringRef on " + testURI.spec +
          " is equal to no-ref version but not equal to ref version");
  var cloneNoRef = testURI.cloneIgnoringRef();
  if (aTest.spec == "http://" && aSuffix == "#") {
    do_info("TODO: bug 657033");
    do_check_uri_eq(cloneNoRef, origURI, todo_check_true);
  } else {
    do_check_uri_eq(cloneNoRef, origURI);
  }
  do_check_false(cloneNoRef.equals(testURI));

  do_check_property(aTest, testURI, "scheme");
  do_check_property(aTest, testURI, "prePath");
  do_check_property(aTest, testURI, "path",
                    function(aStr) { return aStr + aSuffix; });
  do_check_property(aTest, testURI, "ref",
                    function(aStr) { return aSuffix.substr(1); });
}

// Tests various ways of setting & clearing a ref on a URI.
function do_test_mutate_ref(aTest, aSuffix) {
  do_info("making sure caller is using suffix that starts with '#'");
  do_check_eq(aSuffix[0], "#");

  var refURIWithSuffix    = NetUtil.newURI(aTest.spec + aSuffix);
  var refURIWithoutSuffix = NetUtil.newURI(aTest.spec);

  var testURI             = NetUtil.newURI(aTest.spec);

  if (aTest.spec == "file://") {
    do_info("TODO: bug 656853");
    testURI.ref = aSuffix;
    do_check_uri_eq(testURI, refURIWithSuffix, todo_check_true);
    return; // bail out early since file:// doesn't handle hash refs at all
  }

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

  // Now try setting .spec directly (including suffix) and then clearing .ref
  var specWithSuffix = aTest.spec + aSuffix;
  do_info("testing that setting spec to " +
          specWithSuffix + " and then clearing ref does what we expect");
  testURI.spec = specWithSuffix
  testURI.ref = "";
  if (aTest.spec == "http://" && aSuffix == "#") {
    do_info("TODO: bug 657033");
    do_check_uri_eq(testURI, refURIWithoutSuffix, todo_check_true);
    do_check_uri_eqExceptRef(testURI, refURIWithSuffix, todo_check_true);
  } else {
    do_check_uri_eq(testURI, refURIWithoutSuffix);
    do_check_uri_eqExceptRef(testURI, refURIWithSuffix);
  }

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
  gTests.forEach(function(aTest) {
    // Check basic URI functionality
    do_test_uri_basic(aTest);

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
  });
}
