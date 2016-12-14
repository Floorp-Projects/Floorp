add_task(function * ()
{
  const kPrefName_AutoScroll = "general.autoScroll";
  Services.prefs.setBoolPref(kPrefName_AutoScroll, true);

  const kNoKeyEvents   = 0;
  const kKeyDownEvent  = 1;
  const kKeyPressEvent = 2;
  const kKeyUpEvent    = 4;
  const kAllKeyEvents  = 7;

  var expectedKeyEvents;
  var dispatchedKeyEvents;
  var key;

  /**
   * Encapsulates EventUtils.sendChar().
   */
  function sendChar(aChar)
  {
    key = aChar;
    dispatchedKeyEvents = kNoKeyEvents;
    EventUtils.sendChar(key);
    is(dispatchedKeyEvents, expectedKeyEvents,
       "unexpected key events were dispatched or not dispatched: " + key);
  }

  /**
   * Encapsulates EventUtils.sendKey().
   */
  function sendKey(aKey)
  {
    key = aKey;
    dispatchedKeyEvents = kNoKeyEvents;
    EventUtils.sendKey(key);
    is(dispatchedKeyEvents, expectedKeyEvents,
       "unexpected key events were dispatched or not dispatched: " + key);
  }

  function onKey(aEvent)
  {
//    if (aEvent.target != root && aEvent.target != root.ownerDocument.body) {
//      ok(false, "unknown target: " + aEvent.target.tagName);
//      return;
//    }

    var keyFlag;
    switch (aEvent.type) {
      case "keydown":
        keyFlag = kKeyDownEvent;
        break;
      case "keypress":
        keyFlag = kKeyPressEvent;
        break;
      case "keyup":
        keyFlag = kKeyUpEvent;
        break;
      default:
        ok(false, "Unknown events: " + aEvent.type);
        return;
    }
    dispatchedKeyEvents |= keyFlag;
    is(keyFlag, expectedKeyEvents & keyFlag, aEvent.type + " fired: " + key);
  }

  var dataUri = 'data:text/html,<body style="height:10000px;"></body>';

  let loadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.loadURI(dataUri);
  yield loadedPromise;

  yield SimpleTest.promiseFocus(gBrowser.selectedBrowser);

  window.addEventListener("keydown", onKey, false);
  window.addEventListener("keypress", onKey, false);
  window.addEventListener("keyup", onKey, false);

  // Test whether the key events are handled correctly under normal condition
  expectedKeyEvents = kAllKeyEvents;
  sendChar("A");

  // Start autoscrolling by middle button click on the page
  let shownPromise = BrowserTestUtils.waitForEvent(window, "popupshown", false,
                       event => event.originalTarget.className == "autoscroller");
  yield BrowserTestUtils.synthesizeMouseAtPoint(10, 10, { button: 1 },
                                                gBrowser.selectedBrowser);
  yield shownPromise;

  // Most key events should be eaten by the browser.
  expectedKeyEvents = kNoKeyEvents;
  sendChar("A");
  sendKey("DOWN");
  sendKey("RETURN");
  sendKey("RETURN");
  sendKey("HOME");
  sendKey("END");
  sendKey("TAB");
  sendKey("RETURN");

  // Finish autoscrolling by ESC key.  Note that only keydown and keypress
  // events are eaten because keyup event is fired *after* the autoscrolling
  // is finished.
  expectedKeyEvents = kKeyUpEvent;
  sendKey("ESCAPE");

  // Test whether the key events are handled correctly under normal condition
  expectedKeyEvents = kAllKeyEvents;
  sendChar("A");

  window.removeEventListener("keydown", onKey, false);
  window.removeEventListener("keypress", onKey, false);
  window.removeEventListener("keyup", onKey, false);

  // restore the changed prefs
  if (Services.prefs.prefHasUserValue(kPrefName_AutoScroll))
    Services.prefs.clearUserPref(kPrefName_AutoScroll);

  finish();
});
