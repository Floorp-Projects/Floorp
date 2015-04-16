function getLastEventDetails(browser)
{
  return ContentTask.spawn(browser, {}, function* () {
    return content.document.getElementById('out').textContent;
  });
}

add_task(function* () {
  let onClickEvt = 'document.getElementById("out").textContent = event.target.localName + "," + event.clientX + "," + event.clientY;'
  const url = "<body onclick='" + onClickEvt + "' style='margin: 0'>" +
              "<button id='one' style='margin: 0; margin-left: 16px; margin-top: 14px; width: 30px; height: 40px;'>Test</button>" +
              "<div onmousedown='event.preventDefault()' style='margin: 0; width: 80px; height: 60px;'>Other</div>" +
              "<span id='out'></span></body>";
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, "data:text/html," + url);

  let browser = tab.linkedBrowser;
  yield BrowserTestUtils.synthesizeMouseAtCenter("#one", {}, browser);
  let details = yield getLastEventDetails(browser);

  is(details, "button,31,34", "synthesizeMouseAtCenter");

  yield BrowserTestUtils.synthesizeMouse("#one", 4, 9, {}, browser);
  details = yield getLastEventDetails(browser);
  is(details, "button,20,23", "synthesizeMouse");

  yield BrowserTestUtils.synthesizeMouseAtPoint(15, 6, {}, browser);
  details = yield getLastEventDetails(browser);
  is(details, "body,15,6", "synthesizeMouseAtPoint on body");

  yield BrowserTestUtils.synthesizeMouseAtPoint(20, 22, {}, browser);
  details = yield getLastEventDetails(browser);
  is(details, "button,20,22", "synthesizeMouseAtPoint on button");

  yield BrowserTestUtils.synthesizeMouseAtCenter("body > div", {}, browser);
  details = yield getLastEventDetails(browser);
  is(details, "div,40,84", "synthesizeMouseAtCenter with complex selector");

  let cancelled = yield BrowserTestUtils.synthesizeMouseAtCenter("body > div", { type: "mousedown" }, browser);
  details = yield getLastEventDetails(browser);
  is(details, "div,40,84", "synthesizeMouseAtCenter mousedown with complex selector");
  ok(cancelled, "synthesizeMouseAtCenter mousedown with complex selector not cancelled");

  cancelled = yield BrowserTestUtils.synthesizeMouseAtCenter("body > div", { type: "mouseup" }, browser);
  details = yield getLastEventDetails(browser);
  is(details, "div,40,84", "synthesizeMouseAtCenter mouseup with complex selector");
  ok(!cancelled, "synthesizeMouseAtCenter mouseup with complex selector cancelled");

  let one = browser.contentDocument.getElementById("one");
  yield BrowserTestUtils.synthesizeMouseAtCenter(one, {}, browser);
  details = yield getLastEventDetails(browser);
  is(details, "button,31,34", "synthesizeMouseAtCenter with wrapper");

  gBrowser.removeTab(tab);
});
