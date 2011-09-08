
/**
 * DummyCompleter() lets tests easily specify the results of a partial
 * hash completion request.
 */
function DummyCompleter() {
  this.fragments = {};
  this.queries = [];
  this.cachable = true;
  this.tableName = "test-phish-simple";
}

DummyCompleter.prototype =
{
QueryInterface: function(iid)
{
  if (!iid.equals(Ci.nsISupports) &&
      !iid.equals(Ci.nsIUrlClassifierHashCompleter)) {
    throw Cr.NS_ERROR_NO_INTERFACE;
  }
  return this;
},

complete: function(partialHash, cb)
{
  this.queries.push(partialHash);
  var fragments = this.fragments;
  var self = this;
  var doCallback = function() {
      if (self.alwaysFail) {
        cb.completionFinished(1);
        return;
      }
      var results;
      if (fragments[partialHash]) {
        for (var i = 0; i < fragments[partialHash].length; i++) {
          var chunkId = fragments[partialHash][i][0];
          var hash = fragments[partialHash][i][1];
          cb.completion(hash, self.tableName, chunkId, self.cachable);
        }
      }
    cb.completionFinished(0);
  }
  var timer = new Timer(0, doCallback);
},

getHash: function(fragment)
{
  var converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].
  createInstance(Ci.nsIScriptableUnicodeConverter);
  converter.charset = "UTF-8";
  var data = converter.convertToByteArray(fragment);
  var ch = Cc["@mozilla.org/security/hash;1"].createInstance(Ci.nsICryptoHash);
  ch.init(ch.SHA256);
  ch.update(data, data.length);
  var hash = ch.finish(false);
  return hash.slice(0, 32);
},

addFragment: function(chunkId, fragment)
{
  this.addHash(chunkId, this.getHash(fragment));
},

// This method allows the caller to generate complete hashes that match the
// prefix of a real fragment, but have different complete hashes.
addConflict: function(chunkId, fragment)
{
  var realHash = this.getHash(fragment);
  var invalidHash = this.getHash("blah blah blah blah blah");
  this.addHash(chunkId, realHash.slice(0, 4) + invalidHash.slice(4, 32));
},

addHash: function(chunkId, hash)
{
  var partial = hash.slice(0, 4);
  if (this.fragments[partial]) {
    this.fragments[partial].push([chunkId, hash]);
  } else {
    this.fragments[partial] = [[chunkId, hash]];
  }
},

compareQueries: function(fragments)
{
  var expectedQueries = [];
  for (var i = 0; i < fragments.length; i++) {
    expectedQueries.push(this.getHash(fragments[i]).slice(0, 4));
  }
  do_check_eq(this.queries.length, expectedQueries.length);
  expectedQueries.sort();
  this.queries.sort();
  for (var i = 0; i < this.queries.length; i++) {
    do_check_eq(this.queries[i], expectedQueries[i]);
  }
}
};

function setupCompleter(table, hits, conflicts)
{
  var completer = new DummyCompleter();
  completer.tableName = table;
  for (var i = 0; i < hits.length; i++) {
    var chunkId = hits[i][0];
    var fragments = hits[i][1];
    for (var j = 0; j < fragments.length; j++) {
      completer.addFragment(chunkId, fragments[j]);
    }
  }
  for (var i = 0; i < conflicts.length; i++) {
    var chunkId = conflicts[i][0];
    var fragments = conflicts[i][1];
    for (var j = 0; j < fragments.length; j++) {
      completer.addConflict(chunkId, fragments[j]);
    }
  }

  dbservice.setHashCompleter(table, completer);

  return completer;
}

function installCompleter(table, fragments, conflictFragments)
{
  return setupCompleter(table, fragments, conflictFragments);
}

function installFailingCompleter(table) {
  var completer = setupCompleter(table, [], []);
  completer.alwaysFail = true;
  return completer;
}

function installUncachableCompleter(table, fragments, conflictFragments)
{
  var completer = setupCompleter(table, fragments, conflictFragments);
  completer.cachable = false;
  return completer;
}

// Helper assertion for checking dummy completer queries
gAssertions.completerQueried = function(data, cb)
{
  var completer = data[0];
  completer.compareQueries(data[1]);
  cb();
}

function doTest(updates, assertions)
{
  doUpdateTest(updates, assertions, runNextTest, updateError);
}

// Test an add of two partial urls to a fresh database
function testPartialAdds() {
  var addUrls = [ "foo.com/a", "foo.com/b", "bar.com/c" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
    4);


  var completer = installCompleter('test-phish-simple', [[1, addUrls]], []);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsExist" : addUrls,
    "completerQueried" : [completer, addUrls]
  };


  doTest([update], assertions);
}

function testPartialAddsWithConflicts() {
  var addUrls = [ "foo.com/a", "foo.com/b", "bar.com/c" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
    4);

  // Each result will have both a real match and a conflict
  var completer = installCompleter('test-phish-simple',
                                   [[1, addUrls]],
                                   [[1, addUrls]]);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsExist" : addUrls,
    "completerQueried" : [completer, addUrls]
  };

  doTest([update], assertions);
}

// Test whether the fragmenting code does not cause duplicated completions
function testFragments() {
  var addUrls = [ "foo.com/a/b/c", "foo.net/", "foo.com/c/" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
    4);


  var completer = installCompleter('test-phish-simple', [[1, addUrls]], []);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsExist" : addUrls,
    "completerQueried" : [completer, addUrls]
  };


  doTest([update], assertions);
}

// Test http://code.google.com/p/google-safe-browsing/wiki/Protocolv2Spec
// section 6.2 example 1
function testSpecFragments() {
  var probeUrls = [ "a.b.c/1/2.html?param=1" ];

  var addUrls = [ "a.b.c/1/2.html",
                  "a.b.c/",
                  "a.b.c/1/",
                  "b.c/1/2.html?param=1",
                  "b.c/1/2.html",
                  "b.c/",
                  "b.c/1/",
                  "a.b.c/1/2.html?param=1" ];

  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
    4);


  var completer = installCompleter('test-phish-simple', [[1, addUrls]], []);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsExist" : probeUrls,
    "completerQueried" : [completer, addUrls]
  };

  doTest([update], assertions);

}

// Test http://code.google.com/p/google-safe-browsing/wiki/Protocolv2Spec
// section 6.2 example 2
function testMoreSpecFragments() {
  var probeUrls = [ "a.b.c.d.e.f.g/1.html" ];

  var addUrls = [ "a.b.c.d.e.f.g/1.html",
                  "a.b.c.d.e.f.g/",
                  "c.d.e.f.g/1.html",
                  "c.d.e.f.g/",
                  "d.e.f.g/1.html",
                  "d.e.f.g/",
                  "e.f.g/1.html",
                  "e.f.g/",
                  "f.g/1.html",
                  "f.g/" ];

  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
    4);

  var completer = installCompleter('test-phish-simple', [[1, addUrls]], []);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsExist" : probeUrls,
    "completerQueried" : [completer, addUrls]
  };

  doTest([update], assertions);

}

function testFalsePositives() {
  var addUrls = [ "foo.com/a", "foo.com/b", "bar.com/c" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
    4);

  // Each result will have no matching complete hashes and a non-matching
  // conflict
  var completer = installCompleter('test-phish-simple', [], [[1, addUrls]]);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsDontExist" : addUrls,
    "completerQueried" : [completer, addUrls]
  };

  doTest([update], assertions);
}

function testEmptyCompleter() {
  var addUrls = [ "foo.com/a", "foo.com/b", "bar.com/c" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
    4);

  // Completer will never return full hashes
  var completer = installCompleter('test-phish-simple', [], []);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsDontExist" : addUrls,
    "completerQueried" : [completer, addUrls]
  };

  doTest([update], assertions);
}

function testCompleterFailure() {
  var addUrls = [ "foo.com/a", "foo.com/b", "bar.com/c" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
    4);

  // Completer will never return full hashes
  var completer = installFailingCompleter('test-phish-simple');

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsDontExist" : addUrls,
    "completerQueried" : [completer, addUrls]
  };

  doTest([update], assertions);
}

function testMixedSizesSameDomain() {
  var add1Urls = [ "foo.com/a" ];
  var add2Urls = [ "foo.com/b" ];

  var update1 = buildPhishingUpdate(
    [
      { "chunkNum" : 1,
        "urls" : add1Urls }],
    4);
  var update2 = buildPhishingUpdate(
    [
      { "chunkNum" : 2,
        "urls" : add2Urls }],
    32);

  // We should only need to complete the partial hashes
  var completer = installCompleter('test-phish-simple', [[1, add1Urls]], []);

  var assertions = {
    "tableData" : "test-phish-simple;a:1-2",
    // both urls should match...
    "urlsExist" : add1Urls.concat(add2Urls),
    // ... but the completer should only be queried for the partial entry
    "completerQueried" : [completer, add1Urls]
  };

  doTest([update1, update2], assertions);
}

function testMixedSizesDifferentDomains() {
  var add1Urls = [ "foo.com/a" ];
  var add2Urls = [ "bar.com/b" ];

  var update1 = buildPhishingUpdate(
    [
      { "chunkNum" : 1,
        "urls" : add1Urls }],
    4);
  var update2 = buildPhishingUpdate(
    [
      { "chunkNum" : 2,
        "urls" : add2Urls }],
    32);

  // We should only need to complete the partial hashes
  var completer = installCompleter('test-phish-simple', [[1, add1Urls]], []);

  var assertions = {
    "tableData" : "test-phish-simple;a:1-2",
    // both urls should match...
    "urlsExist" : add1Urls.concat(add2Urls),
    // ... but the completer should only be queried for the partial entry
    "completerQueried" : [completer, add1Urls]
  };

  doTest([update1, update2], assertions);
}

function testInvalidHashSize()
{
  var addUrls = [ "foo.com/a", "foo.com/b", "bar.com/c" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
        12); // only 4 and 32 are legal hash sizes

  var addUrls2 = [ "zaz.com/a", "xyz.com/b" ];
  var update2 = buildPhishingUpdate(
        [
          { "chunkNum" : 2,
            "urls" : addUrls2
          }],
        4);

  var completer = installCompleter('test-phish-simple', [[1, addUrls]], []);

  var assertions = {
    "tableData" : "test-phish-simple;a:2",
    "urlsDontExist" : addUrls
  };

  // A successful update will trigger an error
  doUpdateTest([update2, update], assertions, updateError, runNextTest);
}

function testWrongTable()
{
  var addUrls = [ "foo.com/a" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
        4);
  var completer = installCompleter('test-malware-simple', // wrong table
                                   [[1, addUrls]], []);

  // The above installCompleter installs the completer for test-malware-simple,
  // we want it to be used for test-phish-simple too.
  dbservice.setHashCompleter("test-phish-simple", completer);


  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    // The urls were added as phishing urls, but the completer is claiming
    // that they are malware urls, and we trust the completer in this case.
    "malwareUrlsExist" : addUrls,
    // Make sure the completer was actually queried.
    "completerQueried" : [completer, addUrls]
  };

  doUpdateTest([update], assertions,
               function() {
                 // Give the dbservice a chance to (not) cache the result.
                 var timer = new Timer(3000, function() {
                     // The dbservice shouldn't have cached this result,
                     // so this completer should be queried.
                     var newCompleter = installCompleter('test-malware-simple', [[1, addUrls]], []);

                     // The above installCompleter installs the
                     // completer for test-malware-simple, we want it
                     // to be used for test-phish-simple too.
                     dbservice.setHashCompleter("test-phish-simple",
                                                newCompleter);


                     var assertions = {
                       "malwareUrlsExist" : addUrls,
                       "completerQueried" : [newCompleter, addUrls]
                     };
                     checkAssertions(assertions, runNextTest);
                   });
               }, updateError);
}

function testWrongChunk()
{
  var addUrls = [ "foo.com/a" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
        4);
  var completer = installCompleter('test-phish-simple',
                                   [[2, // wrong chunk number
                                     addUrls]], []);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsExist" : addUrls,
    // Make sure the completer was actually queried.
    "completerQueried" : [completer, addUrls]
  };

  doUpdateTest([update], assertions,
               function() {
                 // Give the dbservice a chance to (not) cache the result.
                 var timer = new Timer(3000, function() {
                     // The dbservice shouldn't have cached this result,
                     // so this completer should be queried.
                     var newCompleter = installCompleter('test-phish-simple', [[2, addUrls]], []);

                     var assertions = {
                       "urlsExist" : addUrls,
                       "completerQueried" : [newCompleter, addUrls]
                     };
                     checkAssertions(assertions, runNextTest);
                   });
               }, updateError);
}

function setupCachedResults(addUrls, part2)
{
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
        4);

  var completer = installCompleter('test-phish-simple', [[1, addUrls]], []);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    // Request the add url.  This should cause the completion to be cached.
    "urlsExist" : addUrls,
    // Make sure the completer was actually queried.
    "completerQueried" : [completer, addUrls]
  };

  doUpdateTest([update], assertions,
               function() {
                 // Give the dbservice a chance to cache the result.
                 var timer = new Timer(3000, part2);
               }, updateError);
}

function testCachedResults()
{
  setupCachedResults(["foo.com/a"], function(add) {
      // This is called after setupCachedResults().  Verify that
      // checking the url again does not cause a completer request.

      // install a new completer, this one should never be queried.
      var newCompleter = installCompleter('test-phish-simple', [[1, []]], []);

      var assertions = {
        "urlsExist" : ["foo.com/a"],
        "completerQueried" : [newCompleter, []]
      };
      checkAssertions(assertions, runNextTest);
    });
}

function testCachedResultsWithSub() {
  setupCachedResults(["foo.com/a"], function() {
      // install a new completer, this one should never be queried.
      var newCompleter = installCompleter('test-phish-simple', [[1, []]], []);

      var removeUpdate = buildPhishingUpdate(
        [ { "chunkNum" : 2,
            "chunkType" : "s",
            "urls": ["1:foo.com/a"] }],
        4);

      var assertions = {
        "urlsDontExist" : ["foo.com/a"],
        "completerQueried" : [newCompleter, []]
      }

      doTest([removeUpdate], assertions);
    });
}

function testCachedResultsWithExpire() {
  setupCachedResults(["foo.com/a"], function() {
      // install a new completer, this one should never be queried.
      var newCompleter = installCompleter('test-phish-simple', [[1, []]], []);

      var expireUpdate =
        "n:1000\n" +
        "i:test-phish-simple\n" +
        "ad:1\n";

      var assertions = {
        "urlsDontExist" : ["foo.com/a"],
        "completerQueried" : [newCompleter, []]
      }
      doTest([expireUpdate], assertions);
    });
}

function setupUncachedResults(addUrls, part2)
{
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
        4);

  var completer = installUncachableCompleter('test-phish-simple', [[1, addUrls]], []);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    // Request the add url.  This should cause the completion to be cached.
    "urlsExist" : addUrls,
    // Make sure the completer was actually queried.
    "completerQueried" : [completer, addUrls]
  };

  doUpdateTest([update], assertions,
               function() {
                 // Give the dbservice a chance to cache the result.
                 var timer = new Timer(3000, part2);
               }, updateError);
}

function testUncachedResults()
{
  setupUncachedResults(["foo.com/a"], function(add) {
      // This is called after setupCachedResults().  Verify that
      // checking the url again does not cause a completer request.

      // install a new completer, this one should be queried.
      var newCompleter = installCompleter('test-phish-simple', [[1, ["foo.com/a"]]], []);
      var assertions = {
        "urlsExist" : ["foo.com/a"],
        "completerQueried" : [newCompleter, ["foo.com/a"]]
      };
      checkAssertions(assertions, runNextTest);
    });
}

function testErrorList()
{
  var addUrls = [ "foo.com/a", "foo.com/b", "bar.com/c" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
    32);

  var completer = installCompleter('test-phish-simple', [[1, addUrls]], []);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsExist" : addUrls,
    // These are complete urls, and will only be completed if the
    // list is stale.
    "completerQueried" : [completer, addUrls]
  };

  // Apply the update.
  doStreamUpdate(update, function() {
      // Now the test-phish-simple and test-malware-simple tables are marked
      // as fresh.  Fake an update failure to mark them stale.
      doErrorUpdate("test-phish-simple,test-malware-simple", function() {
          // Now the lists should be marked stale.  Check assertions.
          checkAssertions(assertions, runNextTest);
        }, updateError);
    }, updateError);
}


function testStaleList()
{
  var addUrls = [ "foo.com/a", "foo.com/b", "bar.com/c" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
    32);

  var completer = installCompleter('test-phish-simple', [[1, addUrls]], []);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsExist" : addUrls,
    // These are complete urls, and will only be completed if the
    // list is stale.
    "completerQueried" : [completer, addUrls]
  };

  // Consider a match stale after one second.
  prefBranch.setIntPref("urlclassifier.confirm-age", 1);

  // Apply the update.
  doStreamUpdate(update, function() {
      // Now the test-phish-simple and test-malware-simple tables are marked
      // as fresh.  Wait three seconds to make sure the list is marked stale.
      new Timer(3000, function() {
          // Now the lists should be marked stale.  Check assertions.
          checkAssertions(assertions, function() {
              prefBranch.setIntPref("urlclassifier.confirm-age", 2700);
              runNextTest();
            });
        }, updateError);
    }, updateError);
}

// Same as testStaleList, but verifies that an empty response still
// unconfirms the entry.
function testStaleListEmpty()
{
  var addUrls = [ "foo.com/a", "foo.com/b", "bar.com/c" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }],
        32);

  var completer = installCompleter('test-phish-simple', [], []);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    // None of these should match, because they won't be completed
    "urlsDontExist" : addUrls,
    // These are complete urls, and will only be completed if the
    // list is stale.
    "completerQueried" : [completer, addUrls]
  };

  // Consider a match stale after one second.
  prefBranch.setIntPref("urlclassifier.confirm-age", 1);

  // Apply the update.
  doStreamUpdate(update, function() {
      // Now the test-phish-simple and test-malware-simple tables are marked
      // as fresh.  Wait three seconds to make sure the list is marked stale.
      new Timer(3000, function() {
          // Now the lists should be marked stale.  Check assertions.
          checkAssertions(assertions, function() {
              prefBranch.setIntPref("urlclassifier.confirm-age", 2700);
              runNextTest();
            });
        }, updateError);
    }, updateError);
}


// Verify that different lists (test-phish-simple,
// test-malware-simple) maintain their freshness separately.
function testErrorListIndependent()
{
  var phishUrls = [ "phish.com/a" ];
  var malwareUrls = [ "attack.com/a" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : phishUrls
          }],
    32);

  update += buildMalwareUpdate(
        [
          { "chunkNum" : 2,
            "urls" : malwareUrls
          }],
    32);

  var completer = installCompleter('test-phish-simple', [[1, phishUrls]], []);

  var assertions = {
    "tableData" : "test-malware-simple;a:2\ntest-phish-simple;a:1",
    "urlsExist" : phishUrls,
    "malwareUrlsExist" : malwareUrls,
    // Only this phishing urls should be completed, because only the phishing
    // urls will be stale.
    "completerQueried" : [completer, phishUrls]
  };

  // Apply the update.
  doStreamUpdate(update, function() {
      // Now the test-phish-simple and test-malware-simple tables are
      // marked as fresh.  Fake an update failure to mark *just*
      // phishing data as stale.
      doErrorUpdate("test-phish-simple", function() {
          // Now the lists should be marked stale.  Check assertions.
          checkAssertions(assertions, runNextTest);
        }, updateError);
    }, updateError);
}

function run_test()
{
  runTests([
      testPartialAdds,
      testPartialAddsWithConflicts,
      testFragments,
      testSpecFragments,
      testMoreSpecFragments,
      testFalsePositives,
      testEmptyCompleter,
      testCompleterFailure,
      testMixedSizesSameDomain,
      testMixedSizesDifferentDomains,
      testInvalidHashSize,
      testWrongTable,
      testWrongChunk,
      testCachedResults,
      testCachedResultsWithSub,
      testCachedResultsWithExpire,
      testUncachedResults,
      testStaleList,
      testStaleListEmpty,
      testErrorList,
      testErrorListIndependent,
  ]);
}

do_test_pending();
