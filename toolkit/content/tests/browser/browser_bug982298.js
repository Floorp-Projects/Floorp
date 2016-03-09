/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var Ci = Components.interfaces;

var tab;
var browser;

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
  info("waiting for load event");
}

function find()
{
  info("got load event in 'find' function");
  browser.removeEventListener("load", find, true);
  let listener = {
    onFindResult(aData) {
      info("got find result");
      browser.finder.removeResultListener(listener);

      ok(aData.result == Ci.nsITypeAheadFind.FIND_FOUND, "should find string");

      browser.messageManager.addMessageListener("ScrollDone",
                                                function f(aMsg) {
        info("got ScrollDone event");
        browser.messageManager.removeMessageListener("ScrollDone", f);
        browser.loadURI("about:blank");
        browser.addEventListener("load", findAgain, true);
      });

      // scroll textarea to bottom
      browser.messageManager.loadFrameScript("data:text/javascript;base64," +
                                             btoa(scrollTest), false);

    },
    onCurrentSelection() {}
  };
  info("about to add results listener, open find bar, and send 'F' string");
  browser.finder.addResultListener(listener);
  gFindBar.onFindCommand();
  EventUtils.sendString("F");
  info("added result listener and sent string 'F'");
}

function findAgain()
{
  info("got load event in 'findAgain' function");
  browser.removeEventListener("load", findAgain, true);

  ok(browser.currentURI.spec == "about:blank", "cannot navigate to about:blank");
  let listener = {
    onFindResult(aData) {
      info("got find result #2");
      browser.finder.removeResultListener(listener);
      cleanup();
    },
    onCurrentSelection() {}
  };

  browser.finder.addResultListener(listener);
  // find again needs delay for crash test
  setTimeout(function() {
    // ignore exception if occured
    try {
      info("about to send find again command");
      gFindBar.onFindAgainCommand(false);
      info("sent find again command");
    } catch (e) {
      info("got exception from onFindAgainCommand: " + e);
    }
    cleanup();
  }, 0);
  info("added result listener");
}

function cleanup()
{
  info("in cleanup function");
  gBrowser.removeTab(tab);
  finish();
}
