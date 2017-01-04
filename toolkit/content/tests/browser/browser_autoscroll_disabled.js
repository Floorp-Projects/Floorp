add_task(function* () {
  const kPrefName_AutoScroll = "general.autoScroll";
  Services.prefs.setBoolPref(kPrefName_AutoScroll, false);

  let dataUri = 'data:text/html,<html><body id="i" style="overflow-y: scroll"><div style="height: 2000px"></div>\
      <iframe id="iframe" style="display: none;"></iframe>\
</body></html>';

  let loadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  gBrowser.loadURI(dataUri);
  yield loadedPromise;

  yield BrowserTestUtils.synthesizeMouse("#i", 50, 50, { button: 1 },
                                         gBrowser.selectedBrowser);

  yield ContentTask.spawn(gBrowser.selectedBrowser, { }, function* () {
    var iframe = content.document.getElementById("iframe");

    if (iframe) {
      var e = new iframe.contentWindow.PageTransitionEvent("pagehide",
                                                           { bubbles: true,
                                                             cancelable: true,
                                                             persisted: false });
      iframe.contentDocument.dispatchEvent(e);
      iframe.contentDocument.documentElement.dispatchEvent(e);
    }
  });

  yield BrowserTestUtils.synthesizeMouse("#i", 100, 100,
                                         { type: "mousemove", clickCount: "0" },
                                         gBrowser.selectedBrowser);

  // If scrolling didn't work, we wouldn't do any redraws and thus time out, so
  // request and force redraws to get the chance to check for scrolling at all.
  yield new Promise(resolve => window.requestAnimationFrame(resolve));

  let msg = yield ContentTask.spawn(gBrowser.selectedBrowser, { }, function* () {
    // Skip the first animation frame callback as it's the same callback that
    // the browser uses to kick off the scrolling.
    return new Promise(resolve => {
      function checkScroll() {
        let msg = "";
        let elem = content.document.getElementById('i');
        if (elem.scrollTop != 0) {
          msg += "element should not have scrolled vertically";
        }
        if (elem.scrollLeft != 0) {
          msg += "element should not have scrolled horizontally";
        }

        resolve(msg);
      }

      content.requestAnimationFrame(checkScroll);
    });
  });

  ok(!msg, "element scroll " + msg);

  // restore the changed prefs
  if (Services.prefs.prefHasUserValue(kPrefName_AutoScroll))
    Services.prefs.clearUserPref(kPrefName_AutoScroll);

  // wait for focus to fix a failure in the next test if the latter runs too soon.
  yield SimpleTest.promiseFocus();
});
