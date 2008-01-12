var dbservice = Cc["@mozilla.org/url-classifier/dbservice;1"].getService(Ci.nsIUrlClassifierDBService);

var gAssertions = {

tableData : function(expectedTables, cb)
{
  dbservice.getTables(function(tables) {
      // rebuild the tables in a predictable order.
      var parts = tables.split("\n");
      parts.sort();
      tables = parts.join("\n");

      // tables now has a leading empty newline, because split left an
      // empty string after the trailing newline;
      do_check_eq(tables, "\n" + expectedTables);
      cb();
    });
},

checkUrls: function(urls, expected, cb)
{
  var doLookup = function() {
    if (urls.length > 0) {
      var fragment = urls.shift();
      dbservice.lookup("http://" + fragment,
                       function(arg) {
                         do_check_eq(expected, arg);
                         doLookup();
                       }, true);
    } else {
      cb();
    }
  }
  doLookup();
},

urlsDontExist: function(urls, cb)
{
  this.checkUrls(urls, '', cb);
},

urlsExist: function(urls, cb)
{
  this.checkUrls(urls, 'test-phish-simple', cb);
},

subsDontExist: function(urls, cb)
{
  // XXX: there's no interface for checking items in the subs table
  cb();
},

subsExist: function(urls, cb)
{
  // XXX: there's no interface for checking items in the subs table
  cb();
}

};

function updateError(arg)
{
  do_throw(arg);
}

function checkAssertions(assertions)
{
  var checkAssertion = function() {
    for (var i in assertions) {
      var data = assertions[i];
      delete assertions[i];
      gAssertions[i](data, checkAssertion);
      return;
    }

    runNextTest();
  }

  checkAssertion();
}

function doTest(updates, assertions) {
  dbservice.resetDatabase();

  var runUpdate = function() {
    if (updates.length > 0) {
      var update = updates.shift();
      dbservice.update(update);
      dbservice.finish(runUpdate, updateError);
    } else {
      checkAssertions(assertions);
    }
  }

  runUpdate();
}

function buildPhishingUpdate(chunks) {
  return buildUpdate({"test-phish-simple" : chunks});
}

// Test an add of two urls to a fresh database
function testSimpleAdds() {
  var addUrls = [ "foo.com/a", "foo.com/b", "bar.com/c" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }]);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsExist" : addUrls
  };

  doTest([update], assertions);
}

// Same as testSimpleAdds, but make the same-domain URLs come from different
// chunks.
function testMultipleAdds() {
  var add1Urls = [ "foo.com/a", "bar.com/c" ];
  var add2Urls = [ "foo.com/b" ];

  var update = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "urls" : add1Urls },
      { "chunkNum" : 2,
        "urls" : add2Urls }]);
  var assertions = {
    "tableData" : "test-phish-simple;a:1-2",
    "urlsExist" : add1Urls.concat(add2Urls)
  };

  doTest([update], assertions);
}

// Test that a sub will remove an existing add
function testSimpleSub()
{
  var addUrls = ["foo.com/a", "bar.com/b"];
  var subUrls = ["foo.com/a"];

  var addUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 1, // adds and subtracts don't share a chunk numbering space
       "urls": addUrls }]);

  var subUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "chunkType" : "s",
       "urls": subUrls }]);

  var assertions = {
    "tableData" : "test-phish-simple;a:1:s:1",
    "urlsExist" : [ "bar.com/b" ],
    "urlsDontExist": ["foo.com/a" ],
    "subsDontExist" : [ "foo.com/a" ]
  }

  doTest([addUpdate, subUpdate], assertions);

}

// Same as testSimpleSub(), but the sub comes in before the add.
function testSubEmptiesAdd()
{
  var subUrls = ["foo.com/a"];
  var addUrls = ["foo.com/a", "bar.com/b"];

  var subUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "chunkType" : "s",
       "urls": subUrls }]);

  var addUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 1, // adds and subtracts don't share a chunk numbering space
       "urls": addUrls }]);

  var assertions = {
    "tableData" : "test-phish-simple;a:1:s:1",
    "urlsExist" : [ "bar.com/b" ],
    "urlsDontExist": ["foo.com/a" ],
    "subsDontExist" : [ "foo.com/a" ] // this sub was found, it shouldn't exist anymore
  }

  doTest([subUpdate, addUpdate], assertions);
}

// Very similar to testSubEmptiesAdd, except that the domain entry will
// still have an item left over that needs to be synced.
function testSubPartiallyEmptiesAdd()
{
  var subUrls = ["foo.com/a"];
  var addUrls = ["foo.com/a", "foo.com/b", "bar.com/b"];

  var subUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "chunkType" : "s",
       "urls": subUrls }]);

  var addUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 1, // adds and subtracts don't share a chunk numbering space
       "urls": addUrls }]);

  var assertions = {
    "tableData" : "test-phish-simple;a:1:s:1",
    "urlsExist" : [ "foo.com/b", "bar.com/b" ],
    "urlsDontExist" : ["foo.com/a" ],
    "subsDontExist" : [ "foo.com/a" ] // this sub was found, it shouldn't exist anymore
  }

  doTest([subUpdate, addUpdate], assertions);
}

// We SHOULD be testing that pending subs are removed using
// subsDontExist assertions.  Since we don't have a good interface for getting
// at sub entries, we'll verify it by side-effect.  Subbing a url once
// then adding it twice should leave the url intact.
function testPendingSubRemoved()
{
  var subUrls = ["foo.com/a", "foo.com/b"];
  var addUrls = ["foo.com/a", "foo.com/b"];

  var subUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "chunkType" : "s",
       "urls": subUrls }]);

  var addUpdate1 = buildPhishingUpdate(
    [{ "chunkNum" : 1, // adds and subtracts don't share a chunk numbering space
       "urls": addUrls }]);

  var addUpdate2 = buildPhishingUpdate(
    [{ "chunkNum" : 2,
       "urls": addUrls }]);

  var assertions = {
    "tableData" : "test-phish-simple;a:1-2:s:1",
    "urlsExist" : [ "foo.com/a", "foo.com/b" ],
    "subsDontExist" : [ "foo.com/a", "foo.com/b" ] // this sub was found, it shouldn't exist anymore
  }

  doTest([subUpdate, addUpdate1, addUpdate2], assertions);
}

// Make sure that a saved sub is removed when the sub chunk is expired.
function testPendingSubExpire()
{
  var subUrls = ["foo.com/a", "foo.com/b"];
  var addUrls = ["foo.com/a", "foo.com/b"];

  var subUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "chunkType" : "s",
       "urls": subUrls }]);

  var expireUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "chunkType" : "sd" }]);

  var addUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 1, // adds and subtracts don't share a chunk numbering space
       "urls": addUrls }]);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsExist" : [ "foo.com/a", "foo.com/b" ],
    "subsDontExist" : [ "foo.com/a", "foo.com/b" ] // this sub was expired
  }

  doTest([subUpdate, expireUpdate, addUpdate], assertions);
}

// Two adds plus one sub of the same URL will leave one of the adds there
function testDuplicateAdds()
{
  var urls = ["foo.com/a"];

  var addUpdate1 = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "urls": urls }]);
  var addUpdate2 = buildPhishingUpdate(
    [{ "chunkNum" : 2,
       "urls": urls }]);
  var subUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 3,
       "chunkType" : "s",
       "urls": urls }]);

  var assertions = {
    "tableData" : "test-phish-simple;a:1-2:s:3",
    "urlsExist" : [ "foo.com/a"],
    "subsDontExist" : [ "foo.com/a"]
  }

  doTest([addUpdate1, addUpdate2, subUpdate], assertions);
}

// Tests a sub which matches some existing adds but leaves others.
function testSubPartiallyMatches()
{
  var addUrls = ["foo.com/a"];
  var subUrls = ["foo.com/a", "foo.com/b"];

  var addUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "chunkType" : "s",
       "urls" : addUrls }]);

  var subUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "urls" : subUrls }]);

  var assertions = {
    "tableData" : "test-phish-simple;a:1:s:1",
    "urlsDontExist" : ["foo.com/a"],
    "subsDontExist" : ["foo.com/a"],
    "subsExist" : ["foo.com/b"]
  };

  doTest([addUpdate, subUpdate], assertions);
}

// XXX: because subsExist isn't actually implemented, this is the same
// test as above but with a second add chunk that should fail to be added
// because of a pending sub chunk.
function testSubPartiallyMatches2()
{
  var addUrls = ["foo.com/a"];
  var subUrls = ["foo.com/a", "foo.com/b"];
  var addUrls2 = ["foo.com/b"];

  var addUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "urls" : addUrls }]);

  var subUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "chunkType" : "s",
       "urls" : subUrls }]);

  var addUpdate2 = buildPhishingUpdate(
    [{ "chunkNum" : 2,
       "urls" : addUrls2 }]);

  var assertions = {
    "tableData" : "test-phish-simple;a:1-2:s:1",
    "urlsDontExist" : ["foo.com/a", "foo.com/b"],
    "subsDontExist" : ["foo.com/a", "foo.com/b"]
  };

  doTest([addUpdate, subUpdate, addUpdate2], assertions);
}

// Verify that two subs for the same domain but from different chunks
// match (tests that existing sub entries are properly updated, and
// helps exercise nsUrlClassifierEntry::RemoveFragments().
function testSubsDifferentChunks() {
  var subUrls1 = [ "foo.com/a" ];
  var subUrls2 = [ "foo.com/b" ];

  var addUrls = [ "foo.com/a", "foo.com/b", "foo.com/c" ];

  var subUpdate1 = buildPhishingUpdate(
    [{ "chunkNum" : 1,
       "chunkType" : "s",
       "urls": subUrls1 }]);
  var subUpdate2 = buildPhishingUpdate(
    [{ "chunkNum" : 2,
       "chunkType" : "s",
       "urls" : subUrls2 }]);
  var addUpdate = buildPhishingUpdate(
    [{ "chunkNum" : 3,
       "urls" : addUrls }]);

  var assertions = {
    "tableData" : "test-phish-simple;a:3:s:1-2",
    "urlsExist" : [ "foo.com/c" ],
    "urlsDontExist" : [ "foo.com/a", "foo.com/b" ],
    "subsDontExist" : [ "foo.com/a", "foo.com/b" ]
  };

  doTest([subUpdate1, subUpdate2, addUpdate], assertions);
}

var gTests = [
  testSimpleAdds,
  testMultipleAdds,
  testSimpleSub,
  testSubEmptiesAdd,
  testSubPartiallyEmptiesAdd,
  testPendingSubRemoved,
  testPendingSubExpire,
  testDuplicateAdds,
  testSubPartiallyMatches,
  testSubPartiallyMatches2,
  testSubsDifferentChunks,
];

var gNextTest = 0;

function runNextTest()
{
  if (gNextTest >= gTests.length) {
    do_test_finished();
    return;
  }

  gTests[gNextTest++]();
}

function run_test()
{
  runNextTest();
}

do_test_pending();
