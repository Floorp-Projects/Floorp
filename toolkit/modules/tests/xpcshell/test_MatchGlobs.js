/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Components.utils.import("resource://gre/modules/MatchPattern.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

function test(url, pattern) {
  let uri = Services.io.newURI(url);
  let m = new MatchGlobs(pattern);
  return m.matches(uri.spec);
}

function pass({url, pattern}) {
  ok(test(url, pattern), `Expected match: ${JSON.stringify(pattern)}, ${url}`);
}

function fail({url, pattern}) {
  ok(!test(url, pattern), `Expected no match: ${JSON.stringify(pattern)}, ${url}`);
}

function run_test() {
  let moz = "http://mozilla.org";

  pass({url: moz, pattern: ["*"]});
  pass({url: moz, pattern: ["http://*"]}),
  pass({url: moz, pattern: ["*mozilla*"]});
  pass({url: moz, pattern: ["*example*", "*mozilla*"]});

  pass({url: moz, pattern: ["*://*"]});
  pass({url: "https://mozilla.org", pattern: ["*://*"]});

  // Documentation example
  pass({url: "http://www.example.com/foo/bar", pattern: ["http://???.example.com/foo/*"]});
  pass({url: "http://the.example.com/foo/", pattern: ["http://???.example.com/foo/*"]});
  fail({url: "http://my.example.com/foo/bar", pattern: ["http://???.example.com/foo/*"]});
  fail({url: "http://example.com/foo/", pattern: ["http://???.example.com/foo/*"]});
  fail({url: "http://www.example.com/foo", pattern: ["http://???.example.com/foo/*"]});

  // Matches path
  let path = moz + "/abc/def";
  pass({url: path, pattern: ["*def"]});
  pass({url: path, pattern: ["*c/d*"]});
  pass({url: path, pattern: ["*org/abc*"]});
  fail({url: path + "/", pattern: ["*def"]});

  // Trailing slash
  pass({url: moz, pattern: ["*.org/"]});
  fail({url: moz, pattern: ["*.org"]});

  // Wrong TLD
  fail({url: moz, pattern: ["www*.m*.com/"]});
  // Case sensitive
  fail({url: moz, pattern: ["*.ORG/"]});

  fail({url: moz, pattern: []});
}
