/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 *
 * Contributor(s):
 *  Mihai Șucan <mihai.sucan@gmail.com>
 *
 * ***** END LICENSE BLOCK ***** */

const TESTS_PATH = "http://example.com/browser/toolkit/components/console/hudservice/tests/browser/";
const TESTS = [
  { // #0
    file: "test-bug-595934-css-loader.html",
    category: "CSS Loader",
    matchString: "CSS Loader",
  },
  { // #1
    file: "test-bug-595934-dom-events.html",
    category: "DOM Events",
    matchString: "preventBubble()",
  },
  { // #2
    file: "test-bug-595934-dom-html.html",
    category: "DOM:HTML",
    matchString: "getElementById",
  },
  { // #3
    file: "test-bug-595934-imagemap.html",
    category: "ImageMap",
    matchString: "ImageMap",
  },
  { // #4
    file: "test-bug-595934-html.html",
    category: "HTML",
    matchString: "multipart/form-data",
    onload: function() {
      let form = content.document.querySelector("form");
      form.submit();
    },
  },
  { // #5
    file: "test-bug-595934-malformedxml.xhtml",
    category: "malformed-xml",
    matchString: "malformed-xml",
  },
  { // #6
    file: "test-bug-595934-svg.xhtml",
    category: "SVG",
    matchString: "fooBarSVG",
  },
];

let pos = -1;

let TestObserver = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  observe: function test_observe(aSubject)
  {
    if (!(aSubject instanceof Ci.nsIScriptError)) {
      return;
    }

    is(aSubject.category, TESTS[pos].category,
      "test #" + pos + ": error category");

    if (aSubject.category == TESTS[pos].category) {
      executeSoon(performTest);
    }
    else {
      testEnd();
    }
  }
};

function tabLoad(aEvent) {
  browser.removeEventListener(aEvent.type, arguments.callee, true);

  openConsole();

  let hudId = HUDService.getHudIdByWindow(content);
  hud = HUDService.hudWeakReferences[hudId].get();

  Services.console.registerListener(TestObserver);

  executeSoon(testNext);
}

function testNext() {
  hud.jsterm.clearOutput();

  pos++;
  if (pos < TESTS.length) {
    if (TESTS[pos].onload) {
      browser.addEventListener("load", function(aEvent) {
        browser.removeEventListener(aEvent.type, arguments.callee, true);
        TESTS[pos].onload(aEvent);
      }, true);
    }

    content.location = TESTS_PATH + TESTS[pos].file;
  }
  else {
    testEnd();
  }
}

function testEnd() {
  Services.console.unregisterListener(TestObserver);
  finishTest();
}

function performTest() {
  let textContent = hud.outputNode.textContent;
  isnot(textContent.indexOf(TESTS[pos].matchString), -1,
    "test #" + pos + ": message found");

  testNext();
}

function test() {
  addTab("data:text/html,Web Console test for bug 595934.");
  browser.addEventListener("load", tabLoad, true);
}

