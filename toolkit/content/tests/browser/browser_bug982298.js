const scrollHtml =
  "<textarea id=\"textarea1\" row=2>Firefox\n\nFirefox\n\n\n\n\n\n\n\n\n\n" +
  "</textarea><a href=\"about:blank\">blank</a>";

add_task(function*() {
  let url = "data:text/html;base64," + btoa(scrollHtml);
  yield BrowserTestUtils.withNewTab({gBrowser, url}, function*(browser) {
    let awaitFindResult = new Promise(resolve => {
      let listener = {
        onFindResult(aData) {
          info("got find result");
          browser.finder.removeResultListener(listener);

          ok(aData.result == Ci.nsITypeAheadFind.FIND_FOUND, "should find string");
          resolve();
        },
        onCurrentSelection() {},
        onMatchesCountResult() {}
      };
      info("about to add results listener, open find bar, and send 'F' string");
      browser.finder.addResultListener(listener);
    });
    gFindBar.onFindCommand();
    EventUtils.sendString("F");
    info("added result listener and sent string 'F'");
    yield awaitFindResult;

    let awaitScrollDone = BrowserTestUtils.waitForMessage(browser.messageManager, "ScrollDone");
    // scroll textarea to bottom
    const scrollTest =
      "var textarea = content.document.getElementById(\"textarea1\");" +
      "textarea.scrollTop = textarea.scrollHeight;" +
      "sendAsyncMessage(\"ScrollDone\", { });"
    browser.messageManager.loadFrameScript("data:text/javascript;base64," +
                                           btoa(scrollTest), false);
    yield awaitScrollDone;
    info("got ScrollDone event");
    yield BrowserTestUtils.loadURI(browser, "about:blank");
    yield BrowserTestUtils.browserLoaded(browser);

    ok(browser.currentURI.spec == "about:blank", "got load event for about:blank");

    let awaitFindResult2 = new Promise(resolve => {
      let listener = {
        onFindResult(aData) {
          info("got find result #2");
          browser.finder.removeResultListener(listener);
          resolve();
        },
        onCurrentSelection() {},
        onMatchesCountResult() {}
      };

      browser.finder.addResultListener(listener);
      info("added result listener");
    });
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
    }, 0);
    yield awaitFindResult2;
  });
});
