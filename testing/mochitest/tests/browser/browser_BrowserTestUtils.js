function getLastEventDetails(browser)
{
  return ContentTask.spawn(browser, {}, async function() {
    return content.document.getElementById('out').textContent;
  });
}

add_task(async function() {
  let onClickEvt = 'document.getElementById("out").textContent = event.target.localName + "," + event.clientX + "," + event.clientY;'
  const url = "<body onclick='" + onClickEvt + "' style='margin: 0'>" +
              "<button id='one' style='margin: 0; margin-left: 16px; margin-top: 14px; width: 30px; height: 40px;'>Test</button>" +
              "<div onmousedown='event.preventDefault()' style='margin: 0; width: 80px; height: 60px;'>Other</div>" +
              "<span id='out'></span></body>";
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, "data:text/html," + url);

  let browser = tab.linkedBrowser;
  await BrowserTestUtils.synthesizeMouseAtCenter("#one", {}, browser);
  let details = await getLastEventDetails(browser);

  is(details, "button,31,34", "synthesizeMouseAtCenter");

  await BrowserTestUtils.synthesizeMouse("#one", 4, 9, {}, browser);
  details = await getLastEventDetails(browser);
  is(details, "button,20,23", "synthesizeMouse");

  await BrowserTestUtils.synthesizeMouseAtPoint(15, 6, {}, browser);
  details = await getLastEventDetails(browser);
  is(details, "body,15,6", "synthesizeMouseAtPoint on body");

  await BrowserTestUtils.synthesizeMouseAtPoint(20, 22, {}, browser);
  details = await getLastEventDetails(browser);
  is(details, "button,20,22", "synthesizeMouseAtPoint on button");

  await BrowserTestUtils.synthesizeMouseAtCenter("body > div", {}, browser);
  details = await getLastEventDetails(browser);
  is(details, "div,40,84", "synthesizeMouseAtCenter with complex selector");

  let cancelled = await BrowserTestUtils.synthesizeMouseAtCenter("body > div", { type: "mousedown" }, browser);
  details = await getLastEventDetails(browser);
  is(details, "div,40,84", "synthesizeMouseAtCenter mousedown with complex selector");
  ok(cancelled, "synthesizeMouseAtCenter mousedown with complex selector not cancelled");

  cancelled = await BrowserTestUtils.synthesizeMouseAtCenter("body > div", { type: "mouseup" }, browser);
  details = await getLastEventDetails(browser);
  is(details, "div,40,84", "synthesizeMouseAtCenter mouseup with complex selector");
  ok(!cancelled, "synthesizeMouseAtCenter mouseup with complex selector cancelled");

  gBrowser.removeTab(tab);
});

add_task(async function() {
  await BrowserTestUtils.registerAboutPage(
    registerCleanupFunction, "about-pages-are-cool",
    getRootDirectory(gTestPath) + "dummy.html", 0);
  let tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser, "about:about-pages-are-cool", true);
  ok(tab, "Successfully created an about: page and loaded it.");
  await BrowserTestUtils.removeTab(tab);
  try {
    await BrowserTestUtils.unregisterAboutPage("about-pages-are-cool");
    ok(true, "Successfully unregistered the about page.");
  } catch (ex) {
    ok(false, "Should not throw unregistering a known about: page");
  }
  await BrowserTestUtils.unregisterAboutPage("random-other-about-page").then(() => {
    ok(false, "Should not have succeeded unregistering an unknown about: page.");
  }, () => {
    ok(true, "Should have returned a rejected promise trying to unregister an unknown about page");
  });
});
