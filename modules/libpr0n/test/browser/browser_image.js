waitForExplicitFinish();

// A hold on the current timer, so it doens't get GCed out from
// under us
var gTimer;

// Browsing to a new URL - pushing us into the bfcache - should cause
// animations to stop, and resume when we return
function testBFCache() {
  function theTest() {
    var abort = false;
    var chances, gImage, gFrames;
    gBrowser.selectedTab = gBrowser.addTab(TESTROOT + "image.html");
    gBrowser.selectedBrowser.addEventListener("pageshow", function () {
      gBrowser.selectedBrowser.removeEventListener("pageshow", arguments.callee, true);
      var window = gBrowser.contentWindow;
      // If false, we are in an optimized build, and we abort this and
      // all further tests
      if (!actOnMozImage(window.document, "img1", function(image) {
        gImage = image;
        gFrames = gImage.framesNotified;
      })) {
        gBrowser.removeCurrentTab();
        abort = true;
      }
      goer.next();
    }, true);
    yield;
    if (abort) {
      finish();
      yield; // optimized build
    }

    // Let animation run for a bit
    chances = 120;
    do {
      gTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      gTimer.initWithCallback(function() {
        if (gImage.framesNotified >= 20) {
          goer.send(true);
        } else {
          chances--;
          goer.send(chances == 0); // maybe if we wait a bit, it will happen
        }
      }, 500, Ci.nsITimer.TYPE_ONE_SHOT);
    } while (!(yield));
    is(chances > 0, true, "Must have animated a few frames so far");

    // Browse elsewhere; push our animating page into the bfcache
    gBrowser.loadURI("about:blank");

    // Wait a bit for page to fully load, then wait a while and
    // see that no animation occurs.
    gTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    gTimer.initWithCallback(function() {
      gFrames = gImage.framesNotified;
      gTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      gTimer.initWithCallback(function() {
        // Might have a few stray frames, until other page totally loads
        is(gImage.framesNotified == gFrames, true, "Must have not animated in bfcache!");
        goer.next();
      }, 4000, Ci.nsITimer.TYPE_ONE_SHOT); // 4 seconds - expect 40 frames
    }, 0, Ci.nsITimer.TYPE_ONE_SHOT); // delay of 0 - wait for next event loop
    yield;

    // Go back
    gBrowser.goBack();

    chances = 120;
    do {
      gTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      gTimer.initWithCallback(function() {
        if (gImage.framesNotified - gFrames >= 20) {
          goer.send(true);
        } else {
          chances--;
          goer.send(chances == 0); // maybe if we wait a bit, it will happen
        }
      }, 500, Ci.nsITimer.TYPE_ONE_SHOT);
    } while (!(yield));
    is(chances > 0, true, "Must have animated once out of bfcache!");

    // Finally, check that the css background image has essentially the same
    // # of frames, implying that it animated at the same times as the regular
    // image. We can easily retrieve regular images through their HTML image
    // elements, which is what we did before. For the background image, we
    // create a regular image now, and read the current frame count.
    var doc = gBrowser.selectedBrowser.contentWindow.document;
    var div = doc.getElementById("background_div");
    div.innerHTML += '<img src="animated2.gif" id="img3">';
    actOnMozImage(doc, "img3", function(image) {
      is(Math.abs(image.framesNotified - gImage.framesNotified)/gImage.framesNotified < 0.5, true,
         "Must have also animated the background image, and essentially the same # of frames");
    });

    gBrowser.removeCurrentTab();

    nextTest();
  }

  var goer = theTest();
  goer.next();
}

// Check that imgContainers are shared on the same page and
// between tabs
function testSharedContainers() {
  function theTest() {
    var gImages = [];
    var gFrames;

    gBrowser.selectedTab = gBrowser.addTab(TESTROOT + "image.html");
    gBrowser.selectedBrowser.addEventListener("pageshow", function () {
      gBrowser.selectedBrowser.removeEventListener("pageshow", arguments.callee, true);
      actOnMozImage(gBrowser.contentDocument, "img1", function(image) {
        gImages[0] = image;
        gFrames = image.framesNotified; // May in theory have frames from last test
                                        // in this counter - so subtract them out
      });
      goer.next();
    }, true);
    yield;

    // Load next tab somewhat later
    gTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
    gTimer.initWithCallback(function() {
      goer.next();
    }, 1500, Ci.nsITimer.TYPE_ONE_SHOT);
    yield;

    gBrowser.selectedTab = gBrowser.addTab(TESTROOT + "imageX2.html");
    gBrowser.selectedBrowser.addEventListener("pageshow", function () {
      gBrowser.selectedBrowser.removeEventListener("pageshow", arguments.callee, true);
      [1,2].forEach(function(i) {
        actOnMozImage(gBrowser.contentDocument, "img"+i, function(image) {
          gImages[i] = image;
        });
      });
      goer.next();
    }, true);
    yield;

    chances = 120;
    do {
      gTimer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
      gTimer.initWithCallback(function() {
        if (gImages[0].framesNotified - gFrames >= 10) {
          goer.send(true);
        } else {
          chances--;
          goer.send(chances == 0); // maybe if we wait a bit, it will happen
        }
      }, 500, Ci.nsITimer.TYPE_ONE_SHOT);
    } while (!(yield));
    is(chances > 0, true, "Must have been animating while showing several images");

    // Check they all have the same frame counts
    var theFrames = null;
    [0,1,2].forEach(function(i) {
      var frames = gImages[i].framesNotified;
      if (theFrames == null) {
        theFrames = frames;
      } else {
        is(theFrames, frames, "Sharing the same imgContainer means *exactly* the same frame counts!");
      }
    });

    gBrowser.removeCurrentTab();
    gBrowser.removeCurrentTab();

    nextTest();
  }

  var goer = theTest();
  goer.next();
}

var tests = [testBFCache, testSharedContainers];

function nextTest() {
  if (tests.length == 0) {
    finish();
    return;
  }
  tests.shift()();
}

function test() {
  nextTest();
}

