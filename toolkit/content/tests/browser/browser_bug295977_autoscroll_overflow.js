requestLongerTimeout(2);
add_task(function* ()
{
  function pushPref(name, value) {
    return new Promise(resolve => SpecialPowers.pushPrefEnv({"set": [[name, value]]}, resolve));
  }

  yield pushPref("general.autoScroll", true);

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

  for (let test of allTests) {
    if (test.dataUri) {
      let loadedPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
      gBrowser.loadURI(test.dataUri);
      yield loadedPromise;
      continue;
     }

    let prefsChanged = (test.middlemousepastepref == false || test.middlemousepastepref == true);
    if (prefsChanged) {
      yield pushPref("middlemouse.paste", test.middlemousepastepref);
    }

    yield BrowserTestUtils.synthesizeMouse("#" + test.elem, 50, 80, { button: 1 },
                                           gBrowser.selectedBrowser);
 
    // This ensures bug 605127 is fixed: pagehide in an unrelated document
    // should not cancel the autoscroll.
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

    is(document.activeElement, gBrowser.selectedBrowser, "Browser still focused after autoscroll started");

    yield BrowserTestUtils.synthesizeMouse("#" + test.elem, 100, 100,
                                           { type: "mousemove", clickCount: "0" },
                                           gBrowser.selectedBrowser);

    if (prefsChanged) {
      yield new Promise(resolve => SpecialPowers.popPrefEnv(resolve));
    }

    // Start checking for the scroll.
    let firstTimestamp = undefined;
    let timeCompensation;
    do {
      let timestamp = yield new Promise(resolve => window.requestAnimationFrame(resolve));
      if (firstTimestamp === undefined) {
        firstTimestamp = timestamp;
      }

      // This value is calculated similarly to the value of the same name in
      // ClickEventHandler.autoscrollLoop, except here it's cumulative across
      // all frames after the first one instead of being based only on the
      // current frame.
      timeCompensation = (timestamp - firstTimestamp) / 20;
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
    } while (timeCompensation < 5);

    // Close the autoscroll popup by synthesizing Esc.
    EventUtils.synthesizeKey("VK_ESCAPE", {});
    let scrollVert = test.expected & expectScrollVert;
    let scrollHori = test.expected & expectScrollHori;

    let checkScroll = yield ContentTask.spawn(gBrowser.selectedBrowser,
                                              { scrollVert : scrollVert,
                                                scrollHori: scrollHori,
                                                elemid : test.elem,
                                                checkWindow: test.testwindow },
      function* (args) {
        let msg = "";
        if (args.checkWindow) {
          if (!((args.scrollVert && content.scrollY > 0) ||
                (!args.scrollVert && content.scrollY == 0))) {
            msg += "Failed: ";
          }
          msg += 'Window for ' + args.elemid + ' should' + (args.scrollVert ? '' : ' not') + ' have scrolled vertically\n';
         
          if (!((args.scrollHori && content.scrollX > 0) ||
                (!args.scrollHori && content.scrollX == 0))) {
            msg += "Failed: ";
          }
          msg += ' Window for ' + args.elemid + ' should' + (args.scrollHori ? '' : ' not') + ' have scrolled horizontally\n';
        } else {
          let elem = content.document.getElementById(args.elemid);
          if (!((args.scrollVert && elem.scrollTop > 0) ||
                (!args.scrollVert && elem.scrollTop == 0))) {
            msg += "Failed: ";
          }
          msg += ' ' + args.elemid + ' should' + (args.scrollVert ? '' : ' not') + ' have scrolled vertically\n';
          if (!((args.scrollHori && elem.scrollLeft > 0) ||
                (!args.scrollHori && elem.scrollLeft == 0))) {
            msg += "Failed: ";
          }
          msg += args.elemid + ' should' + (args.scrollHori ? '' : ' not') + ' have scrolled horizontally';
        }

        return msg;
       }
    );

    ok(checkScroll.indexOf("Failed") == -1, checkScroll)

    // Before continuing the test, we need to ensure that the IPC
    // message that stops autoscrolling has had time to arrive.
    yield new Promise(resolve => executeSoon(resolve));
  }

  // remove 2 tabs that were opened by middle-click on links
  while (gBrowser.visibleTabs.length > 1) {
    gBrowser.removeTab(gBrowser.visibleTabs[gBrowser.visibleTabs.length - 1]);
  }

  // wait for focus to fix a failure in the next test if the latter runs too soon.
  yield SimpleTest.promiseFocus();
});
