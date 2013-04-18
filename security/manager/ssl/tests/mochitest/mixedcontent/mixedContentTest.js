/**
 * Helper script for mixed content testing. It opens a new top-level window
 * from a secure origin and '?runtest' query. That tells us to run the test
 * body, function runTest(). Then we wait for call of finish(). On its first
 * call it loads helper page 'backward.html' that immediately navigates
 * back to the test secure test. This checks the bfcache. We got second call
 * to onload and this time we call afterNavigationTest() function to let the
 * test check security state after re-navigation back. Then we again wait for
 * finish() call, that this time finishes completelly the test.
 */

// Tells the framework if to load the test in an insecure page (http://)
var loadAsInsecure = false;
// Set true to bypass the navigation forward/back test
var bypassNavigationTest = false;
// Set true to do forward/back navigation over an http:// page, test state leaks
var navigateToInsecure = false;
// Open the test in two separate windows, test requests sharing among windows
var openTwoWindows = false;
// Override the name of the test page to load, useful e.g. to prevent load
// of images or other content before the test starts; this is actually
// a 'redirect' to a different test page.
var testPage = "";
// Assign a function to this variable to have a clean up at the end
var testCleanUp = null;
// Contains mixed active content that needs to load to run the test
var hasMixedActiveContent = false;


// Internal variables
var _windowCount = 0;

window.onload = function onLoad()
{
  if (location.search == "?runtest")
  {
    try
    {
      if (history.length == 1)
        runTest();
      else
        afterNavigationTest();
    }
    catch (ex)
    {
      ok(false, "Exception thrown during test: " + ex);
      finish();
    }
  }
  else
  {
    window.addEventListener("message", onMessageReceived, false);

    var secureTestLocation;
    if (loadAsInsecure)
      secureTestLocation = "http://example.com";
    else
      secureTestLocation = "https://example.com";
    secureTestLocation += location.pathname
    if (testPage != "")
    {
      array = secureTestLocation.split("/");
      array.pop();
      array.push(testPage);
      secureTestLocation = array.join("/");
    }
    secureTestLocation += "?runtest";

    if (hasMixedActiveContent)
    {
      SpecialPowers.pushPrefEnv(
        {"set": [["security.mixed_content.block_active_content", false]]},
        null);
    }
    if (openTwoWindows)
    {
      _windowCount = 2;
      window.open(secureTestLocation, "_new1", "");
      window.open(secureTestLocation, "_new2", "");
    }
    else
    {
      _windowCount = 1;
      window.open(secureTestLocation);
    }
  }
}

function onMessageReceived(event)
{
  switch (event.data)
  {
    // Indication of all test parts finish (from any of the frames)
    case "done":
      if (--_windowCount == 0)
      {
        if (testCleanUp)
          testCleanUp();
        if (hasMixedActiveContent) {
          SpecialPowers.popPrefEnv(null);
        }

        SimpleTest.finish();
      }
      break;

    // Any other message indicates error or succes message of a test
    default:
      var failureRegExp = new RegExp("^FAILURE");
      var todoRegExp = new RegExp("^TODO");
      if (event.data.match(todoRegExp))
        SimpleTest.todo(false, event.data);
      else
        SimpleTest.ok(!event.data.match(failureRegExp), event.data);
      break;
  }
}

function postMsg(message)
{
  opener.postMessage(message, "http://mochi.test:8888");
}

function finish()
{
  if (history.length == 1 && !bypassNavigationTest)
  {
    window.setTimeout(function()
    {
      window.location.assign(navigateToInsecure ?
        "http://example.com/tests/security/manager/ssl/tests/mochitest/mixedcontent/backward.html" :
        "https://example.com/tests/security/manager/ssl/tests/mochitest/mixedcontent/backward.html");
    }, 0);
  }
  else
  {
    postMsg("done");
    window.close();
  }
}

function ok(a, message)
{
  if (!a)
    postMsg("FAILURE: " + message);
  else
    postMsg(message);
}

function is(a, b, message)
{
  if (a != b)
    postMsg("FAILURE: " + message + ", expected "+b+" got "+a);
  else
    postMsg(message + ", expected "+b+" got "+a);
}

function todo(a, message)
{
  if (a)
    postMsg("FAILURE: TODO works? " + message);
  else
    postMsg("TODO: " + message);
}

function isSecurityState(expectedState, message, test)
{
  if (!test)
    test = ok;

  // Quit nasty but working :)
  var ui = SpecialPowers.wrap(window)
    .QueryInterface(SpecialPowers.Ci.nsIInterfaceRequestor)
    .getInterface(SpecialPowers.Ci.nsIWebNavigation)
    .QueryInterface(SpecialPowers.Ci.nsIDocShell)
    .securityUI;

  var isInsecure = !ui ||
    (ui.state & SpecialPowers.Ci.nsIWebProgressListener.STATE_IS_INSECURE);
  var isBroken = ui &&
    (ui.state & SpecialPowers.Ci.nsIWebProgressListener.STATE_IS_BROKEN);
  var isEV = ui &&
    (ui.state & SpecialPowers.Ci.nsIWebProgressListener.STATE_IDENTITY_EV_TOPLEVEL);

  var gotState;
  if (isInsecure)
    gotState = "insecure";
  else if (isBroken)
    gotState = "broken";
  else if (isEV)
    gotState = "EV";
  else
    gotState = "secure";

  test(gotState == expectedState, (message || "") + ", " + "expected " + expectedState + " got " + gotState);

  switch (expectedState)
  {
    case "insecure":
      test(isInsecure && !isBroken && !isEV, "for 'insecure' excpected flags [1,0,0], " + (message || ""));
      break;
    case "broken":
      test(ui && !isInsecure && isBroken && !isEV, "for 'broken' expected  flags [0,1,0], " + (message || ""));
      break;
    case "secure":
      test(ui && !isInsecure && !isBroken && !isEV, "for 'secure' expected flags [0,0,0], " + (message || ""));
      break;
    case "EV":
      test(ui && !isInsecure && !isBroken && isEV, "for 'EV' expected flags [0,0,1], " + (message || ""));
      break;
    default:
      throw "Invalid isSecurityState state";
  }
}

function waitForSecurityState(expectedState, callback)
{
  var roundsLeft = 200; // Wait for 20 seconds (=200*100ms)
  var interval =
  window.setInterval(function() {
    isSecurityState(expectedState, "", function(isok) {if (isok) {roundsLeft = 0;}});
    if (!roundsLeft--) {
      window.clearInterval(interval);
      callback();
    }
  }, 100);
}
