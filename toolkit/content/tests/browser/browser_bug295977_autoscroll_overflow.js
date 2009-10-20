function test()
{
  const kPrefName_AutoScroll = "general.autoScroll";
  var prefSvc = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService(Components.interfaces.nsIPrefBranch2);
  prefSvc.setBoolPref(kPrefName_AutoScroll, true);

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
    {elem: 'f', expected: expectScrollNone}
  ];

  var doc;

  function nextTest() {
    var test = allTests.shift();
    if(!test) {
      endTest();
      return;
    }
    var elem = doc.getElementById(test.elem);
    EventUtils.synthesizeMouse(elem, 10, 10, { button: 1 },
                               gBrowser.contentWindow);
    EventUtils.synthesizeMouse(elem, 80, 80,
                               { type: "mousemove", clickCount: "0" },
                               gBrowser.contentWindow);
    // the autoscroll implementation uses a 20ms interval
    // wait for 40ms to make sure it did autoscroll at least once
    setTimeout(function () {
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
    }, 40);
  }

  waitForExplicitFinish();
  gBrowser.addEventListener("load", onLoad, false);
  var dataUri = 'data:text/html,<body>\
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
    </body>';
  gBrowser.loadURI(dataUri);

  function onLoad() {
    gBrowser.removeEventListener("load", onLoad, false);
    waitForFocus(onFocus, content);
  }

  function onFocus() {
    doc = gBrowser.contentDocument;
    nextTest();
  }

  function endTest() {
    // restore the changed prefs
    prefSvc.clearUserPref(kPrefName_AutoScroll);

    // cleaning-up
    gBrowser.addTab().linkedBrowser.stop();
    gBrowser.removeCurrentTab();

    finish();
  }
}
