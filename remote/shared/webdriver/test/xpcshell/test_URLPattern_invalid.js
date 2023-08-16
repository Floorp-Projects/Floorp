/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { parseURLPattern } = ChromeUtils.importESModule(
  "chrome://remote/content/shared/webdriver/URLPattern.sys.mjs"
);

add_task(
  async function test_parseURLPattern_patternPattern_unescapedCharacters() {
    const properties = ["protocol", "hostname", "port", "pathname", "search"];
    const values = ["*", "(", ")", "{", "}"];
    for (const property of properties) {
      for (const value of values) {
        Assert.throws(
          () => parseURLPattern({ type: "pattern", [property]: value }),
          /InvalidArgumentError/
        );
      }
    }
  }
);

add_task(async function test_parseURLPattern_patternPattern_protocol() {
  const values = [
    "",
    "http/",
    "http\\*",
    "http\\(",
    "http\\)",
    "http\\{",
    "http\\}",
    "http#",
    "http@",
    "http%",
  ];
  for (const value of values) {
    Assert.throws(
      () => parseURLPattern({ type: "pattern", protocol: value }),
      /InvalidArgumentError/
    );
  }
});

add_task(
  async function test_parseURLPattern_patternPattern_unsupported_protocol() {
    const values = ["ftp", "abc", "webpack"];
    for (const value of values) {
      Assert.throws(
        () => parseURLPattern({ type: "pattern", protocol: value }),
        /UnsupportedOperationError/
      );
    }
  }
);

add_task(async function test_parseURLPattern_patternPattern_hostname() {
  const values = ["", "abc/com/", "abc?com", "abc#com", "abc:com"];
  for (const value of values) {
    Assert.throws(
      () => parseURLPattern({ type: "pattern", hostname: value }),
      /InvalidArgumentError/
    );
  }
});

add_task(async function test_parseURLPattern_patternPattern_port() {
  const values = ["", "abcd", "-1", "80 ", "1.3", ":80", "65536"];
  for (const value of values) {
    Assert.throws(
      () => parseURLPattern({ type: "pattern", port: value }),
      /InvalidArgumentError/
    );
  }
});

add_task(async function test_parseURLPattern_patternPattern_pathname() {
  const values = ["path?", "path#"];
  for (const value of values) {
    Assert.throws(
      () => parseURLPattern({ type: "pattern", pathname: value }),
      /InvalidArgumentError/
    );
  }
});

add_task(async function test_parseURLPattern_patternPattern_search() {
  const values = ["search#"];
  for (const value of values) {
    Assert.throws(
      () => parseURLPattern({ type: "pattern", search: value }),
      /InvalidArgumentError/
    );
  }
});

add_task(async function test_parseURLPattern_stringPattern_invalid_url() {
  const values = ["", "invalid", "http:invalid:url", "[1::", "127.0..1"];
  for (const value of values) {
    Assert.throws(
      () => parseURLPattern({ type: "string", pattern: value }),
      /InvalidArgumentError/
    );
  }
});

add_task(
  async function test_parseURLPattern_stringPattern_unescaped_characters() {
    const values = ["*", "(", ")", "{", "}"];
    for (const value of values) {
      Assert.throws(
        () => parseURLPattern({ type: "string", pattern: value }),
        /InvalidArgumentError/
      );
    }
  }
);

add_task(
  async function test_parseURLPattern_stringPattern_unsupported_protocol() {
    const values = ["ftp://some/path", "abc:pathplaceholder", "webpack://test"];
    for (const value of values) {
      Assert.throws(
        () => parseURLPattern({ type: "string", pattern: value }),
        /UnsupportedOperationError/
      );
    }
  }
);
