function getLastEventDetails(browser) {
  return SpecialPowers.spawn(browser, [], async function () {
    return content.document.getElementById("out").textContent;
  });
}

add_task(async function () {
  let onClickEvt =
    'document.getElementById("out").textContent = event.target.localName + "," + event.clientX + "," + event.clientY;';
  const url =
    "<body onclick='" +
    onClickEvt +
    "' style='margin: 0'>" +
    "<button id='one' style='margin: 0; margin-left: 16px; margin-top: 14px; width: 80px; height: 40px;'>Test</button>" +
    "<div onmousedown='event.preventDefault()' style='margin: 0; width: 80px; height: 60px;'>Other</div>" +
    "<span id='out'></span></body>";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html," + url
  );

  let browser = tab.linkedBrowser;
  await BrowserTestUtils.synthesizeMouseAtCenter("#one", {}, browser);
  let details = await getLastEventDetails(browser);

  is(details, "button,56,34", "synthesizeMouseAtCenter");

  await BrowserTestUtils.synthesizeMouse("#one", 4, 9, {}, browser);
  details = await getLastEventDetails(browser);
  is(details, "button,20,23", "synthesizeMouse");

  await BrowserTestUtils.synthesizeMouseAtPoint(15, 6, {}, browser);
  details = await getLastEventDetails(browser);
  is(details, "body,15,6", "synthesizeMouseAtPoint on body");

  await BrowserTestUtils.synthesizeMouseAtPoint(
    20,
    22,
    {},
    browser.browsingContext
  );
  details = await getLastEventDetails(browser);
  is(details, "button,20,22", "synthesizeMouseAtPoint on button");

  await BrowserTestUtils.synthesizeMouseAtCenter("body > div", {}, browser);
  details = await getLastEventDetails(browser);
  is(details, "div,40,84", "synthesizeMouseAtCenter with complex selector");

  let cancelled = await BrowserTestUtils.synthesizeMouseAtCenter(
    "body > div",
    { type: "mousedown" },
    browser
  );
  details = await getLastEventDetails(browser);
  is(
    details,
    "div,40,84",
    "synthesizeMouseAtCenter mousedown with complex selector"
  );
  ok(
    cancelled,
    "synthesizeMouseAtCenter mousedown with complex selector not cancelled"
  );

  cancelled = await BrowserTestUtils.synthesizeMouseAtCenter(
    "body > div",
    { type: "mouseup" },
    browser
  );
  details = await getLastEventDetails(browser);
  is(
    details,
    "div,40,84",
    "synthesizeMouseAtCenter mouseup with complex selector"
  );
  ok(
    !cancelled,
    "synthesizeMouseAtCenter mouseup with complex selector cancelled"
  );

  gBrowser.removeTab(tab);
});

add_task(async function testSynthesizeMouseAtPointsButtons() {
  let onMouseEvt =
    'document.getElementById("mouselog").textContent += "/" + [event.type,event.clientX,event.clientY,event.button,event.buttons].join(",");';

  let getLastMouseEventDetails = browser => {
    return SpecialPowers.spawn(browser, [], async () => {
      let log = content.document.getElementById("mouselog").textContent;
      content.document.getElementById("mouselog").textContent = "";
      return log;
    });
  };

  const url =
    "<body" +
    "' onmousedown='" +
    onMouseEvt +
    "' onmousemove='" +
    onMouseEvt +
    "' onmouseup='" +
    onMouseEvt +
    "' style='margin: 0'>" +
    "<div style='margin: 0; width: 80px; height: 60px;'>Mouse area</div>" +
    "<span id='mouselog'></span>" +
    "</body>";
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html," + url
  );

  let browser = tab.linkedBrowser;
  let details;

  await BrowserTestUtils.synthesizeMouseAtPoint(
    21,
    22,
    {
      type: "mousemove",
    },
    browser.browsingContext
  );
  details = await getLastMouseEventDetails(browser);
  is(details, "/mousemove,21,22,0,0", "synthesizeMouseAtPoint mousemove");

  await BrowserTestUtils.synthesizeMouseAtPoint(
    22,
    23,
    {},
    browser.browsingContext
  );
  details = await getLastMouseEventDetails(browser);
  is(
    details,
    "/mousedown,22,23,0,1/mouseup,22,23,0,0",
    "synthesizeMouseAtPoint default action includes buttons on mousedown only"
  );

  await BrowserTestUtils.synthesizeMouseAtPoint(
    20,
    22,
    {
      type: "mousedown",
    },
    browser.browsingContext
  );
  details = await getLastMouseEventDetails(browser);
  is(
    details,
    "/mousedown,20,22,0,1",
    "synthesizeMouseAtPoint mousedown includes buttons"
  );

  await BrowserTestUtils.synthesizeMouseAtPoint(
    21,
    20,
    {
      type: "mouseup",
    },
    browser.browsingContext
  );
  details = await getLastMouseEventDetails(browser);
  is(details, "/mouseup,21,20,0,0", "synthesizeMouseAtPoint mouseup");

  await BrowserTestUtils.synthesizeMouseAtPoint(
    20,
    22,
    {
      type: "mousedown",
      button: 2,
    },
    browser.browsingContext
  );
  details = await getLastMouseEventDetails(browser);
  is(
    details,
    "/mousedown,20,22,2,2",

    "synthesizeMouseAtPoint mousedown respects specified button 2"
  );

  await BrowserTestUtils.synthesizeMouseAtPoint(
    21,
    20,
    {
      type: "mouseup",
      button: 2,
    },
    browser.browsingContext
  );
  details = await getLastMouseEventDetails(browser);
  is(
    details,
    "/mouseup,21,20,2,0",
    "synthesizeMouseAtPoint mouseup with button 2"
  );

  gBrowser.removeTab(tab);
});

add_task(async function mouse_in_iframe() {
  let onClickEvt = "document.body.lastChild.textContent = event.target.id;";
  const url = `<iframe style='margin: 30px;' src='data:text/html,<body onclick="${onClickEvt}">
     <p><button>One</button></p><p><button id="two">Two</button></p><p id="out"></p></body>'></iframe>
     <iframe style='margin: 10px;' src='data:text/html,<body onclick="${onClickEvt}">
     <p><button>Three</button></p><p><button id="four">Four</button></p><p id="out"></p></body>'></iframe>`;
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "data:text/html," + url
  );

  let browser = tab.linkedBrowser;

  await BrowserTestUtils.synthesizeMouse(
    "#two",
    5,
    10,
    {},
    browser.browsingContext.children[0]
  );

  let details = await getLastEventDetails(browser.browsingContext.children[0]);
  is(details, "two", "synthesizeMouse");

  await BrowserTestUtils.synthesizeMouse(
    "#four",
    5,
    10,
    {},
    browser.browsingContext.children[1]
  );
  details = await getLastEventDetails(browser.browsingContext.children[1]);
  is(details, "four", "synthesizeMouse");

  gBrowser.removeTab(tab);
});

add_task(async function () {
  await BrowserTestUtils.registerAboutPage(
    registerCleanupFunction,
    "about-pages-are-cool",
    getRootDirectory(gTestPath) + "dummy.html",
    0
  );
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "about:about-pages-are-cool",
    true
  );
  ok(tab, "Successfully created an about: page and loaded it.");
  BrowserTestUtils.removeTab(tab);
  try {
    await BrowserTestUtils.unregisterAboutPage("about-pages-are-cool");
    ok(true, "Successfully unregistered the about page.");
  } catch (ex) {
    ok(false, "Should not throw unregistering a known about: page");
  }
  await BrowserTestUtils.unregisterAboutPage("random-other-about-page").then(
    () => {
      ok(
        false,
        "Should not have succeeded unregistering an unknown about: page."
      );
    },
    () => {
      ok(
        true,
        "Should have returned a rejected promise trying to unregister an unknown about page"
      );
    }
  );
});

add_task(async function testWaitForEvent() {
  // A promise returned by BrowserTestUtils.waitForEvent should not be resolved
  // in the same event tick as the event listener is called.
  let eventListenerCalled = false;
  let waitForEventResolved = false;
  // Use capturing phase to make sure the event listener added by
  // BrowserTestUtils.waitForEvent is called before the normal event listener
  // below.
  let eventPromise = BrowserTestUtils.waitForEvent(
    gBrowser,
    "dummyevent",
    true
  );
  eventPromise.then(() => {
    waitForEventResolved = true;
  });
  // Add normal event listener that is called after the event listener added by
  // BrowserTestUtils.waitForEvent.
  gBrowser.addEventListener(
    "dummyevent",
    () => {
      eventListenerCalled = true;
      is(
        waitForEventResolved,
        false,
        "BrowserTestUtils.waitForEvent promise resolution handler shouldn't be called at this point."
      );
    },
    { once: true }
  );

  var event = new CustomEvent("dummyevent");
  gBrowser.dispatchEvent(event);

  await eventPromise;

  is(eventListenerCalled, true, "dummyevent listener should be called");
});
