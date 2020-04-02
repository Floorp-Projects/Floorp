var tests = [
  // Config is an array with 4 elements:
  // - annotation blacklist
  // - annotation whitelist
  // - tracking blacklist
  // - tracking whitelist

  // All disabled.
  {
    config: [false, false, false, false],
    loadExpected: true,
    annotationExpected: false,
  },

  // Just whitelisted.
  {
    config: [false, false, false, true],
    loadExpected: true,
    annotationExpected: false,
  },

  // Just blacklisted.
  {
    config: [false, false, true, false],
    loadExpected: false,
    annotationExpected: false,
  },

  // whitelist + blacklist => whitelist wins
  {
    config: [false, false, true, true],
    loadExpected: true,
    annotationExpected: false,
  },

  // just annotated in whitelist.
  {
    config: [false, true, false, false],
    loadExpected: true,
    annotationExpected: false,
  },

  // TP and annotation whitelisted.
  {
    config: [false, true, false, true],
    loadExpected: true,
    annotationExpected: false,
  },

  // Annotation whitelisted, but TP blacklisted.
  {
    config: [false, true, true, false],
    loadExpected: false,
    annotationExpected: false,
  },

  // Annotation whitelisted. TP blacklisted and whitelisted: whitelist wins.
  {
    config: [false, true, true, true],
    loadExpected: true,
    annotationExpected: false,
  },

  // Just blacklist annotated.
  {
    config: [true, false, false, false],
    loadExpected: true,
    annotationExpected: true,
  },

  // annotated but TP whitelisted.
  {
    config: [true, false, false, true],
    loadExpected: true,
    annotationExpected: true,
  },

  // annotated and blacklisted.
  {
    config: [true, false, true, false],
    loadExpected: false,
    annotationExpected: false,
  },

  // annotated, TP blacklisted and whitelisted: whitelist wins.
  {
    config: [true, false, true, true],
    loadExpected: true,
    annotationExpected: true,
  },

  // annotated in white and blacklist.
  {
    config: [true, true, false, false],
    loadExpected: true,
    annotationExpected: false,
  },

  // annotated in white and blacklist. TP Whiteslited
  {
    config: [true, true, false, true],
    loadExpected: true,
    annotationExpected: false,
  },

  // everywhere. TP whitelist wins.
  {
    config: [true, true, true, true],
    loadExpected: true,
    annotationExpected: false,
  },
];

function prefBlacklistValue(value) {
  return value ? "example.com" : "";
}

function prefWhitelistValue(value) {
  return value ? "mochi.test" : "";
}

async function runTest(test, expectedFlag, expectedTrackingResource, prefs) {
  let config = [
    [
      "urlclassifier.trackingAnnotationTable.testEntries",
      prefBlacklistValue(test.config[0]),
    ],
    [
      "urlclassifier.features.fingerprinting.annotate.blacklistHosts",
      prefBlacklistValue(test.config[0]),
    ],
    [
      "urlclassifier.features.cryptomining.annotate.blacklistHosts",
      prefBlacklistValue(test.config[0]),
    ],
    [
      "urlclassifier.features.socialtracking.annotate.blacklistHosts",
      prefBlacklistValue(test.config[0]),
    ],

    [
      "urlclassifier.trackingAnnotationWhitelistTable.testEntries",
      prefWhitelistValue(test.config[1]),
    ],
    [
      "urlclassifier.features.fingerprinting.annotate.whitelistHosts",
      prefWhitelistValue(test.config[1]),
    ],
    [
      "urlclassifier.features.cryptomining.annotate.whitelistHosts",
      prefWhitelistValue(test.config[1]),
    ],
    [
      "urlclassifier.features.socialtracking.annotate.whitelistHosts",
      prefWhitelistValue(test.config[1]),
    ],

    [
      "urlclassifier.trackingTable.testEntries",
      prefBlacklistValue(test.config[2]),
    ],
    [
      "urlclassifier.features.fingerprinting.blacklistHosts",
      prefBlacklistValue(test.config[2]),
    ],
    [
      "urlclassifier.features.cryptomining.blacklistHosts",
      prefBlacklistValue(test.config[2]),
    ],
    [
      "urlclassifier.features.socialtracking.blacklistHosts",
      prefBlacklistValue(test.config[2]),
    ],

    [
      "urlclassifier.trackingWhitelistTable.testEntries",
      prefWhitelistValue(test.config[3]),
    ],
    [
      "urlclassifier.features.fingerprinting.whitelistHosts",
      prefWhitelistValue(test.config[3]),
    ],
    [
      "urlclassifier.features.cryptomining.whitelistHosts",
      prefWhitelistValue(test.config[3]),
    ],
    [
      "urlclassifier.features.socialtracking.whitelistHosts",
      prefWhitelistValue(test.config[3]),
    ],
  ];

  info("Testing: " + JSON.stringify(config) + "\n");

  await SpecialPowers.pushPrefEnv({ set: config.concat(prefs) });

  // This promise will be resolved when the chromeScript knows if the channel
  // is annotated or not.
  let annotationPromise;
  if (test.loadExpected) {
    info("We want to have annotation information");
    annotationPromise = new Promise(resolve => {
      chromeScript.addMessageListener(
        "last-channel-flags",
        data => resolve(data),
        { once: true }
      );
    });
  }

  // Let's load a script with a random query string to avoid network cache.
  // Using a script as the fingerprinting feature does not block display content
  let result = await new Promise(resolve => {
    let script = document.createElement("script");
    script.setAttribute(
      "src",
      "http://example.com/tests/toolkit/components/url-classifier/tests/mochitest/evil.js?" +
        Math.random()
    );
    script.onload = _ => resolve(true);
    script.onerror = _ => resolve(false);
    document.body.appendChild(script);
  });

  is(result, test.loadExpected, "The loading happened correctly");

  if (annotationPromise) {
    let data = await annotationPromise;
    is(
      !!data.classificationFlags,
      test.annotationExpected,
      "The annotation happened correctly"
    );
    if (test.annotationExpected) {
      is(data.classificationFlags, expectedFlag, "Correct flag");
      is(
        data.isThirdPartyTrackingResource,
        expectedTrackingResource,
        "Tracking resource flag matches"
      );
    }
  }
}

var chromeScript;

function runTests(flag, prefs, trackingResource) {
  chromeScript = SpecialPowers.loadChromeScript(_ => {
    const { Services } = ChromeUtils.import(
      "resource://gre/modules/Services.jsm"
    );

    function onExamResp(subject, topic, data) {
      let channel = subject.QueryInterface(Ci.nsIHttpChannel);
      let classifiedChannel = subject.QueryInterface(Ci.nsIClassifiedChannel);
      if (
        !channel ||
        !classifiedChannel ||
        !channel.URI.spec.startsWith(
          "http://example.com/tests/toolkit/components/url-classifier/tests/mochitest/evil.js"
        )
      ) {
        return;
      }

      // eslint-disable-next-line no-undef
      sendAsyncMessage("last-channel-flags", {
        classificationFlags: classifiedChannel.classificationFlags,
        isThirdPartyTrackingResource: classifiedChannel.isThirdPartyTrackingResource(),
      });
    }

    // eslint-disable-next-line no-undef
    addMessageListener("done", __ => {
      Services.obs.removeObserver(onExamResp, "http-on-examine-response");
    });

    Services.obs.addObserver(onExamResp, "http-on-examine-response");

    // eslint-disable-next-line no-undef
    sendAsyncMessage("start-test");
  });

  chromeScript.addMessageListener(
    "start-test",
    async _ => {
      for (let test in tests) {
        await runTest(tests[test], flag, trackingResource, prefs);
      }

      chromeScript.sendAsyncMessage("done");
      SimpleTest.finish();
    },
    { once: true }
  );
}
