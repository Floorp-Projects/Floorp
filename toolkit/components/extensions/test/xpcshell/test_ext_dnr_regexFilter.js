"use strict";

// This file provides test coverage for regexFilter and regexSubstitution.
//
// The validate_actions task of test_ext_dnr_session_rules.js checks that the
// basic requirements of regexFilter + regexSubstitution are met.
//
// The match_regexFilter task of test_ext_dnr_testMatchOutcome.js verifies that
// regexFilter is evaluated correctly in testMatchOutcome.
//
// The quota on regexFilter is verified in test_ext_dnr_regexFilter_limits.js.

add_setup(() => {
  Services.prefs.setBoolPref("extensions.manifestV3.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.enabled", true);
  Services.prefs.setBoolPref("extensions.dnr.feedback", true);
});

const server = createHttpServer({
  hosts: ["example.com", "example-com", "from", "dest"],
});
server.registerPrefixHandler("/", (req, res) => {
  res.setHeader("Access-Control-Allow-Origin", "*");
  res.write("GOOD_RESPONSE");
});

// This function is serialized and called in the context of the test extension's
// background page. dnrTestUtils is passed to the background function.
function makeDnrTestUtils() {
  const dnrTestUtils = {};
  const dnr = browser.declarativeNetRequest;

  async function testFetch(from, to, description) {
    let res = await fetch(from);
    browser.test.assertEq(to, res.url, description);
    browser.test.assertEq("GOOD_RESPONSE", await res.text(), "expected body");
  }

  async function _testRegexFilterOrRedirect({
    description,
    regexFilter,
    isUrlFilterCaseSensitive,
    expectedRedirectUrl = "http://dest/",
    regexSubstitution = expectedRedirectUrl,
    urlsMatching,
    urlsNonMatching,
  }) {
    browser.test.log(`Test description: ${description}`);
    await dnr.updateSessionRules({
      addRules: [
        {
          id: 12345,
          condition: { regexFilter, isUrlFilterCaseSensitive },
          action: { type: "redirect", redirect: { regexSubstitution } },
        },
      ],
    });
    for (let url of urlsMatching) {
      const description = `regexFilter ${regexFilter} should match: ${url}`;
      await testFetch(url, expectedRedirectUrl, description);
    }
    for (let url of urlsNonMatching) {
      const description = `regexFilter ${regexFilter} should not match: ${url}`;
      let expectedUrl = new URL(url);
      expectedUrl.hash = "";
      await testFetch(url, expectedUrl.href, description);
    }
    await dnr.updateSessionRules({ removeRuleIds: [12345] });
  }

  async function testValidRegexFilter({
    description,
    regexFilter,
    isUrlFilterCaseSensitive,
    urlsMatching,
    urlsNonMatching,
  }) {
    browser.test.assertDeepEq(
      { isSupported: true },
      await dnr.isRegexSupported({
        regex: regexFilter,
        isCaseSensitive: isUrlFilterCaseSensitive,
      }),
      `isRegexSupported should detect support for: ${regexFilter}`
    );
    await _testRegexFilterOrRedirect({
      description,
      regexFilter,
      isUrlFilterCaseSensitive,
      expectedRedirectUrl: "http://dest/",
      regexSubstitution: "http://dest/",
      urlsMatching,
      urlsNonMatching,
    });
  }

  async function testValidRegexSubstitution({
    description,
    regexFilter,
    regexSubstitution,
    inputUrl,
    expectedRedirectUrl,
  }) {
    browser.test.assertDeepEq(
      { isSupported: true },
      await dnr.isRegexSupported({
        regex: regexFilter,
        // requireCapturing option not strictly needed, but included to verify
        // that the method can take the option without issues.
        requireCapturing: true,
      }),
      `isRegexSupported should accept regexFilter: ${regexFilter}`
    );

    await _testRegexFilterOrRedirect({
      description,
      regexFilter,
      regexSubstitution,
      urlsMatching: [inputUrl],
      urlsNonMatching: [],
      expectedRedirectUrl,
    });
  }

  async function testInvalidRegexFilter(regexFilter, expectedError, msg) {
    browser.test.assertDeepEq(
      { isSupported: false, reason: "syntaxError" },
      await dnr.isRegexSupported({ regex: regexFilter }),
      `isRegexSupported should detect unsupported regex: ${regexFilter}`
    );
    await browser.test.assertRejects(
      dnr.updateSessionRules({
        addRules: [
          { id: 123, condition: { regexFilter }, action: { type: "block" } },
        ],
      }),
      expectedError,
      `Should reject invalid regexFilter (${regexFilter}) - ${msg}`
    );
  }

  async function testInvalidRegexSubstitution(
    regexSubstitution,
    expectedError,
    msg
  ) {
    await browser.test.assertRejects(
      _testRegexFilterOrRedirect({
        description: `testInvalidRegexSubstitution: "${regexSubstitution}"`,
        regexFilter: ".",
        regexSubstitution,
        urlsMatching: [],
        urlsNonMatching: [],
      }),
      expectedError,
      msg
    );
  }

  async function testRejectedRedirectAtRuntime({ regexSubstitution, url }) {
    // Some regexSubstitution rules pass validation but the generated redirect
    // URL is rejected at runtime. That is validated here.
    await _testRegexFilterOrRedirect({
      description: `testRejectedRedirectAtRuntime for URL: ${url}`,
      regexFilter: "http://from/.*",
      regexSubstitution,
      // When regexSubstitution is invalid, it should not be redirected:
      expectedRedirectUrl: url,
      urlsMatching: [url],
      urlsNonMatching: [],
    });
  }

  Object.assign(dnrTestUtils, {
    testValidRegexFilter,
    testValidRegexSubstitution,
    testInvalidRegexFilter,
    testInvalidRegexSubstitution,
    testRejectedRedirectAtRuntime,
  });
  return dnrTestUtils;
}

async function runAsDNRExtension({ background, manifest }) {
  let extension = ExtensionTestUtils.loadExtension({
    background: `(${background})((${makeDnrTestUtils})())`,
    allowInsecureRequests: true,
    manifest: {
      manifest_version: 3,
      permissions: ["declarativeNetRequest"],
      // host_permissions are needed for the redirect action.
      host_permissions: ["<all_urls>"],
      granted_host_permissions: true,
      ...manifest,
    },
    temporarilyInstalled: true, // <-- for granted_host_permissions
  });
  await extension.startup();
  await extension.awaitFinish();
  await extension.unload();
}

// The least common denominator across Chrome, Safari and Firefox is Safari, at
// the time of writing, the supported syntax in Safari's regexFilter is
// documented at https://webkit.org/blog/3476/content-blockers-first-look/,
// section "The Regular expression format":
//
// - Matching any character with “.”.
// - Matching ranges with the range syntax [a-b].
// - Quantifying expressions with “?”, “+” and “*”.
// - Groups with parenthesis.
// - ... beginning of line (“^”) and end of line (“$”) marker ...
//
// The above syntax is very limited, as expressed at
// https://github.com/w3c/webextensions/issues/344
//
// The tests continue in regexFilter_more_than_basic.
add_task(async function regexFilter_basic() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testValidRegexFilter } = dnrTestUtils;

      await testValidRegexFilter({
        description: "URL as regexFilter is sometimes a valid regexp",
        regexFilter: "http://example.com/",
        urlsMatching: [
          "http://example.com/",
          // dot is wildcard.
          "http://example-com/",
          // Without ^ anchor, matches substring elsewhere.
          "http://from/http://example.com/",
        ],
        urlsNonMatching: [
          "http://dest/http://example.com-no-slash-after-.com",
          // Does not match reference fragment.
          "http://dest/#http://example.com/",
        ],
      });

      await testValidRegexFilter({
        description: "\\. is literal dot",
        regexFilter: "http://example\\.com/",
        urlsMatching: ["http://example.com/"],
        urlsNonMatching: ["http://example-com/"],
      });

      await testValidRegexFilter({
        description: "[a-b] range is supported",
        regexFilter: "http://from/[a-b]",
        urlsMatching: ["http://from/a", "http://from/b"],
        urlsNonMatching: ["http://from/c", "http://from/"],
      });

      await testValidRegexFilter({
        description: "groups with parenthesis are supported",
        regexFilter: "http://from/(a)",
        urlsMatching: ["http://from/a", "http://from/aa"],
        urlsNonMatching: ["http://from/b", "http://from/ba"],
      });

      await testValidRegexFilter({
        description: "+, * and ? are quantifiers",
        regexFilter: "a+b*c?d",
        urlsMatching: [
          "http://from/ad",
          "http://from/abcd",
          "http://from/aaabbcd",
        ],
        urlsNonMatching: [
          "http://from/bcd", // "a+" requires "a" to be specified.
          "http://from/abccd", // "c?" matches only one c, but got two.
        ],
      });

      await testValidRegexFilter({
        description: ".* matches anything",
        regexFilter: "a.*b",
        urlsMatching: ["http://from/ab/", "http://from/aANYTHINGb"],
        urlsNonMatching: ["http://from/a"],
      });

      await testValidRegexFilter({
        description: "^ is start-of-string anchor",
        regexFilter: "^http://from/",
        urlsMatching: ["http://from/", "http://from/path"],
        urlsNonMatching: ["http://dest/^http://from/"],
      });

      await testValidRegexFilter({
        description: "$ is end-of-string anchor",
        regexFilter: "http://from/$",
        urlsMatching: ["http://from/", "http://dest/http://from/"],
        urlsNonMatching: ["http://from/path", "http://from/$"],
      });

      browser.test.notifyPass();
    },
  });
});

// regexFilter_basic lists the bare minimum, this tests more useful features.
add_task(async function regexFilter_more_than_basic() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testValidRegexFilter } = dnrTestUtils;

      // Use cases listed at
      // https://github.com/w3c/webextensions/issues/344#issuecomment-1430358116

      await testValidRegexFilter({
        description: "{n,m} quantifier",
        regexFilter: "http://from/a{2,3}b",
        urlsMatching: ["http://from/aab", "http://from/aaab"],
        urlsNonMatching: ["http://from/ab", "http://from/aaaab"],
      });

      await testValidRegexFilter({
        description: "{n,} quantifier",
        regexFilter: "http://from/a{2,}$",
        urlsMatching: ["http://from/aa", "http://from/aaa", "http://from/aaaa"],
        urlsNonMatching: ["http://from/a"],
      });

      await testValidRegexFilter({
        description: "| disjunction and within groups",
        regexFilter: "from/a|from/b$|c$",
        urlsMatching: ["http://from/a", "http://from/b", "http://from/c"],
        urlsNonMatching: ["http://from/b$|c$"],
      });

      await testValidRegexFilter({
        description: "(?!) negative look-ahead",
        regexFilter: "http://from/a(?!notme|$)",
        urlsMatching: ["http://from/aOK"],
        urlsNonMatching: ["http://from/anotme", "http://from/a"],
      });

      // Features based on
      // https://github.com/w3c/webextensions/issues/344#issuecomment-1430127543
      await testValidRegexFilter({
        description: "Negated character class",
        regexFilter: "http://from/[^a-z]",
        urlsMatching: ["http://from/1"],
        urlsNonMatching: ["http://from/a", "http://from/y", "http://from/"],
      });

      await testValidRegexFilter({
        description: "Word character class (\\w)",
        regexFilter: "http://from/\\w",
        urlsMatching: ["http://from/1", "http://from/a", "http://from/_"],
        urlsNonMatching: ["http://from/-", "http://from/%20"],
      });

      // Rule that leads to "memoryLimitExceeded" in Chrome:
      // https://github.com/w3c/webextensions/issues/344#issuecomment-1424527627
      await testValidRegexFilter({
        description: "regexFilter that triggers memoryLimitExceeded in Chrome",
        regexFilter: "(https?://)104\\.154\\..{100,}",
        urlsMatching: ["http://from/http://104.154.0.0/" + "x".repeat(100)],
        urlsNonMatching: ["http://from/http://104.154.0.0/too-short"],
      });

      browser.test.notifyPass();
    },
  });
});

// Adds more coverage in addition to what was tested by validate_regexFilter in
// test_ext_dnr_session_rules.js.
add_task(async function regexFilter_invalid() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testInvalidRegexFilter } = dnrTestUtils;

      await testInvalidRegexFilter(
        "(",
        "regexFilter is not a valid regular expression",
        "( opens a group and should be closed"
      );

      await testInvalidRegexFilter(
        "straß.d",
        "regexFilter should not contain non-ASCII characters",
        "regexFilter matches the canonical URL which does not contain non-ASCII"
      );

      browser.test.notifyPass();
    },
  });
});

add_task(async function regexFilter_isUrlFilterCaseSensitive() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testValidRegexFilter } = dnrTestUtils;

      await testValidRegexFilter({
        description: "isUrlFilterCaseSensitive omitted (= false by default)",
        // isUrlFilterCaseSensitive = false by default.
        regexFilter: "from/Pa",
        urlsMatching: ["http://from/Pa", "http://from/pa", "http://from/PA"],
        urlsNonMatching: [],
      });

      await testValidRegexFilter({
        description: "isUrlFilterCaseSensitive: false",
        isUrlFilterCaseSensitive: false,
        regexFilter: "from/Pa",
        urlsMatching: ["http://from/Pa", "http://from/pa", "http://from/PA"],
        urlsNonMatching: [],
      });

      await testValidRegexFilter({
        description: "isUrlFilterCaseSensitive: true",
        isUrlFilterCaseSensitive: true,
        regexFilter: "from/Pa",
        urlsMatching: ["http://from/Pa"],
        urlsNonMatching: ["http://from/pa", "http://from/PA"],
      });

      await testValidRegexFilter({
        description: "Case-sensitive uppercase regexFilter cannot match HOST",
        isUrlFilterCaseSensitive: true,
        regexFilter: "FROM",
        urlsMatching: [],
        urlsNonMatching: ["http://FROM/canonical_host_is_lowercase"],
      });

      browser.test.notifyPass();
    },
  });
});

add_task(async function regexSubstitution_invalid() {
  let { messages } = await promiseConsoleOutput(async () => {
    await runAsDNRExtension({
      manifest: { browser_specific_settings: { gecko: { id: "@dnr" } } },
      background: async dnrTestUtils => {
        const { testRejectedRedirectAtRuntime, testInvalidRegexSubstitution } =
          dnrTestUtils;

        await testInvalidRegexSubstitution(
          "http://dest/\\x20",
          "redirect.regexSubstitution only allows digit or \\ after \\.",
          "\\x should not be allowed in regexSubstitution"
        );

        await testInvalidRegexSubstitution(
          "http://dest/?\\",
          "redirect.regexSubstitution only allows digit or \\ after \\.",
          "\\<end> should not be allowed in regexSubstitution"
        );

        await testRejectedRedirectAtRuntime({
          regexSubstitution: "not-URL",
          url: "http://from/should_not_be_directed_invalid_url",
        });

        await testRejectedRedirectAtRuntime({
          regexSubstitution: "javascript://-URL",
          url: "http://from/should_not_be_directed_javascript_url",
        });

        // May be allowed once bug 1622986 is fixed.
        await testRejectedRedirectAtRuntime({
          regexSubstitution: "data:,redirect-from-dnr",
          url: "http://from/should_not_be_directed_disallowed_url",
        });

        await testRejectedRedirectAtRuntime({
          regexSubstitution: "resource://gre/",
          url: "http://from/should_not_be_directed_resource_url",
        });

        browser.test.notifyPass();
      },
    });
  });

  AddonTestUtils.checkMessages(messages, {
    expected: [
      {
        message: /Extension @dnr tried to redirect to an invalid URL: not-URL/,
      },
      {
        message: /Extension @dnr may not redirect to: javascript:\/\/-URL/,
      },
      {
        message: /Extension @dnr may not redirect to: data:,redirect-from-dnr/,
      },
      {
        message: /Extension @dnr may not redirect to: resource:\/\/gre\//,
      },
    ],
  });
});

add_task(async function regexSubstitution_valid() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testValidRegexSubstitution } = dnrTestUtils;

      await testValidRegexSubstitution({
        description: "All captured groups can be accessed by \\1 - \\9",
        regexFilter: "from/(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)(.)",
        regexSubstitution: "http://dest/\\9\\8\\7\\6\\5\\4\\3\\2\\1",
        inputUrl: "http://from/abcdef?gh-ignoredsuffix",
        // ^ captured groups:  123456789
        expectedRedirectUrl: "http://dest/hg?fedcba",
      });

      await testValidRegexSubstitution({
        description: "\\0 captures the full match",
        regexFilter: "from/$",
        regexSubstitution: "http://dest/\\0/end",
        inputUrl: "http://from/",
        expectedRedirectUrl: "http://dest/from//end",
      });

      await testValidRegexSubstitution({
        description: "\\10 means: captured group 1 + literal 0",
        regexFilter: "/(captured)$",
        regexSubstitution: "http://dest/\\10",
        inputUrl: "http://from/captured",
        expectedRedirectUrl: "http://dest/captured0",
      });

      await testValidRegexSubstitution({
        description: "\\\\ is an escaped backslash",
        regexFilter: "/(XXX)",
        regexSubstitution: "http://dest/?\\1\\\\1\\\\\\\\1\\1",
        inputUrl: "http://from/XXX",
        expectedRedirectUrl: "http://dest/?XXX\\1\\\\1XXX",
      });

      await testValidRegexSubstitution({
        description: "Captured groups can be repeated",
        regexFilter: "/(captured)$",
        regexSubstitution: "http://dest/\\1+\\1",
        inputUrl: "http://from/captured",
        expectedRedirectUrl: "http://dest/captured+captured",
      });

      await testValidRegexSubstitution({
        description: "Non-matching optional group is an empty string",
        regexFilter: "(doesnotmatch)?suffix",
        regexSubstitution: "http://dest/[\\1]=group1_is_optional",
        inputUrl: "http://from/suffix",
        expectedRedirectUrl: "http://dest/[]=group1_is_optional",
      });

      await testValidRegexSubstitution({
        description: "Non-existing capturing group is an empty string",
        regexFilter: "(captured)",
        regexSubstitution: "http://dest/[\\2]=missing_group_2",
        inputUrl: "http://from/captured",
        expectedRedirectUrl: "http://dest/[]=missing_group_2",
      });

      await testValidRegexSubstitution({
        description: "Non-capturing group is not captured",
        regexFilter: "(?:non-)(captured)",
        regexSubstitution: "http://dest/[\\1]=only_captured_group",
        inputUrl: "http://from/non-captured",
        expectedRedirectUrl: "http://dest/[captured]=only_captured_group",
      });

      browser.test.notifyPass();
    },
  });
});

add_task(async function regexSubstitution_redirect_chain() {
  await runAsDNRExtension({
    background: async dnrTestUtils => {
      const { testValidRegexSubstitution } = dnrTestUtils;

      await testValidRegexSubstitution({
        description: "regexFilter matches intermediate redirect URLs",
        regexFilter: "^(http://from/)(a|b|c)(.+)",
        regexSubstitution: "\\1\\3",
        inputUrl: "http://from/abcdef",
        // After redirecting three times, we end up here:
        expectedRedirectUrl: "http://from/def",
      });

      browser.test.notifyPass();
    },
  });
});
