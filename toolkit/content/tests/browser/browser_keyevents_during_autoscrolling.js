add_task(async function() {
  const kPrefName_AutoScroll = "general.autoScroll";
  Services.prefs.setBoolPref(kPrefName_AutoScroll, true);
  registerCleanupFunction(() =>
    Services.prefs.clearUserPref(kPrefName_AutoScroll)
  );

  const kNoKeyEvents = 0;
  const kKeyDownEvent = 1;
  const kKeyPressEvent = 2;
  const kKeyUpEvent = 4;
  const kAllKeyEvents = 7;

  var expectedKeyEvents;
  var dispatchedKeyEvents;
  var key;

  /**
   * Encapsulates EventUtils.sendChar().
   */
  function sendChar(aChar) {
    key = aChar;
    dispatchedKeyEvents = kNoKeyEvents;
    EventUtils.sendChar(key);
    is(
      dispatchedKeyEvents,
      expectedKeyEvents,
      "unexpected key events were dispatched or not dispatched: " + key
    );
  }

  /**
   * Encapsulates EventUtils.sendKey().
   */
  function sendKey(aKey) {
    key = aKey;
    dispatchedKeyEvents = kNoKeyEvents;
    EventUtils.sendKey(key);
    is(
      dispatchedKeyEvents,
      expectedKeyEvents,
      "unexpected key events were dispatched or not dispatched: " + key
    );
  }

  function onKey(aEvent) {
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

  await BrowserTestUtils.withNewTab(dataUri, async function(browser) {
    info("Loaded data URI in new tab");
    await SimpleTest.promiseFocus(browser);
    info("Focused selected browser");

    window.addEventListener("keydown", onKey);
    window.addEventListener("keypress", onKey);
    window.addEventListener("keyup", onKey);
    registerCleanupFunction(() => {
      window.removeEventListener("keydown", onKey);
      window.removeEventListener("keypress", onKey);
      window.removeEventListener("keyup", onKey);
    });

    // Test whether the key events are handled correctly under normal condition
    expectedKeyEvents = kAllKeyEvents;
    sendChar("A");

    // Start autoscrolling by middle button click on the page
    info("Creating popup shown promise");
    let shownPromise = BrowserTestUtils.waitForEvent(
      window,
      "popupshown",
      false,
      event => event.originalTarget.className == "autoscroller"
    );
    await BrowserTestUtils.synthesizeMouseAtPoint(
      10,
      10,
      { button: 1 },
      gBrowser.selectedBrowser
    );
    info("Waiting for autoscroll popup to show");
    await shownPromise;

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
  });
});
