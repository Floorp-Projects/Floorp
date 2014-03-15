function test()
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
  var root;

  /**
   * Encapsulates EventUtils.sendChar().
   */
  function sendChar(aChar)
  {
    key = aChar;
    dispatchedKeyEvents = kNoKeyEvents;
    EventUtils.sendChar(key, gBrowser.contentWindow);
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
    EventUtils.sendKey(key, gBrowser.contentWindow);
    is(dispatchedKeyEvents, expectedKeyEvents,
       "unexpected key events were dispatched or not dispatched: " + key);
  }

  function onKey(aEvent)
  {
    if (aEvent.target != root && aEvent.target != root.ownerDocument.body) {
      ok(false, "unknown target: " + aEvent.target.tagName);
      return;
    }

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

  waitForExplicitFinish();
  gBrowser.selectedBrowser.addEventListener("pageshow", onLoad, false);
  var dataUri = 'data:text/html,<body style="height:10000px;"></body>';
  gBrowser.loadURI(dataUri);

  function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("pageshow", onLoad, false);
    waitForFocus(onFocus, content);
  }

  function onFocus() {
    var doc = gBrowser.contentDocument;

    root = doc.documentElement;
    root.addEventListener("keydown", onKey, true);
    root.addEventListener("keypress", onKey, true);
    root.addEventListener("keyup", onKey, true);

    // Test whether the key events are handled correctly under normal condition
    expectedKeyEvents = kAllKeyEvents;
    sendChar("A");

    // Start autoscrolling by middle button lick on the page
    EventUtils.synthesizeMouse(root, 10, 10, { button: 1 },
                               gBrowser.contentWindow);

    // Before continuing the test, we need to ensure that the IPC
    // message that starts autoscrolling has had time to arrive.
    executeSoon(continueTest);
  }

  function continueTest() {
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

    root.removeEventListener("keydown", onKey, true);
    root.removeEventListener("keypress", onKey, true);
    root.removeEventListener("keyup", onKey, true);

    // restore the changed prefs
    if (Services.prefs.prefHasUserValue(kPrefName_AutoScroll))
      Services.prefs.clearUserPref(kPrefName_AutoScroll);

    // cleaning-up
    gBrowser.addTab().linkedBrowser.stop();
    gBrowser.removeCurrentTab();

    finish();
  }
}
