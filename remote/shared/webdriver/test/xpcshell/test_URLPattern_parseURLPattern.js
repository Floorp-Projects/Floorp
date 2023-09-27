/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { parseURLPattern } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/URLPattern.sys.mjs"
);

add_task(async function test_parseURLPattern_stringPatterns() {
  const STRING_PATTERN_TESTS = [
    {
      input: "http://example.com",
      protocol: "http",
      hostname: "example.com",
      port: "",
      pathname: "/",
      search: "",
    },
    {
      input: "http://example.com/",
      protocol: "http",
      hostname: "example.com",
      port: "",
      pathname: "/",
      search: "",
    },
    {
      input: "http://EXAMPLE.com",
      protocol: "http",
      hostname: "example.com",
      port: "",
      pathname: "/",
      search: "",
    },
    {
      input: "http://example%2Ecom",
      protocol: "http",
      hostname: "example.com",
      port: "",
      pathname: "/",
      search: "",
    },

    {
      input: "http://example.com:80",
      protocol: "http",
      hostname: "example.com",
      port: "",
      pathname: "/",
      search: "",
    },
    {
      input: "http://example.com:8888",
      protocol: "http",
      hostname: "example.com",
      port: "8888",
      pathname: "/",
      search: "",
    },
    {
      input: "http://example.com/a////b",
      protocol: "http",
      hostname: "example.com",
      port: "",
      pathname: "/a////b",
      search: "",
    },
    {
      input: "http://example.com/?",
      protocol: "http",
      hostname: "example.com",
      port: "",
      pathname: "/",
      search: "",
    },
    {
      input: "http://example.com/??",
      protocol: "http",
      hostname: "example.com",
      port: "",
      pathname: "/",
      search: "?",
    },
    {
      input: "http://example.com/?/",
      protocol: "http",
      hostname: "example.com",
      port: "",
      pathname: "/",
      search: "/",
    },
    {
      input: "file:///testfolder/test.zip",
      protocol: "file",
      hostname: "",
      port: null,
      pathname: "/testfolder/test.zip",
      search: "",
    },
    {
      input: "http://example\\{.com/",
      protocol: "http",
      hostname: "example{.com",
      port: "",
      pathname: "/",
      search: "",
    },
    {
      input: "http://[2001:db8::1]/",
      protocol: "http",
      hostname: "[2001:db8::1]",
      port: "",
      pathname: "/",
      search: "",
    },
    {
      input: "http://127.0.0.1/",
      protocol: "http",
      hostname: "127.0.0.1",
      port: "",
      pathname: "/",
      search: "",
    },
  ];

  for (const test of STRING_PATTERN_TESTS) {
    const pattern = parseURLPattern({
      type: "string",
      pattern: test.input,
    });

    equal(pattern.protocol, "protocol" in test ? test.protocol : null);
    equal(pattern.hostname, "hostname" in test ? test.hostname : null);
    equal(pattern.port, "port" in test ? test.port : null);
    equal(pattern.pathname, "pathname" in test ? test.pathname : null);
    equal(pattern.search, "search" in test ? test.search : null);
  }
});

add_task(async function test_parseURLPattern_patternPatterns() {
  const PATTERN_PATTERN_TESTS = [
    {
      pattern: {
        protocol: "http",
      },
      protocol: "http",
      hostname: null,
      port: null,
      pathname: null,
      search: null,
    },
    {
      pattern: {
        protocol: "HTTP",
      },
      protocol: "http",
      hostname: null,
      port: null,
      pathname: null,
      search: null,
    },
    {
      pattern: {
        hostname: "example.com",
      },
      protocol: null,
      hostname: "example.com",
      port: null,
      pathname: null,
      search: null,
    },
    {
      pattern: {
        hostname: "EXAMPLE.com",
      },
      protocol: null,
      hostname: "example.com",
      port: null,
      pathname: null,
      search: null,
    },
    {
      pattern: {
        hostname: "127.0.0.1",
      },
      protocol: null,
      hostname: "127.0.0.1",
      port: null,
      pathname: null,
      search: null,
    },
    {
      pattern: {
        hostname: "[2001:db8::1]",
      },
      protocol: null,
      hostname: "[2001:db8::1]",
      port: null,
      pathname: null,
      search: null,
    },
    {
      pattern: {
        port: "80",
      },
      protocol: null,
      hostname: null,
      port: "",
      pathname: null,
      search: null,
    },
    {
      pattern: {
        port: "1234",
      },
      protocol: null,
      hostname: null,
      port: "1234",
      pathname: null,
      search: null,
    },
    {
      pattern: {
        pathname: "path/to",
      },
      protocol: null,
      hostname: null,
      port: null,
      pathname: "/path/to",
      search: null,
    },
    {
      pattern: {
        pathname: "/path/to",
      },
      protocol: null,
      hostname: null,
      port: null,
      pathname: "/path/to",
      search: null,
    },
    {
      pattern: {
        pathname: "/path/to/",
      },
      protocol: null,
      hostname: null,
      port: null,
      pathname: "/path/to/",
      search: null,
    },
    {
      pattern: {
        search: "?search",
      },
      protocol: null,
      hostname: null,
      port: null,
      pathname: null,
      search: "search",
    },
    {
      pattern: {
        search: "search",
      },
      protocol: null,
      hostname: null,
      port: null,
      pathname: null,
      search: "search",
    },
    {
      pattern: {
        search: "?search=something",
      },
      protocol: null,
      hostname: null,
      port: null,
      pathname: null,
      search: "search=something",
    },
    {
      pattern: {
        search: "search=something",
      },
      protocol: null,
      hostname: null,
      port: null,
      pathname: null,
      search: "search=something",
    },
  ];

  for (const test of PATTERN_PATTERN_TESTS) {
    const pattern = parseURLPattern({
      type: "pattern",
      ...test.pattern,
    });

    equal(pattern.protocol, "protocol" in test ? test.protocol : null);
    equal(pattern.hostname, "hostname" in test ? test.hostname : null);
    equal(pattern.port, "port" in test ? test.port : null);
    equal(pattern.pathname, "pathname" in test ? test.pathname : null);
    equal(pattern.search, "search" in test ? test.search : null);
  }
});

add_task(async function test_parseURLPattern_invalid_type() {
  const values = [null, undefined, 1, [], "string"];
  for (const value of values) {
    Assert.throws(() => parseURLPattern(value), /InvalidArgumentError/);
  }
});

add_task(async function test_parseURLPattern_invalid_type_type() {
  const values = [null, undefined, 1, {}, []];
  for (const type of values) {
    Assert.throws(() => parseURLPattern({ type }), /InvalidArgumentError/);
  }
});

add_task(async function test_parseURLPattern_invalid_type_value() {
  const values = ["", "unknownType"];
  for (const type of values) {
    Assert.throws(() => parseURLPattern({ type }), /InvalidArgumentError/);
  }
});

add_task(async function test_parseURLPattern_invalid_stringPatternType() {
  const values = [null, undefined, 1, {}, []];
  for (const pattern of values) {
    Assert.throws(
      () => parseURLPattern({ type: "string", pattern }),
      /InvalidArgumentError/
    );
  }
});

add_task(async function test_parseURLPattern_invalid_stringPattern() {
  const values = [
    "foo",
    "*",
    "(",
    ")",
    "{",
    "}",
    "http\\{s\\}://example.com",
    "https://example.com:port/",
  ];
  for (const pattern of values) {
    Assert.throws(
      () => parseURLPattern({ type: "string", pattern }),
      /InvalidArgumentError/
    );
  }
});

add_task(async function test_parseURLPattern_invalid_patternPattern_type() {
  const properties = ["protocol", "hostname", "port", "pathname", "search"];
  const values = [false, 42, [], {}];
  for (const property of properties) {
    for (const value of values) {
      Assert.throws(
        () => parseURLPattern({ type: "pattern", [property]: value }),
        /InvalidArgumentError/
      );
    }
  }
});
