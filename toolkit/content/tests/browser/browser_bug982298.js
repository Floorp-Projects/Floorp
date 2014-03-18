/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Timer.jsm");

const Ci = Components.interfaces;

let tab;
let browser;

const scrollHtml =
  "<textarea id=\"textarea1\" row=2>Firefox\n\nFirefox\n\n\n\n\n\n\n\n\n\n" +
  "</textarea><a href=\"about:blank\">blank</a>";

const scrollTest =
  "var textarea = content.document.getElementById(\"textarea1\");" +
  "textarea.scrollTop = textarea.scrollHeight;" +
  "sendAsyncMessage(\"ScrollDone\", { });"

function test()
{
  waitForExplicitFinish();

  tab = gBrowser.addTab("data:text/html;base64," + 
                        btoa(scrollHtml));
  browser = gBrowser.getBrowserForTab(tab);
  gBrowser.selectedTab = tab;

  browser.addEventListener("load", find, true);
}

function find()
{
  browser.removeEventListener("load", find, true);
  let listener = {
    onFindResult: function(aData) {
      browser.finder.removeResultListener(listener);

      ok(aData.result == Ci.nsITypeAheadFind.FIND_FOUND, "should find string");

      browser.messageManager.addMessageListener("ScrollDone",
                                                function f(aMsg) {
        browser.messageManager.removeMessageListener("ScrollDone", f);
        browser.loadURI("about:blank");
        browser.addEventListener("load", findAgain, true);
      });

      // scroll textarea to bottom
      browser.messageManager.loadFrameScript("data:text/javascript;base64," +
                                             btoa(scrollTest), false);

    },
  };
  browser.finder.addResultListener(listener);
  gFindBar.onFindCommand();
  EventUtils.sendString("F");
}

function findAgain()
{
  browser.removeEventListener("load", findAgain, true);

  ok(browser.currentURI.spec == "about:blank", "cannot navigate to about:blank");
  let listener = {
    onFindResult: function(aData) {
      browser.finder.removeResultListener(listener);
      cleanup();
    },
  };

  browser.finder.addResultListener(listener);
  // find again needs delay for crash test
  setTimeout(function() {
    // ignore exception if occured
    try {
      gFindBar.onFindAgainCommand(false);
    } catch (e) {
    }
    cleanup();
  }, 0);
}

function cleanup()
{
  gBrowser.removeTab(tab);
  finish();
}
