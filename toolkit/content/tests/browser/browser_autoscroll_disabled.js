function test()
{
  const kPrefName_AutoScroll = "general.autoScroll";
  Services.prefs.setBoolPref(kPrefName_AutoScroll, false);

  var doc;

  function startLoad(dataUri) {
    gBrowser.selectedBrowser.addEventListener("pageshow", onLoad, false);
    gBrowser.loadURI(dataUri);
  }

  function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("pageshow", onLoad, false);
    waitForFocus(onFocus, content);
  }

  function onFocus() {
    doc = gBrowser.contentDocument;
    runChecks();
  }

  function endTest() {
    // restore the changed prefs
    if (Services.prefs.prefHasUserValue(kPrefName_AutoScroll))
      Services.prefs.clearUserPref(kPrefName_AutoScroll);

    // waitForFocus() fixes a failure in the next test if the latter runs too soon.
    waitForFocus(finish);
  }

  waitForExplicitFinish();

  let dataUri = 'data:text/html,<html><body id="i" style="overflow-y: scroll"><div style="height: 2000px"></div>\
      <iframe id="iframe" style="display: none;"></iframe>\
</body></html>';
  startLoad(dataUri);

  function runChecks() {
    var elem = doc.getElementById('i');
    // Skip the first callback as it's the same callback that the browser
    // uses to kick off the scrolling.
    var skipFrames = 1;
    var checkScroll = function () {
      if (skipFrames--) {
        window.mozRequestAnimationFrame(checkScroll);
        return;
      }
      ok(elem.scrollTop == 0, "element should not have scrolled vertically");
      ok(elem.scrollLeft == 0, "element should not have scrolled horizontally");

      endTest();
    };
    EventUtils.synthesizeMouse(elem, 50, 50, { button: 1 },
                               gBrowser.contentWindow);

    var iframe = gBrowser.contentDocument.getElementById("iframe");
    var e = iframe.contentDocument.createEvent("pagetransition");
    e.initPageTransitionEvent("pagehide", true, true, false);
    iframe.contentDocument.dispatchEvent(e);
    iframe.contentDocument.documentElement.dispatchEvent(e);

    EventUtils.synthesizeMouse(elem, 100, 100,
                               { type: "mousemove", clickCount: "0" },
                               gBrowser.contentWindow);
    /*
     * if scrolling didn’t work, we wouldn’t do any redraws and thus time out.
     * so request and force redraws to get the chance to check for scrolling at
     * all.
     */
    window.mozRequestAnimationFrame(checkScroll);
  }
}
