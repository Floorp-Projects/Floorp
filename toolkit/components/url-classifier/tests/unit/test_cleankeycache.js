//* -*- Mode: Javascript; tab-width: 8; indent-tabs-mode: nil; js-indent-level: 2 -*- *
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
      let principal = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                        .getService(Components.interfaces.nsIScriptSecurityManager)
                        .getNoAppCodebasePrincipal(uri);

      // Use the nsIURIClassifier interface (the
      // nsIUrlClassifierDBService will always queue a lookup,
      // nsIURIClassifier won't if the host key is known to be clean.
      var classifier = dbservice.QueryInterface(Ci.nsIURIClassifier);
      var result = classifier.classify(principal, function(errorCode) {
          var result2 = classifier.classify(principal, function() {
              do_throw("shouldn't get a callback");
            });
          // second call shouldn't result in a callback.
          do_check_eq(result2, false);
        do_throw("shouldn't get a callback");
        });

      // The first classifier call will not result in a callback
      do_check_eq(result, false);
      runNextTest();
    }, updateError);
}

// Make sure that an update properly clears the host key cache
function testUpdate() {
  var ios = Components.classes["@mozilla.org/network/io-service;1"].
    getService(Components.interfaces.nsIIOService);

  // Must put something in the PrefixSet
  var preUrls = [ "foo.com/b" ];
  var preUpdate = buildPhishingUpdate(
    [
      { "chunkNum" : 1,
        "urls" : preUrls
      }]);

  doStreamUpdate(preUpdate, function() {
    // First lookup won't happen...
    var uri = ios.newURI("http://foo.com/a", null, null);
    let principal = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                      .getService(Components.interfaces.nsIScriptSecurityManager)
                      .getNoAppCodebasePrincipal(uri);

    // Use the nsIURIClassifier interface (the
    // nsIUrlClassifierDBService will always queue a lookup,
    // nsIURIClassifier won't if the host key is known to be clean.
    var classifier = dbservice.QueryInterface(Ci.nsIURIClassifier);
    var result = classifier.classify(principal, function(errorCode) {
      // shouldn't arrive here
      do_check_eq(errorCode, Cr.NS_OK);
      do_throw("shouldn't get a callback");
    });
    do_check_eq(result, false);

    // Now add the url to the db...
    var addUrls = [ "foo.com/a" ];
    var update = buildPhishingUpdate(
      [
        { "chunkNum" : 2,
          "urls" : addUrls
        }]);
    doStreamUpdate(update, function() {
      var result2 = classifier.classify(principal, function(errorCode) {
        do_check_neq(errorCode, Cr.NS_OK);
        runNextTest();
      });
      // second call should result in a callback.
      do_check_eq(result2, true);
    }, updateError);
  }, updateError);
}

function testResetFullCache() {
  // Must put something in the PrefixSet
  var preUrls = [ "zaz.com/b" ];
  var preUpdate = buildPhishingUpdate(
    [
      { "chunkNum" : 1,
        "urls" : preUrls
      }]);

  doStreamUpdate(preUpdate, function() {
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
      let principal = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                        .getService(Components.interfaces.nsIScriptSecurityManager)
                        .getNoAppCodebasePrincipal(uri);

      var result = classifier.classify(principal, function(errorCode) {
      });
      runSecondLookup();
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
      let principal = Components.classes["@mozilla.org/scriptsecuritymanager;1"]
                        .getService(Components.interfaces.nsIScriptSecurityManager)
                        .getNoAppCodebasePrincipal(uri);
      var result = classifier.classify(principal, function(errorCode) {
      });
      runInitialLookup();
      // None of these will generate a callback
      do_check_eq(result, false);
      if (!result) {
        doNextTest();
      }
    }

    // XXX bug 457790: dbservice.resetDatabase() doesn't have a way to
    // wait to make sure it has been applied.  Until this is added, we'll
    // just use a timeout.
    var t = new Timer(3000, runInitialLookup);
  }, updateError);
}

function testBug475436() {
  var addUrls = [ "foo.com/a", "www.foo.com/" ];
  var update = buildPhishingUpdate(
        [
          { "chunkNum" : 1,
            "urls" : addUrls
          }]);

  var assertions = {
    "tableData" : "test-phish-simple;a:1",
    "urlsExist" : ["foo.com/a", "foo.com/a" ]
  };

  doUpdateTest([update], assertions, runNextTest, updateError);
}

function run_test()
{
  runTests([
             // XXX: We need to run testUpdate first, because of a
             // race condition (bug 457790) calling dbservice.classify()
             // directly after dbservice.resetDatabase().
             testUpdate,
             testCleanHostKeys,
             testResetFullCache,
             testBug475436
  ]);
}

do_test_pending();
