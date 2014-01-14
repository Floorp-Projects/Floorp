/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;

let tab, browser;

function test () {
  waitForExplicitFinish();

  tab = gBrowser.addTab("data:text/html;base64," +
                        btoa("<body><iframe srcdoc=\"content\"/></iframe>" +
                             "<a href=\"http://test.com\">test link</a>"));
  browser = gBrowser.getBrowserForTab(tab);
  gBrowser.selectedTab = tab;

  browser.addEventListener("load", startTests, true);
}

var outlineTest = "sendAsyncMessage(\"OutlineTest\", " +
                  "{ ok : !!content.document." +
                  "getElementsByTagName(\"a\")[0].style.outline }" +
                  ");";

function startTests () {
  browser.removeEventListener("load", startTests, true);

  let finder = browser.finder;
  let listener = {
    onFindResult: function () {
      ok(false, "callback wasn't replaced");
    }
  };
  finder.addResultListener(listener);

  listener.onFindResult = function ({result}) {
    ok(result == Ci.nsITypeAheadFind.FIND_FOUND, "should find string");

    listener.onFindResult = function ({result}) {
      ok(result == Ci.nsITypeAheadFind.FIND_NOTFOUND, "should not find string");

      let first = true;
      listener.onFindResult = function ({result}) {
        ok(result == Ci.nsITypeAheadFind.FIND_FOUND, "should find link");

        browser.messageManager.addMessageListener("OutlineTest", function f(aMessage) {
          browser.messageManager.removeMessageListener("OutlineTest", f);


          if (first) {
            ok(aMessage.data.ok, "content script should send okay");
            first = false;

            // Just a simple search for "test link".
            finder.fastFind("test link", false, false);
          } else {
            ok(!aMessage.data.ok, "content script should not send okay");
            cleanup();
          }
        })
        browser.messageManager.loadFrameScript("data:," + outlineTest, false)
      }
      // Search only for links and draw outlines.
      finder.fastFind("test link", true, true);
    }
    finder.highlight(true, "Bla");
  }
  finder.highlight(true, "content");
}

function cleanup() {
  gBrowser.removeTab(tab);
  finish();
}
