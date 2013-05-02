function test()
{
  const kPrefName_AutoScroll = "general.autoScroll";
  Services.prefs.setBoolPref(kPrefName_AutoScroll, true);

  const expectScrollNone = 0;
  const expectScrollVert = 1;
  const expectScrollHori = 2;
  const expectScrollBoth = 3;

  var allTests = [
    {elem: 'a', expected: expectScrollNone},
    {elem: 'b', expected: expectScrollBoth},
    {elem: 'c', expected: expectScrollHori},
    {elem: 'd', expected: expectScrollVert},
    {elem: 'e', expected: expectScrollVert},
    {elem: 'f', expected: expectScrollNone},
    {elem: 'g', expected: expectScrollBoth},
    {elem: 'h', expected: expectScrollNone}
  ];

  var doc;

  function nextTest() {
    var test = allTests.shift();
    if (!test) {
      endTest();
      return;
    }

    var elem = doc.getElementById(test.elem);
    // Skip the first callback as it's the same callback that the browser
    // uses to kick off the scrolling.
    var skipFrames = 1;
    var checkScroll = function () {
      if (skipFrames--) {
        window.mozRequestAnimationFrame(checkScroll);
        return;
      }
      EventUtils.synthesizeKey("VK_ESCAPE", {}, gBrowser.contentWindow);
      var scrollVert = test.expected & expectScrollVert;
      ok((scrollVert && elem.scrollTop > 0) ||
         (!scrollVert && elem.scrollTop == 0),
         test.elem+' should'+(scrollVert ? '' : ' not')+' have scrolled vertically');
      var scrollHori = test.expected & expectScrollHori;
      ok((scrollHori && elem.scrollLeft > 0) ||
         (!scrollHori && elem.scrollLeft == 0),
         test.elem+' should'+(scrollHori ? '' : ' not')+' have scrolled horizontally');
      nextTest();
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

  waitForExplicitFinish();
  var dataUri = 'data:text/html,<body><style type="text/css">div { display: inline-block; }</style>\
    <div id="a" style="width: 100px; height: 100px; overflow: hidden;"><div style="width: 200px; height: 200px;"></div></div>\
    <div id="b" style="width: 100px; height: 100px; overflow: auto;"><div style="width: 200px; height: 200px;"></div></div>\
    <div id="c" style="width: 100px; height: 100px; overflow-x: auto; overflow-y: hidden;"><div style="width: 200px; height: 200px;"></div></div>\
    <div id="d" style="width: 100px; height: 100px; overflow-y: auto; overflow-x: hidden;"><div style="width: 200px; height: 200px;"></div></div>\
    <select id="e" style="width: 100px; height: 100px;" multiple="multiple"><option>aaaaaaaaaaaaaaaaaaaaaaaa</option><option>a</option><option>a</option>\
    <option>a</option><option>a</option><option>a</option><option>a</option><option>a</option><option>a</option><option>a</option>\
    <option>a</option><option>a</option><option>a</option><option>a</option><option>a</option><option>a</option><option>a</option></select>\
    <select id="f" style="width: 100px; height: 100px;"><option>a</option><option>aaaaaaaaaaaaaaaaaaaaaaaaaaaaaa</option><option>a</option>\
    <option>a</option><option>a</option><option>a</option><option>a</option><option>a</option><option>a</option><option>a</option>\
    <option>a</option><option>a</option><option>a</option><option>a</option><option>a</option><option>a</option><option>a</option></select>\
    <div id="g" style="width: 99px; height: 99px; padding: 10px; border: 10px solid black; margin: 10px; overflow: auto;"><div style="width: 100px; height: 100px; margin: 10px;"></div></div>\
    <div id="h" style="width: 100px; height: 100px; overflow: -moz-hidden-unscrollable;"><div style="width: 200px; height: 200px;"></div></div>\
    <iframe id="iframe" style="display: none;"></iframe>\
    </body>';
  gBrowser.selectedBrowser.addEventListener("pageshow", onLoad, false);
  gBrowser.loadURI(dataUri);

  function onLoad() {
    gBrowser.selectedBrowser.removeEventListener("pageshow", onLoad, false);
    waitForFocus(onFocus, content);
  }

  function onFocus() {
    doc = gBrowser.contentDocument;
    nextTest();
  }

  function endTest() {
    // restore the changed prefs
    if (Services.prefs.prefHasUserValue(kPrefName_AutoScroll))
      Services.prefs.clearUserPref(kPrefName_AutoScroll);

    // cleaning-up
    gBrowser.addTab("about:blank");
    gBrowser.removeCurrentTab();

    // waitForFocus() fixes a failure in the next test if the latter runs too soon.
    waitForFocus(finish);
  }
}
