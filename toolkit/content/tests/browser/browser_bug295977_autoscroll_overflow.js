function test()
{
  const kPrefName_AutoScroll = "general.autoScroll";
  Services.prefs.setBoolPref(kPrefName_AutoScroll, true);

  const expectScrollNone = 0;
  const expectScrollVert = 1;
  const expectScrollHori = 2;
  const expectScrollBoth = 3;

  var allTests = [
    {dataUri: 'data:text/html,<html><head><meta charset="utf-8"></head><body><style type="text/css">div { display: inline-block; }</style>\
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
      <div id="g" style="width: 99px; height: 99px; border: 10px solid black; margin: 10px; overflow: auto;"><div style="width: 100px; height: 100px;"></div></div>\
      <div id="h" style="width: 100px; height: 100px; overflow: -moz-hidden-unscrollable;"><div style="width: 200px; height: 200px;"></div></div>\
      <iframe id="iframe" style="display: none;"></iframe>\
      </body></html>'},
    {elem: 'a', expected: expectScrollNone},
    {elem: 'b', expected: expectScrollBoth},
    {elem: 'c', expected: expectScrollHori},
    {elem: 'd', expected: expectScrollVert},
    {elem: 'e', expected: expectScrollVert},
    {elem: 'f', expected: expectScrollNone},
    {elem: 'g', expected: expectScrollBoth},
    {elem: 'h', expected: expectScrollNone},
    {dataUri: 'data:text/html,<html><head><meta charset="utf-8"></head><body id="i" style="overflow-y: scroll"><div style="height: 2000px"></div>\
      <iframe id="iframe" style="display: none;"></iframe>\
      </body></html>'},
    {elem: 'i', expected: expectScrollVert}, // bug 695121
    {dataUri: 'data:text/html,<html><head><meta charset="utf-8"></head><style>html, body { width: 100%; height: 100%; overflow-x: hidden; overflow-y: scroll; }</style>\
      <body id="j"><div style="height: 2000px"></div>\
      <iframe id="iframe" style="display: none;"></iframe>\
      </body></html>'},
    {elem: 'j', expected: expectScrollVert},  // bug 914251
    {dataUri: 'data:text/html,<html><head><meta charset="utf-8"></head><body>\
<div id="k" style="height: 150px;  width: 200px; overflow: scroll; border: 1px solid black;">\
<iframe style="height: 200px; width: 300px;"></iframe>\
</div>\
<div id="l" style="height: 150px;  width: 300px; overflow: scroll; border: 1px dashed black;">\
<iframe style="height: 200px; width: 200px;" src="data:text/html,<div style=\'border: 5px solid blue; height: 200%; width: 200%;\'></div>"></iframe>\
</div>\
<iframe id="m"></iframe>\
<div style="height: 200%; border: 5px dashed black;">filler to make document overflow: scroll;</div>\
</body></html>'},
    {elem: 'k', expected: expectScrollBoth},
    {elem: 'k', expected: expectScrollNone, testwindow: true},
    {elem: 'l', expected: expectScrollNone},
    {elem: 'm', expected: expectScrollVert, testwindow: true},
    {dataUri: 'data:text/html,<html><head><meta charset="utf-8"></head><body>\
<img width="100" height="100" alt="image map" usemap="%23planetmap">\
<map name="planetmap">\
  <area id="n" shape="rect" coords="0,0,100,100" href="javascript:void(null)">\
</map>\
<a href="javascript:void(null)" id="o" style="width: 100px; height: 100px; border: 1px solid black; display: inline-block; vertical-align: top;">link</a>\
<input id="p" style="width: 100px; height: 100px; vertical-align: top;">\
<textarea id="q" style="width: 100px; height: 100px; vertical-align: top;"></textarea>\
<div style="height: 200%; border: 1px solid black;"></div>\
</body></html>'},
    {elem: 'n', expected: expectScrollNone, testwindow: true},
    {elem: 'o', expected: expectScrollNone, testwindow: true},
    {elem: 'p', expected: expectScrollVert, testwindow: true, middlemousepastepref: false},
    {elem: 'q', expected: expectScrollVert, testwindow: true, middlemousepastepref: false},
    {dataUri: 'data:text/html,<html><head><meta charset="utf-8"></head><body>\
<input id="r" style="width: 100px; height: 100px; vertical-align: top;">\
<textarea id="s" style="width: 100px; height: 100px; vertical-align: top;"></textarea>\
<div style="height: 200%; border: 1px solid black;"></div>\
</body></html>'},
    {elem: 'r', expected: expectScrollNone, testwindow: true, middlemousepastepref: true},
    {elem: 's', expected: expectScrollNone, testwindow: true, middlemousepastepref: true}
  ];

  var doc;

  function nextTest() {
    var test = allTests.shift();
    if (!test) {
      endTest();
      return;
    }

    if (test.dataUri) {
      startLoad(test.dataUri);
      return;
    }

    var elem = doc.getElementById(test.elem);

    let firstTimestamp = undefined;
    function checkScroll(timestamp) {
      if (firstTimestamp === undefined) {
        firstTimestamp = timestamp;
      }

      // This value is calculated similarly to the value of the same name in
      // ClickEventHandler.autoscrollLoop, except here it's cumulative across
      // all frames after the first one instead of being based only on the
      // current frame.
      let timeCompensation = (timestamp - firstTimestamp) / 20;
      info("timestamp=" + timestamp + " firstTimestamp=" + firstTimestamp +
           " timeCompensation=" + timeCompensation);

      // Try to wait until enough time has passed to allow the scroll to happen.
      // autoscrollLoop incrementally scrolls during each animation frame, but
      // due to how its calculations work, when a frame is very close to the
      // previous frame, no scrolling may actually occur during that frame.
      // After 100ms's worth of frames, timeCompensation will be 1, making it
      // more likely that the accumulated scroll in autoscrollLoop will be >= 1,
      // although it also depends on acceleration, which here in this test
      // should be > 1 due to how it synthesizes mouse events below.
      if (timeCompensation < 5) {
        window.mozRequestAnimationFrame(checkScroll);
        return;
      }

      // Close the autoscroll popup by synthesizing Esc.
      EventUtils.synthesizeKey("VK_ESCAPE", {}, gBrowser.contentWindow);
      var scrollVert = test.expected & expectScrollVert;
      var scrollHori = test.expected & expectScrollHori;

      if (test.testwindow) {
        ok((scrollVert && gBrowser.contentWindow.scrollY > 0) ||
           (!scrollVert && gBrowser.contentWindow.scrollY == 0),
           'Window for '+test.elem+' should'+(scrollVert ? '' : ' not')+' have scrolled vertically');
        ok((scrollHori && gBrowser.contentWindow.scrollX > 0) ||
           (!scrollHori && gBrowser.contentWindow.scrollX == 0),
           'Window for '+test.elem+' should'+(scrollHori ? '' : ' not')+' have scrolled horizontally');
      } else {
        ok((scrollVert && elem.scrollTop > 0) ||
           (!scrollVert && elem.scrollTop == 0),
           test.elem+' should'+(scrollVert ? '' : ' not')+' have scrolled vertically');
        ok((scrollHori && elem.scrollLeft > 0) ||
           (!scrollHori && elem.scrollLeft == 0),
           test.elem+' should'+(scrollHori ? '' : ' not')+' have scrolled horizontally');
      }

      // Before continuing the test, we need to ensure that the IPC
      // message that stops autoscrolling has had time to arrive.
      executeSoon(nextTest);
    };

    if (test.middlemousepastepref == false || test.middlemousepastepref == true)
      Services.prefs.setBoolPref("middlemouse.paste", test.middlemousepastepref);

    EventUtils.synthesizeMouse(elem, 50, 50, { button: 1 },
                               gBrowser.contentWindow);

    // This ensures bug 605127 is fixed: pagehide in an unrelated document
    // should not cancel the autoscroll.
    var iframe = gBrowser.contentDocument.getElementById("iframe");

    if (iframe) {
      var e = new iframe.contentWindow.PageTransitionEvent("pagehide",
                                                           { bubbles: true,
                                                             cancelable: true,
                                                             persisted: false });
      iframe.contentDocument.dispatchEvent(e);
      iframe.contentDocument.documentElement.dispatchEvent(e);
    }

    EventUtils.synthesizeMouse(elem, 100, 100,
                               { type: "mousemove", clickCount: "0" },
                               gBrowser.contentWindow);

    if (Services.prefs.prefHasUserValue("middlemouse.paste"))
      Services.prefs.clearUserPref("middlemouse.paste");

    // Start checking for the scroll.
    window.mozRequestAnimationFrame(checkScroll);
  }

  waitForExplicitFinish();

  nextTest();

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
    nextTest();
  }

  function endTest() {
    registerCleanupFunction(function() {
      // restore the changed prefs
      if (Services.prefs.prefHasUserValue(kPrefName_AutoScroll))
        Services.prefs.clearUserPref(kPrefName_AutoScroll);
      if (Services.prefs.prefHasUserValue("middlemouse.paste"))
        Services.prefs.clearUserPref("middlemouse.paste");

      // remove 2 tabs that were opened by middle-click on links
      while (gBrowser.visibleTabs.length > 1) {
        gBrowser.removeTab(gBrowser.visibleTabs[gBrowser.visibleTabs.length - 1]);
      }
    });

    // waitForFocus() fixes a failure in the next test if the latter runs too soon.
    waitForFocus(finish);
  }
}
