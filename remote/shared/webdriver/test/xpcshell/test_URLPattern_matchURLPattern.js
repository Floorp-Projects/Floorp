/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { matchURLPattern, parseURLPattern } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/URLPattern.sys.mjs"
);

// Test several variations which should match a string based http://example.com
// pattern.
add_task(async function test_matchURLPattern_url_variations() {
  const pattern = parseURLPattern({
    type: "string",
    pattern: "http://example.com",
  });

  const urls = [
    "http://example.com",
    "http://EXAMPLE.com",
    "http://user:password@example.com",
    "http://example.com:80",
    "http://example.com/",
    "http://example.com/#some-hash",
    "http:example.com",
    "http:/example.com",
    "http://example.com?",
    "http://example.com/?",
  ];
  for (const url of urls) {
    ok(
      matchURLPattern(pattern, url),
      `url "${url}" should match pattern "http://example.com"`
    );
  }

  // Test URLs close to http://example.com but which should not match.
  const failingUrls = [
    "https://example.com",
    "http://example.com:88",
    "http://example.com/a",
    "http://example.com/?abc",
  ];
  for (const url of failingUrls) {
    ok(
      !matchURLPattern(pattern, url),
      `url "${url}" should not match pattern "http://example.com"`
    );
  }
});

add_task(async function test_matchURLPattern_stringPatterns() {
  const tests = [
    {
      pattern: "http://example.com",
      url: "http://example.com",
      match: true,
    },
    {
      pattern: "HTTP://example.com:80",
      url: "http://example.com",
      match: true,
    },
    {
      pattern: "http://example.com:80",
      url: "http://example.com",
      match: true,
    },
    {
      pattern: "http://example.com/path",
      url: "http://example.com/path",
      match: true,
    },
    {
      pattern: "http://example.com/PATH_CASE",
      url: "http://example.com/path_case",
      match: false,
    },
    {
      pattern: "http://example.com/path_single_segment",
      url: "http://example.com/path_single_segment/",
      match: false,
    },
    {
      pattern: "http://example.com/path",
      url: "http://example.com/path_continued",
      match: false,
    },
    {
      pattern: "http://example.com/path_two_segments/",
      url: "http://example.com/path_two_segments/",
      match: true,
    },
    {
      pattern: "http://example.com/path_two_segments/",
      url: "http://example.com/path_two_segments",
      match: false,
    },
    {
      pattern: "http://example.com/emptysearch?",
      url: "http://example.com/emptysearch?",
      match: true,
    },
    {
      pattern: "http://example.com/emptysearch?",
      url: "http://example.com/emptysearch",
      match: true,
    },
    {
      pattern: "http://example.com/emptysearch?",
      url: "http://example.com/emptysearch??",
      match: false,
    },
    {
      pattern: "http://example.com/emptysearch?",
      url: "http://example.com/emptysearch?a",
      match: false,
    },
    {
      pattern: "http://example.com/search?param",
      url: "http://example.com/search?param",
      match: true,
    },
    {
      pattern: "http://example.com/search?param",
      url: "http://example.com/search?param=value",
      match: false,
    },
    {
      pattern: "http://example.com/search?param=value",
      url: "http://example.com/search?param=value",
      match: true,
    },
    {
      pattern: "http://example.com/search?a=b&c=d",
      url: "http://example.com/search?a=b&c=d",
      match: true,
    },
    {
      pattern: "http://example.com/search?a=b&c=d",
      url: "http://example.com/search?c=d&a=b",
      match: false,
    },
    {
      pattern: "http://example.com/search?param",
      url: "http://example.com/search?param#ref",
      match: true,
    },
    {
      pattern: "http://example.com/search?param#ref",
      url: "http://example.com/search?param#ref",
      match: true,
    },
    {
      pattern: "http://example.com/search?param#ref",
      url: "http://example.com/search?param",
      match: true,
    },
    {
      pattern: "http://example.com/search?param",
      url: "http://example.com/search?parameter",
      match: false,
    },
    {
      pattern: "http://example.com/search?parameter",
      url: "http://example.com/search?param",
      match: false,
    },
    {
      pattern: "https://example.com:80",
      url: "https://example.com",
      match: false,
    },
    {
      pattern: "https://example.com:443",
      url: "https://example.com",
      match: true,
    },
    {
      pattern: "ws://example.com",
      url: "ws://example.com:80",
      match: true,
    },
  ];

  runMatchPatternTests(tests, "string");
});

add_task(async function test_patternPatterns_no_property() {
  const tests = [
    // Test protocol
    {
      pattern: {},
      url: "https://example.com",
      match: true,
    },
    {
      pattern: {},
      url: "https://example.com",
      match: true,
    },
    {
      pattern: {},
      url: "https://example.com:1234",
      match: true,
    },
    {
      pattern: {},
      url: "https://example.com/a",
      match: true,
    },
    {
      pattern: {},
      url: "https://example.com/a?test",
      match: true,
    },
  ];

  runMatchPatternTests(tests, "pattern");
});

add_task(async function test_patternPatterns_protocol() {
  const tests = [
    // Test protocol
    {
      pattern: {
        protocol: "http",
      },
      url: "http://example.com",
      match: true,
    },
    {
      pattern: {
        protocol: "HTTP",
      },
      url: "http://example.com",
      match: true,
    },
    {
      pattern: {
        protocol: "http",
      },
      url: "http://example.com:80",
      match: true,
    },
    {
      pattern: {
        protocol: "http",
      },
      url: "http://example.com:1234",
      match: true,
    },
    {
      pattern: {
        protocol: "http",
        port: "80",
      },
      url: "http://example.com:80",
      match: true,
    },
    {
      pattern: {
        protocol: "http",
        port: "1234",
      },
      url: "http://example.com:1234",
      match: true,
    },
    {
      pattern: {
        protocol: "http",
        port: "1234",
      },
      url: "http://example.com",
      match: false,
    },
    {
      pattern: {
        protocol: "http",
      },
      url: "https://wrong-scheme.com",
      match: false,
    },
    {
      pattern: {
        protocol: "http",
      },
      url: "http://whatever.com/?search#ref",
      match: true,
    },
    {
      pattern: {
        protocol: "http",
      },
      url: "http://example.com/a",
      match: true,
    },
    {
      pattern: {
        protocol: "http",
      },
      url: "http://whatever.com/path?search#ref",
      match: true,
    },
  ];

  runMatchPatternTests(tests, "pattern");
});

add_task(async function test_patternPatterns_port() {
  const tests = [
    {
      pattern: {
        protocol: "http",
        port: "80",
      },
      url: "http://abc.com/",
      match: true,
    },
    {
      pattern: {
        port: "1234",
      },
      url: "http://a.com:1234",
      match: true,
    },
    {
      pattern: {
        port: "1234",
      },
      url: "https://a.com:1234",
      match: true,
    },
  ];

  runMatchPatternTests(tests, "pattern");
});

add_task(async function test_patternPatterns_hostname() {
  const tests = [
    {
      pattern: {
        hostname: "example.com",
      },
      url: "http://example.com",
      match: true,
    },
    {
      pattern: {
        hostname: "example.com",
      },
      url: "http://example.com:80",
      match: true,
    },
    {
      pattern: {
        hostname: "example.com",
      },
      url: "https://example.com",
      match: true,
    },
    {
      pattern: {
        hostname: "example.com",
      },
      url: "https://example.com:443",
      match: true,
    },
    {
      pattern: {
        hostname: "example.com",
      },
      url: "ws://example.com",
      match: true,
    },
    {
      pattern: {
        hostname: "example.com",
      },
      url: "ws://example.com:80",
      match: true,
    },
    {
      pattern: {
        hostname: "example.com",
      },
      url: "http://example.com/path",
      match: true,
    },
    {
      pattern: {
        hostname: "example.com",
      },
      url: "http://example.com/?search",
      match: true,
    },
    {
      pattern: {
        hostname: "example\\{.com",
      },
      url: "http://example{.com/",
      match: true,
    },
    {
      pattern: {
        hostname: "example\\{.com",
      },
      url: "http://example\\{.com/",
      match: false,
    },
    {
      pattern: {
        hostname: "127.0.0.1",
      },
      url: "http://127.0.0.1/",
      match: true,
    },
    {
      pattern: {
        hostname: "127.0.0.1",
      },
      url: "http://127.0.0.2/",
      match: false,
    },
    {
      pattern: {
        hostname: "[2001:db8::1]",
      },
      url: "http://[2001:db8::1]/",
      match: true,
    },
    {
      pattern: {
        hostname: "[::AB:1]",
      },
      url: "http://[::ab:1]/",
      match: true,
    },
  ];

  runMatchPatternTests(tests, "pattern");
});

add_task(async function test_patternPatterns_pathname() {
  const tests = [
    {
      pattern: {
        pathname: "/",
      },
      url: "http://example.com",
      match: true,
    },
    {
      pattern: {
        pathname: "/",
      },
      url: "http://example.com/",
      match: true,
    },
    {
      pattern: {
        pathname: "/",
      },
      url: "http://example.com/?",
      match: true,
    },
    {
      pattern: {
        pathname: "path",
      },
      url: "http://example.com/path",
      match: true,
    },
    {
      pattern: {
        pathname: "/path",
      },
      url: "http://example.com/path",
      match: true,
    },
    {
      pattern: {
        pathname: "path",
      },
      url: "http://example.com/path/",
      match: false,
    },
    {
      pattern: {
        pathname: "path",
      },
      url: "http://example.com/path_continued",
      match: false,
    },
    {
      pattern: {
        pathname: "/",
      },
      url: "http://example.com/path",
      match: false,
    },
  ];

  runMatchPatternTests(tests, "pattern");
});

add_task(async function test_patternPatterns_search() {
  const tests = [
    {
      pattern: {
        search: "",
      },
      url: "http://example.com/?",
      match: true,
    },
    {
      pattern: {
        search: "",
      },
      url: "http://example.com/",
      match: true,
    },
    {
      pattern: {
        search: "",
      },
      url: "http://example.com/?#",
      match: true,
    },
    {
      pattern: {
        search: "?",
      },
      url: "http://example.com/?",
      match: true,
    },
    {
      pattern: {
        search: "?a",
      },
      url: "http://example.com/?a",
      match: true,
    },
    {
      pattern: {
        search: "?",
      },
      url: "http://example.com/??",
      match: false,
    },
    {
      pattern: {
        search: "query",
      },
      url: "http://example.com/?query",
      match: true,
    },
    {
      pattern: {
        search: "?query",
      },
      url: "http://example.com/?query",
      match: true,
    },
    {
      pattern: {
        search: "query=value",
      },
      url: "http://example.com/?query=value",
      match: true,
    },
    {
      pattern: {
        search: "query",
      },
      url: "http://example.com/?query=value",
      match: false,
    },
    {
      pattern: {
        search: "query",
      },
      url: "http://example.com/?query#value",
      match: true,
    },
  ];

  runMatchPatternTests(tests, "pattern");
});

function runMatchPatternTests(tests, type) {
  for (const test of tests) {
    let pattern;
    if (type == "pattern") {
      pattern = parseURLPattern({ type: "pattern", ...test.pattern });
    } else {
      pattern = parseURLPattern({ type: "string", pattern: test.pattern });
    }

    equal(
      matchURLPattern(pattern, test.url),
      test.match,
      `url "${test.url}" ${
        test.match ? "should" : "should not"
      } match pattern ${JSON.stringify(test.pattern)}`
    );
  }
}
