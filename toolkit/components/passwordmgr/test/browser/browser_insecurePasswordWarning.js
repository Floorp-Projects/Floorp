"use strict";

const TEST_URL_PATH = "/browser/toolkit/components/passwordmgr/test/browser/";

const WARNING_PATTERN = [{
  key: "INSECURE_FORM_ACTION",
  msg: 'JavaScript Warning: "Password fields present in a form with an insecure (http://) form action. This is a security risk that allows user login credentials to be stolen."'
}, {
  key: "INSECURE_PAGE",
  msg: 'JavaScript Warning: "Password fields present on an insecure (http://) page. This is a security risk that allows user login credentials to be stolen."'
}];

add_task(function* testInsecurePasswordWarning() {
  let warningPatternHandler;

  function messageHandler(msg) {
    function findWarningPattern(msg) {
      return WARNING_PATTERN.find(patternPair => {
        return msg.indexOf(patternPair.msg) !== -1;
      });
    }

    let warning = findWarningPattern(msg.message);

    // Only handle the insecure password related warning messages.
    if (warning) {
      // Prevent any unexpected or redundant matched warning message coming after
      // the test case is ended.
      ok(warningPatternHandler, "Invoke a valid warning message handler");
      warningPatternHandler(warning, msg.message);
    }
  }
  Services.console.registerListener(messageHandler);
  registerCleanupFunction(function() {
    Services.console.unregisterListener(messageHandler);
  });

  for (let [origin, testFile, expectWarnings] of [
    // Form action at 127.0.0.1/localhost is considered as a secure case.
    // There should be no INSECURE_FORM_ACTION warning at 127.0.0.1/localhost.
    // This will be fixed at Bug 1261234.
    ["http://127.0.0.1", "form_basic.html", ["INSECURE_FORM_ACTION"]],
    ["http://127.0.0.1", "formless_basic.html", []],
    ["http://example.com", "form_basic.html", ["INSECURE_FORM_ACTION", "INSECURE_PAGE"]],
    ["http://example.com", "formless_basic.html", ["INSECURE_PAGE"]],
    ["https://example.com", "form_basic.html", []],
    ["https://example.com", "formless_basic.html", []],
  ]) {
    let testUrlPath = origin + TEST_URL_PATH + testFile;
    var promiseConsoleMessages = new Promise(resolve => {
      warningPatternHandler = function (warning, originMessage) {
        ok(warning, "Handling a warning pattern");
        let fullMessage = `[${warning.msg} {file: "${testUrlPath}" line: 0 column: 0 source: "0"}]`;
        is(originMessage, fullMessage, "Message full matched:" + originMessage);

        let index = expectWarnings.indexOf(warning.key);
        isnot(index, -1, "Found warning: " + warning.key + " for URL:" + testUrlPath);
        if (index !== -1) {
          // Remove the shown message.
          expectWarnings.splice(index, 1);
        }
        if (expectWarnings.length === 0) {
          info("All warnings are shown. URL:" + testUrlPath);
          resolve();
        }
      }
    });

    yield BrowserTestUtils.withNewTab({
      gBrowser,
      url: testUrlPath
    }, function*() {
      if (expectWarnings.length === 0) {
        info("All warnings are shown. URL:" + testUrlPath);
        return Promise.resolve();
      }
      return promiseConsoleMessages;
    });

    // Remove warningPatternHandler to stop handling the matched warning pattern
    // and the task should not get any warning anymore.
    warningPatternHandler = null;
  }
});
