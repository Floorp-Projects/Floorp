"use strict";

const WARNING_PATTERN = [
  {
    key: "INSECURE_FORM_ACTION",
    msg:
      'JavaScript Warning: "Password fields present in a form with an insecure (http://) form action. This is a security risk that allows user login credentials to be stolen."',
  },
  {
    key: "INSECURE_PAGE",
    msg:
      'JavaScript Warning: "Password fields present on an insecure (http://) page. This is a security risk that allows user login credentials to be stolen."',
  },
];

add_task(async function testInsecurePasswordWarning() {
  // By default, proxies don't apply to 127.0.0.1. We need them to for this test, though:
  await SpecialPowers.pushPrefEnv({
    set: [["network.proxy.allow_hijacking_localhost", true]],
  });
  let warningPatternHandler;

  function messageHandler(msgObj) {
    function findWarningPattern(msg) {
      return WARNING_PATTERN.find(patternPair => {
        return msg.includes(patternPair.msg);
      });
    }

    let warning = findWarningPattern(msgObj.message);

    // Only handle the insecure password related warning messages.
    if (warning) {
      // Prevent any unexpected or redundant matched warning message coming after
      // the test case is ended.
      ok(warningPatternHandler, "Invoke a valid warning message handler");
      warningPatternHandler(warning, msgObj.message);
    }
  }
  Services.console.registerListener(messageHandler);
  registerCleanupFunction(function() {
    Services.console.unregisterListener(messageHandler);
  });

  for (let [origin, testFile, expectWarnings] of [
    ["http://127.0.0.1", "form_basic.html", []],
    ["http://127.0.0.1", "formless_basic.html", []],
    ["http://example.com", "form_basic.html", ["INSECURE_PAGE"]],
    ["http://example.com", "formless_basic.html", ["INSECURE_PAGE"]],
    ["https://example.com", "form_basic.html", []],
    ["https://example.com", "formless_basic.html", []],

    // For a form with customized action link in the same origin.
    ["http://127.0.0.1", "form_same_origin_action.html", []],
    ["http://example.com", "form_same_origin_action.html", ["INSECURE_PAGE"]],
    ["https://example.com", "form_same_origin_action.html", []],

    // For a form with an insecure (http) customized action link.
    [
      "http://127.0.0.1",
      "form_cross_origin_insecure_action.html",
      ["INSECURE_FORM_ACTION"],
    ],
    [
      "http://example.com",
      "form_cross_origin_insecure_action.html",
      ["INSECURE_PAGE"],
    ],
    [
      "https://example.com",
      "form_cross_origin_insecure_action.html",
      ["INSECURE_FORM_ACTION"],
    ],

    // For a form with a secure (https) customized action link.
    ["http://127.0.0.1", "form_cross_origin_secure_action.html", []],
    [
      "http://example.com",
      "form_cross_origin_secure_action.html",
      ["INSECURE_PAGE"],
    ],
    ["https://example.com", "form_cross_origin_secure_action.html", []],
  ]) {
    let testURL = origin + DIRECTORY_PATH + testFile;
    let promiseConsoleMessages = new Promise(resolve => {
      warningPatternHandler = function(warning, originMessage) {
        ok(warning, "Handling a warning pattern");
        let fullMessage = `[${warning.msg} {file: "${testURL}" line: 0 column: 0 source: "0"}]`;
        is(originMessage, fullMessage, "Message full matched:" + originMessage);

        let index = expectWarnings.indexOf(warning.key);
        isnot(
          index,
          -1,
          "Found warning: " + warning.key + " for URL:" + testURL
        );
        if (index !== -1) {
          // Remove the shown message.
          expectWarnings.splice(index, 1);
        }
        if (expectWarnings.length === 0) {
          info("All warnings are shown for URL:" + testURL);
          resolve();
        }
      };
    });

    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
        url: testURL,
      },
      function() {
        if (expectWarnings.length === 0) {
          info("All warnings are shown for URL:" + testURL);
          return Promise.resolve();
        }
        return promiseConsoleMessages;
      }
    );

    // Remove warningPatternHandler to stop handling the matched warning pattern
    // and the task should not get any warning anymore.
    warningPatternHandler = null;
  }
});
