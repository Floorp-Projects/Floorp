// Test an add of two urls to a fresh database
function testCleanHostKeys() {
  var addUrls = [ "foo.com/a" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }]);

  doStreamUpdate(update, function() {
      var ios = Components.classes["@mozilla.org/network/io-service;1"].
        getService(Components.interfaces.nsIIOService);

      // Check with a clean host key
      var uri = ios.newURI("http://bar.com/a", null, null);

      // Use the nsIURIClassifier interface (the
      // nsIUrlClassifierDBService will always queue a lookup,
      // nsIURIClassifier won't if the host key is known to be clean.
      var classifier = dbservice.QueryInterface(Ci.nsIURIClassifier);
      var result = classifier.classify(uri, function(errorCode) {
          var result2 = classifier.classify(uri, function() {
              do_throw("shouldn't get a callback");
            });
          // second call shouldn't result in a callback.
          do_check_eq(result2, false);

          runNextTest();
        });

      // The first classifier call should result in a callback.
      do_check_eq(result, true);
    }, updateError);
}

// Test an add of two urls to a fresh database
function testDirtyHostKeys() {
  var addUrls = [ "foo.com/a" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }]);

  doStreamUpdate(update, function() {
      var ios = Components.classes["@mozilla.org/network/io-service;1"].
        getService(Components.interfaces.nsIIOService);

      // Check with a dirty host key - both checks should happen.
      var uri = ios.newURI("http://foo.com/b", null, null);

      // Use the nsIURIClassifier interface (the
      // nsIUrlClassifierDBService will always queue a lookup,
      // nsIURIClassifier won't if the host key is known to be clean.
      var classifier = dbservice.QueryInterface(Ci.nsIURIClassifier);
      var result = classifier.classify(uri, function(errorCode) {
          var result2 = classifier.classify(uri, function() {
              runNextTest();
            });
          // second call should result in a callback.
          do_check_eq(result2, true);
        });

      // The first classifier call should result in a callback.
      do_check_eq(result, true);
    }, updateError);
}

// Make sure that an update properly clears the host key cache
function testUpdate() {
  var ios = Components.classes["@mozilla.org/network/io-service;1"].
    getService(Components.interfaces.nsIIOService);

  // First lookup should happen...
  var uri = ios.newURI("http://foo.com/a", null, null);

  // Use the nsIURIClassifier interface (the
  // nsIUrlClassifierDBService will always queue a lookup,
  // nsIURIClassifier won't if the host key is known to be clean.
  var classifier = dbservice.QueryInterface(Ci.nsIURIClassifier);
  var result = classifier.classify(uri, function(errorCode) {
      // This check will succeed, which will cause the key to
      // be put in the clean host key cache...
      do_check_eq(errorCode, Cr.NS_OK);

      // Now add the url to the db...
      var addUrls = [ "foo.com/a" ];
      var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }]);
      doStreamUpdate(update, function() {
          // The clean key cache should be blown now that we've
          // added, and this callback should execute.
          var result2 = classifier.classify(uri, function(errorCode) {
              do_check_neq(errorCode, Cr.NS_OK);
              runNextTest();
            });
          // second call should result in a callback.
          do_check_eq(result2, true);
        }, updateError);
    });
}

function testResetFullCache() {
  // First do enough queries to fill up the clean hostkey cache
  var ios = Components.classes["@mozilla.org/network/io-service;1"].
    getService(Components.interfaces.nsIIOService);

  // Use the nsIURIClassifier interface (the
  // nsIUrlClassifierDBService will always queue a lookup,
  // nsIURIClassifier won't if the host key is known to be clean.
  var classifier = dbservice.QueryInterface(Ci.nsIURIClassifier);

  var uris1 = [
    "www.foo.com/",
    "www.bar.com/",
    "www.blah.com/",
    "www.site.com/",
    "www.example.com/",
    "www.test.com/",
    "www.malware.com/",
    "www.phishing.com/",
    "www.clean.com/" ];

  var uris2 = [];

  var runSecondLookup = function() {
    if (uris2.length == 0) {
      runNextTest();
      return;
    }

    var spec = uris2.pop();
    var uri = ios.newURI("http://" + spec, null, null);

    var result = classifier.classify(uri, function(errorCode) {
        runSecondLookup();
      });
    // now look up a few more times.
  }

  var runInitialLookup = function() {
    if (uris1.length == 0) {
      // We're done filling up the cache.  Run an update to flush it,
      // then start lookup up again.
      var addUrls = [ "notgoingtocheck.com/a" ];
      var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }]);
      doStreamUpdate(update, function() {
          runSecondLookup();
        }, updateError);
      return;
    }
    var spec = uris1.pop();

    uris2.push(spec);
    var uri = ios.newURI("http://" + spec, null, null);
    var result = classifier.classify(uri, function(errorCode) {
        runInitialLookup();
      });
    // All of these classifications should succeed.
    do_check_eq(result, true);
    if (result) {
      doNextTest();
    }
  }

  // XXX bug 457790: dbservice.resetDatabase() doesn't have a way to
  // wait to make sure it has been applied.  Until this is added, we'll
  // just use a timeout.
  var t = new Timer(3000, runInitialLookup);
}

function run_test()
{
  runTests([
             // XXX: We need to run testUpdate first, because of a
             // race condition (bug 457790) calling dbservice.classify()
             // directly after dbservice.resetDatabase().
             testUpdate,
             testCleanHostKeys,
             testDirtyHostKeys,
             testResetFullCache,
  ]);
}

do_test_pending();
