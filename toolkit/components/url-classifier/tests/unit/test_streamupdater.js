function doTest(updates, assertions, expectError)
{
  if (expectError) {
    doUpdateTest(updates, assertions, updateError, runNextTest);
  } else {
    doUpdateTest(updates, assertions, runNextTest, updateError);
  }
}

function testSimpleForward() {
  var add1Urls = [ "foo.com/a", "bar.com/c" ];
  var add2Urls = [ "foo.com/b" ];
  var add3Urls = [ "bar.com/d" ];

  var update = "n:1000\n";

  var update1 = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:data:," + encodeURIComponent(update1) + "\n";

  var update2 = buildPhishingUpdate(
    [{ "chunkNum" : 2,
       "urls" : add2Urls }]);
  update += "u:data:," + encodeURIComponent(update2) + "\n";

  var update3 = buildPhishingUpdate(
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

  var update1 = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:data:," + encodeURIComponent(update1) + "\n";

  var update2 = buildPhishingUpdate(
    [{ "chunkNum" : 2 }]);
  var update3 = buildPhishingUpdate(
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

  var assertions = {
    "tableData" : "",
    "urlsDontExist" : add1Urls
  };

  doTest([update], assertions, true);
}

// A failed network request causes the update to fail.
function testErrorUrlForward() {
  var add1Urls = [ "foo.com/a", "bar.com/c" ];

  var update = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls }]);
  update += "u:http://test.invalid/asdf/asdf\n";  // invalid URL scheme

  var assertions = {
    "tableData" : "",
    "urlsDontExist" : add1Urls
  };

  doTest([update], assertions, true);
}

function run_test()
{
  runTests([
    testSimpleForward,
    testNestedForward,
    testInvalidUrlForward,
    testErrorUrlForward
  ]);
}

do_test_pending();
