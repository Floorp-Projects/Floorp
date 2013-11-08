XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/commonjs/sdk/core/promise.js");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
Components.utils.import("resource://gre/modules/Timer.jsm", this);

let gTabs = [];

registerCleanupFunction(function() {
  for (let tab of gTabs) {
    if (!tab)
      continue;
    gBrowser.removeTab(tab);
  }
});

function test() {
  waitForExplicitFinish();

  Task.spawn(function() {
    info("Check correct 'Phrase not found' on new tab");

    // Create a tab to run the test.
    yield promiseAboutHomeLoad();

    // Search for the first word.
    yield promiseFindFinished("--- THIS SHOULD NEVER MATCH ---", false);
    let findbar = gBrowser.getFindBar();
    is(findbar._findStatusDesc.textContent, findbar._notFoundStr,
       "Findbar status text should be 'Phrase not found'");

    // Create second tab.
    yield promiseAboutHomeLoad();

    // Search for a string that WILL be found, with 'Highlight All' on
    yield promiseFindFinished("s", true);
    ok(!gBrowser.getFindBar()._findStatusDesc.textContent,
       "Findbar status should be empty");

    finish();
  });
}

function promiseAboutHomeLoad() {
  let deferred = Promise.defer();

  let tab = gBrowser.selectedTab = gBrowser.addTab("about:home");
  gTabs.push(tab);
  let browser = gBrowser.selectedBrowser;
  browser.addEventListener("load", function listener() {
    if (browser.currentURI.spec == "about:blank")
      return;
    info("Page loaded: " + browser.currentURI.spec);
    browser.removeEventListener("load", listener, true);

    deferred.resolve();
  }, true);

  return deferred.promise;
}

function promiseFindFinished(searchText, highlightOn) {
  let deferred = Promise.defer();

  let findbar = gBrowser.getFindBar();
  findbar.startFind(findbar.FIND_NORMAL);
  let highlightElement = findbar.getElement("highlight");
  if (highlightElement.checked != highlightOn)
    highlightElement.click();
  executeSoon(() => {
    findbar._findField.value = searchText;

    let resultListener;
    let findTimeout = setTimeout(() => foundOrTimedout(true), 2000);
    let foundOrTimedout = function(timedOut) {
      if (timedOut)
        info("Result listener not called, timeout reached.");
      clearTimeout(findTimeout);
      findbar.browser.finder.removeResultListener(resultListener);
      deferred.resolve();
    }

    resultListener = {
      onFindResult: foundOrTimedout
    };
    findbar.browser.finder.addResultListener(resultListener);
    findbar._find();
  });

  return deferred.promise;
}
