/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_MatchPattern_matches() {
  function test(url, pattern, normalized = pattern) {
    let uri = Services.io.newURI(url);

    pattern = Array.concat(pattern);
    normalized = Array.concat(normalized);

    let patterns = pattern.map(pat => new MatchPattern(pat));

    let set = new MatchPatternSet(pattern);
    let set2 = new MatchPatternSet(patterns);

    deepEqual(set2.patterns, patterns, "Patterns in set should equal the input patterns");

    equal(set.matches(uri), set2.matches(uri), "Single pattern and pattern set should return the same match");

    for (let [i, pat] of patterns.entries()) {
      equal(pat.pattern, normalized[i], "Pattern property should contain correct normalized pattern value");
    }

    if (patterns.length == 1) {
      equal(patterns[0].matches(uri), set.matches(uri), "Single pattern and string set should return the same match");
    }

    return set.matches(uri);
  }

  function pass({url, pattern, normalized}) {
    ok(test(url, pattern, normalized), `Expected match: ${JSON.stringify(pattern)}, ${url}`);
  }

  function fail({url, pattern, normalized}) {
    ok(!test(url, pattern, normalized), `Expected no match: ${JSON.stringify(pattern)}, ${url}`);
  }

  function invalid({pattern}) {
    Assert.throws(() => new MatchPattern(pattern), /.*/,
                  `Invalid pattern '${pattern}' should throw`);
    Assert.throws(() => new MatchPatternSet([pattern]), /.*/,
                  `Invalid pattern '${pattern}' should throw`);
  }

  // Invalid pattern.
  invalid({pattern: ""});

  // Pattern must include trailing slash.
  invalid({pattern: "http://mozilla.org"});

  // Protocol not allowed.
  invalid({pattern: "gopher://wuarchive.wustl.edu/"});

  pass({url: "http://mozilla.org", pattern: "http://mozilla.org/"});
  pass({url: "http://mozilla.org/", pattern: "http://mozilla.org/"});

  pass({url: "http://mozilla.org/", pattern: "*://mozilla.org/"});
  pass({url: "https://mozilla.org/", pattern: "*://mozilla.org/"});
  fail({url: "file://mozilla.org/", pattern: "*://mozilla.org/"});
  fail({url: "ftp://mozilla.org/", pattern: "*://mozilla.org/"});

  fail({url: "http://mozilla.com", pattern: "http://*mozilla.com*/"});
  fail({url: "http://mozilla.com", pattern: "http://mozilla.*/"});
  invalid({pattern: "http:/mozilla.com/"});

  pass({url: "http://google.com", pattern: "http://*.google.com/"});
  pass({url: "http://docs.google.com", pattern: "http://*.google.com/"});

  pass({url: "http://mozilla.org:8080", pattern: "http://mozilla.org/"});
  pass({url: "http://mozilla.org:8080", pattern: "*://mozilla.org/"});
  fail({url: "http://mozilla.org:8080", pattern: "http://mozilla.org:8080/"});

  // Now try with * in the path.
  pass({url: "http://mozilla.org", pattern: "http://mozilla.org/*"});
  pass({url: "http://mozilla.org/", pattern: "http://mozilla.org/*"});

  pass({url: "http://mozilla.org/", pattern: "*://mozilla.org/*"});
  pass({url: "https://mozilla.org/", pattern: "*://mozilla.org/*"});
  fail({url: "file://mozilla.org/", pattern: "*://mozilla.org/*"});
  fail({url: "http://mozilla.com", pattern: "http://mozilla.*/*"});

  pass({url: "http://google.com", pattern: "http://*.google.com/*"});
  pass({url: "http://docs.google.com", pattern: "http://*.google.com/*"});

  // Check path stuff.
  fail({url: "http://mozilla.com/abc/def", pattern: "http://mozilla.com/"});
  pass({url: "http://mozilla.com/abc/def", pattern: "http://mozilla.com/*"});
  pass({url: "http://mozilla.com/abc/def", pattern: "http://mozilla.com/a*f"});
  pass({url: "http://mozilla.com/abc/def", pattern: "http://mozilla.com/a*"});
  pass({url: "http://mozilla.com/abc/def", pattern: "http://mozilla.com/*f"});
  fail({url: "http://mozilla.com/abc/def", pattern: "http://mozilla.com/*e"});
  fail({url: "http://mozilla.com/abc/def", pattern: "http://mozilla.com/*c"});

  invalid({pattern: "http:///a.html"});
  pass({url: "file:///foo", pattern: "file:///foo*"});
  pass({url: "file:///foo/bar.html", pattern: "file:///foo*"});

  pass({url: "http://mozilla.org/a", pattern: "<all_urls>"});
  pass({url: "https://mozilla.org/a", pattern: "<all_urls>"});
  pass({url: "ftp://mozilla.org/a", pattern: "<all_urls>"});
  pass({url: "file:///a", pattern: "<all_urls>"});
  fail({url: "gopher://wuarchive.wustl.edu/a", pattern: "<all_urls>"});

  // Multiple patterns.
  pass({url: "http://mozilla.org", pattern: ["http://mozilla.org/"]});
  pass({url: "http://mozilla.org", pattern: ["http://mozilla.org/", "http://mozilla.com/"]});
  pass({url: "http://mozilla.com", pattern: ["http://mozilla.org/", "http://mozilla.com/"]});
  fail({url: "http://mozilla.biz", pattern: ["http://mozilla.org/", "http://mozilla.com/"]});

  // Match url with fragments.
  pass({url: "http://mozilla.org/base#some-fragment", pattern: "http://mozilla.org/base"});
});

add_task(async function test_MatchPattern_overlaps() {
  function test(filter, hosts, optional) {
    filter = Array.concat(filter);
    hosts = Array.concat(hosts);
    optional = Array.concat(optional);

    const set = new MatchPatternSet([...hosts, ...optional]);
    const pat = new MatchPatternSet(filter);
    return set.overlapsAll(pat);
  }

  function pass({filter = [], hosts = [], optional = []}) {
    ok(test(filter, hosts, optional), `Expected overlap: ${filter}, ${hosts} (${optional})`);
  }

  function fail({filter = [], hosts = [], optional = []}) {
    ok(!test(filter, hosts, optional), `Expected no overlap: ${filter}, ${hosts} (${optional})`);
  }

  // Direct comparison.
  pass({hosts: "http://ab.cd/", filter: "http://ab.cd/"});
  fail({hosts: "http://ab.cd/", filter: "ftp://ab.cd/"});

  // Wildcard protocol.
  pass({hosts: "*://ab.cd/", filter: "https://ab.cd/"});
  fail({hosts: "*://ab.cd/", filter: "ftp://ab.cd/"});

  // Wildcard subdomain.
  pass({hosts: "http://*.ab.cd/", filter: "http://ab.cd/"});
  pass({hosts: "http://*.ab.cd/", filter: "http://www.ab.cd/"});
  fail({hosts: "http://*.ab.cd/", filter: "http://ab.cd.ef/"});
  fail({hosts: "http://*.ab.cd/", filter: "http://www.cd/"});

  // Wildcard subsumed.
  pass({hosts: "http://*.ab.cd/", filter: "http://*.cd/"});
  fail({hosts: "http://*.cd/", filter: "http://*.xy/"});

  // Subdomain vs substring.
  fail({hosts: "http://*.ab.cd/", filter: "http://fake-ab.cd/"});
  fail({hosts: "http://*.ab.cd/", filter: "http://*.fake-ab.cd/"});

  // Wildcard domain.
  pass({hosts: "http://*/", filter: "http://ab.cd/"});
  fail({hosts: "http://*/", filter: "https://ab.cd/"});

  // Wildcard wildcards.
  pass({hosts: "<all_urls>", filter: "ftp://ab.cd/"});
  fail({hosts: "<all_urls>"});

  // Multiple hosts.
  pass({hosts: ["http://ab.cd/"], filter: ["http://ab.cd/"]});
  pass({hosts: ["http://ab.cd/", "http://ab.xy/"], filter: "http://ab.cd/"});
  pass({hosts: ["http://ab.cd/", "http://ab.xy/"], filter: "http://ab.xy/"});
  fail({hosts: ["http://ab.cd/", "http://ab.xy/"], filter: "http://ab.zz/"});

  // Multiple Multiples.
  pass({hosts: ["http://*.ab.cd/"], filter: ["http://ab.cd/", "http://www.ab.cd/"]});
  pass({hosts: ["http://ab.cd/", "http://ab.xy/"], filter: ["http://ab.cd/", "http://ab.xy/"]});
  fail({hosts: ["http://ab.cd/", "http://ab.xy/"], filter: ["http://ab.cd/", "http://ab.zz/"]});

  // Optional.
  pass({hosts: [], optional: "http://ab.cd/", filter: "http://ab.cd/"});
  pass({hosts: "http://ab.cd/", optional: "http://ab.xy/", filter: ["http://ab.cd/", "http://ab.xy/"]});
  fail({hosts: "http://ab.cd/", optional: "https://ab.xy/", filter: "http://ab.xy/"});
});

add_task(async function test_MatchGlob() {
  function test(url, pattern) {
    let m = new MatchGlob(pattern[0]);
    return m.matches(Services.io.newURI(url).spec);
  }

  function pass({url, pattern}) {
    ok(test(url, pattern), `Expected match: ${JSON.stringify(pattern)}, ${url}`);
  }

  function fail({url, pattern}) {
    ok(!test(url, pattern), `Expected no match: ${JSON.stringify(pattern)}, ${url}`);
  }

  let moz = "http://mozilla.org";

  pass({url: moz, pattern: ["*"]});
  pass({url: moz, pattern: ["http://*"]});
  pass({url: moz, pattern: ["*mozilla*"]});
  // pass({url: moz, pattern: ["*example*", "*mozilla*"]});

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
  fail({url: moz, pattern: ["*oz*.com/"]});
  // Case sensitive
  fail({url: moz, pattern: ["*.ORG/"]});
});
