function doTest(updates, assertions, expectError)
{
  if (expectError) {
    doUpdateTest(updates, assertions, updateError, runNextTest);
  } else {
    doUpdateTest(updates, assertions, runNextTest, updateError);
  }
}

function testFillDb() {
  var add1Urls = [ "zaz.com/a", "yxz.com/c" ];

  var update = "n:1000\n";
  update += "i:test-phish-simple\n";

  var update1 = buildBareUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:data:," + encodeURIComponent(update1) + "\n";

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsExist" : add1Urls
  };

  doTest([update], assertions, false);
}

function testSimpleForward() {
  var add1Urls = [ "foo.com/a", "bar.com/c" ];
  var add2Urls = [ "foo.com/b" ];
  var add3Urls = [ "bar.com/d" ];

  var update = "n:1000\n";
  update += "i:test-phish-simple\n";

  var update1 = buildBareUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:data:," + encodeURIComponent(update1) + "\n";

  var update2 = buildBareUpdate(
    [{ "chunkNum" : 2,
       "urls" : add2Urls }]);
  update += "u:data:," + encodeURIComponent(update2) + "\n";

  var update3 = buildBareUpdate(
    [{ "chunkNum" : 3,
       "urls" : add3Urls }]);
  update += "u:data:," + encodeURIComponent(update3) + "\n";

  var assertions = {
    "tableData" : "test-phish-simple;a:1-3",
    "urlsExist" : add1Urls.concat(add2Urls).concat(add3Urls)
  };

  doTest([update], assertions, false);
}

// Make sure that a nested forward (a forward within a forward) causes
// the update to fail.
function testNestedForward() {
  var add1Urls = [ "foo.com/a", "bar.com/c" ];
  var add2Urls = [ "foo.com/b" ];

  var update = "n:1000\n";
  update += "i:test-phish-simple\n";

  var update1 = buildBareUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:data:," + encodeURIComponent(update1) + "\n";

  var update2 = buildBareUpdate(
    [{ "chunkNum" : 2 }]);
  var update3 = buildBareUpdate(
    [{ "chunkNum" : 3,
       "urls" : add1Urls }]);

  update2 += "u:data:," + encodeURIComponent(update3) + "\n";

  update += "u:data:," + encodeURIComponent(update2) + "\n";

  var assertions = {
    "tableData" : "",
    "urlsDontExist" : add1Urls.concat(add2Urls)
  };

  doTest([update], assertions, true);
}

// An invalid URL forward causes the update to fail.
function testInvalidUrlForward() {
  var add1Urls = [ "foo.com/a", "bar.com/c" ];

  var update = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:asdf://blah/blah\n";  // invalid URL scheme

  // The first part of the update should have succeeded.

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsExist" : add1Urls
  };

  doTest([update], assertions, false);
}

// A failed network request causes the update to fail.
function testErrorUrlForward() {
  var add1Urls = [ "foo.com/a", "bar.com/c" ];

  var update = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:http://test.invalid/asdf/asdf\n";  // invalid URL scheme

  // The first part of the update should have succeeded

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsExist" : add1Urls
  };

  doTest([update], assertions, false);
}

function testMultipleTables() {
  var add1Urls = [ "foo.com/a", "bar.com/c" ];
  var add2Urls = [ "foo.com/b" ];
  var add3Urls = [ "bar.com/d" ];

  var update = "n:1000\n";
  update += "i:test-phish-simple\n";

  var update1 = buildBareUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:data:," + encodeURIComponent(update1) + "\n";

  var update2 = buildBareUpdate(
    [{ "chunkNum" : 2,
       "urls" : add2Urls }]);
  update += "u:data:," + encodeURIComponent(update2) + "\n";

  update += "i:test-malware-simple\n";

  var update3 = buildBareUpdate(
    [{ "chunkNum" : 3,
       "urls" : add3Urls }]);
  update += "u:data:," + encodeURIComponent(update3) + "\n";

  var assertions = {
    "tableData" : "test-malware-simple;a:3\ntest-phish-simple;a:1-2",
    "urlsExist" : add1Urls.concat(add2Urls),
    "malwareUrlsExist" : add3Urls
  };

  doTest([update], assertions, false);
}

function Observer(callback) {
  this.observe = callback;
}

Observer.prototype =
{
QueryInterface: function(iid)
{
  if (!iid.equals(Ci.nsISupports) &&
      !iid.equals(Ci.nsIObserver)) {
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
  return this;
}
};

// Tests a database reset request.
function testReset() {
  var addUrls1 = [ "foo.com/a", "foo.com/b" ];
  var update1 = buildPhishingUpdate(
    [
      { "chunkNum" : 1,
        "urls" : addUrls1
      }]);

  var update2 = "n:1000\nr:pleasereset\n";

  var addUrls3 = [ "bar.com/a", "bar.com/b" ];
  var update3 = buildPhishingUpdate(
    [
      { "chunkNum" : 3,
        "urls" : addUrls3
      }]);

  var assertions = {
    "tableData" : "test-phish-simple;a:3",
    "urlsExist" : addUrls3,
    "urlsDontExist" : addUrls1
  };

  doTest([update1, update2, update3], assertions, false);
}


function run_test()
{
  runTests([
    testSimpleForward,
    testNestedForward,
    testInvalidUrlForward,
    testErrorUrlForward,
    testMultipleTables,
    testReset
  ]);
}

do_test_pending();
