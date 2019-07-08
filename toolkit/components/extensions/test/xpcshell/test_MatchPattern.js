/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(async function test_MatchPattern_matches() {
  function test(url, pattern, normalized = pattern, options = {}, explicit) {
    let uri = Services.io.newURI(url);

    pattern = Array.prototype.concat.call(pattern);
    normalized = Array.prototype.concat.call(normalized);

    let patterns = pattern.map(pat => new MatchPattern(pat, options));

    let set = new MatchPatternSet(pattern, options);
    let set2 = new MatchPatternSet(patterns, options);

    deepEqual(
      set2.patterns,
      patterns,
      "Patterns in set should equal the input patterns"
    );

    equal(
      set.matches(uri, explicit),
      set2.matches(uri, explicit),
      "Single pattern and pattern set should return the same match"
    );

    for (let [i, pat] of patterns.entries()) {
      equal(
        pat.pattern,
        normalized[i],
        "Pattern property should contain correct normalized pattern value"
      );
    }

    if (patterns.length == 1) {
      equal(
        patterns[0].matches(uri, explicit),
        set.matches(uri, explicit),
        "Single pattern and string set should return the same match"
      );
    }

    return set.matches(uri, explicit);
  }

  function pass({ url, pattern, normalized, options, explicit }) {
    ok(
      test(url, pattern, normalized, options, explicit),
      `Expected match: ${JSON.stringify(pattern)}, ${url}`
    );
  }

  function fail({ url, pattern, normalized, options, explicit }) {
    ok(
      !test(url, pattern, normalized, options, explicit),
      `Expected no match: ${JSON.stringify(pattern)}, ${url}`
    );
  }

  function invalid({ pattern }) {
    Assert.throws(
      () => new MatchPattern(pattern),
      /.*/,
      `Invalid pattern '${pattern}' should throw`
    );
    Assert.throws(
      () => new MatchPatternSet([pattern]),
      /.*/,
      `Invalid pattern '${pattern}' should throw`
    );
  }

  // Invalid pattern.
  invalid({ pattern: "" });

  // Pattern must include trailing slash.
  invalid({ pattern: "http://mozilla.org" });

  // Protocol not allowed.
  invalid({ pattern: "gopher://wuarchive.wustl.edu/" });

  pass({ url: "http://mozilla.org", pattern: "http://mozilla.org/" });
  pass({ url: "http://mozilla.org/", pattern: "http://mozilla.org/" });

  pass({ url: "http://mozilla.org/", pattern: "*://mozilla.org/" });
  pass({ url: "https://mozilla.org/", pattern: "*://mozilla.org/" });
  fail({ url: "file://mozilla.org/", pattern: "*://mozilla.org/" });
  fail({ url: "ftp://mozilla.org/", pattern: "*://mozilla.org/" });

  fail({ url: "http://mozilla.com", pattern: "http://*mozilla.com*/" });
  fail({ url: "http://mozilla.com", pattern: "http://mozilla.*/" });
  invalid({ pattern: "http:/mozilla.com/" });

  pass({ url: "http://google.com", pattern: "http://*.google.com/" });
  pass({ url: "http://docs.google.com", pattern: "http://*.google.com/" });

  pass({ url: "http://mozilla.org:8080", pattern: "http://mozilla.org/" });
  pass({ url: "http://mozilla.org:8080", pattern: "*://mozilla.org/" });
  fail({ url: "http://mozilla.org:8080", pattern: "http://mozilla.org:8080/" });

  // Now try with * in the path.
  pass({ url: "http://mozilla.org", pattern: "http://mozilla.org/*" });
  pass({ url: "http://mozilla.org/", pattern: "http://mozilla.org/*" });

  pass({ url: "http://mozilla.org/", pattern: "*://mozilla.org/*" });
  pass({ url: "https://mozilla.org/", pattern: "*://mozilla.org/*" });
  fail({ url: "file://mozilla.org/", pattern: "*://mozilla.org/*" });
  fail({ url: "http://mozilla.com", pattern: "http://mozilla.*/*" });

  pass({ url: "http://google.com", pattern: "http://*.google.com/*" });
  pass({ url: "http://docs.google.com", pattern: "http://*.google.com/*" });

  // Check path stuff.
  fail({ url: "http://mozilla.com/abc/def", pattern: "http://mozilla.com/" });
  pass({ url: "http://mozilla.com/abc/def", pattern: "http://mozilla.com/*" });
  pass({
    url: "http://mozilla.com/abc/def",
    pattern: "http://mozilla.com/a*f",
  });
  pass({ url: "http://mozilla.com/abc/def", pattern: "http://mozilla.com/a*" });
  pass({ url: "http://mozilla.com/abc/def", pattern: "http://mozilla.com/*f" });
  fail({ url: "http://mozilla.com/abc/def", pattern: "http://mozilla.com/*e" });
  fail({ url: "http://mozilla.com/abc/def", pattern: "http://mozilla.com/*c" });

  invalid({ pattern: "http:///a.html" });
  pass({ url: "file:///foo", pattern: "file:///foo*" });
  pass({ url: "file:///foo/bar.html", pattern: "file:///foo*" });

  pass({ url: "http://mozilla.org/a", pattern: "<all_urls>" });
  pass({ url: "https://mozilla.org/a", pattern: "<all_urls>" });
  pass({ url: "ftp://mozilla.org/a", pattern: "<all_urls>" });
  pass({ url: "file:///a", pattern: "<all_urls>" });
  fail({ url: "gopher://wuarchive.wustl.edu/a", pattern: "<all_urls>" });

  // Multiple patterns.
  pass({ url: "http://mozilla.org", pattern: ["http://mozilla.org/"] });
  pass({
    url: "http://mozilla.org",
    pattern: ["http://mozilla.org/", "http://mozilla.com/"],
  });
  pass({
    url: "http://mozilla.com",
    pattern: ["http://mozilla.org/", "http://mozilla.com/"],
  });
  fail({
    url: "http://mozilla.biz",
    pattern: ["http://mozilla.org/", "http://mozilla.com/"],
  });

  // Match url with fragments.
  pass({
    url: "http://mozilla.org/base#some-fragment",
    pattern: "http://mozilla.org/base",
  });

  // Match data:-URLs.
  pass({ url: "data:text/plain,foo", pattern: ["data:text/plain,foo"] });
  pass({ url: "data:text/plain,foo", pattern: ["data:text/plain,*"] });
  pass({
    url: "data:text/plain;charset=utf-8,foo",
    pattern: ["data:text/plain;charset=utf-8,foo"],
  });
  fail({
    url: "data:text/plain,foo",
    pattern: ["data:text/plain;charset=utf-8,foo"],
  });
  fail({
    url: "data:text/plain;charset=utf-8,foo",
    pattern: ["data:text/plain,foo"],
  });

  // Privileged matchers:
  invalid({ pattern: "about:foo" });
  invalid({ pattern: "resource://foo/*" });

  pass({
    url: "about:foo",
    pattern: ["about:foo", "about:foo*"],
    options: { restrictSchemes: false },
  });
  pass({
    url: "about:foo",
    pattern: ["about:foo*"],
    options: { restrictSchemes: false },
  });
  pass({
    url: "about:foobar",
    pattern: ["about:foo*"],
    options: { restrictSchemes: false },
  });

  pass({
    url: "resource://foo/bar",
    pattern: ["resource://foo/bar"],
    options: { restrictSchemes: false },
  });
  fail({
    url: "resource://fog/bar",
    pattern: ["resource://foo/bar"],
    options: { restrictSchemes: false },
  });
  fail({
    url: "about:foo",
    pattern: ["about:meh"],
    options: { restrictSchemes: false },
  });

  // Matchers for schemes without host should ignore ignorePath.
  pass({
    url: "about:reader?http://e.com/",
    pattern: ["about:reader*"],
    options: { ignorePath: true, restrictSchemes: false },
  });
  pass({ url: "data:,", pattern: ["data:,*"], options: { ignorePath: true } });

  // Matchers for schems without host should still match even if the explicit (host) flag is set.
  pass({
    url: "about:reader?explicit",
    pattern: ["about:reader*"],
    options: { restrictSchemes: false },
    explicit: true,
  });
  pass({
    url: "about:reader?explicit",
    pattern: ["about:reader?explicit"],
    options: { restrictSchemes: false },
    explicit: true,
  });
  pass({ url: "data:,explicit", pattern: ["data:,explicit"], explicit: true });
  pass({ url: "data:,explicit", pattern: ["data:,*"], explicit: true });

  // Matchers without "//" separator in the pattern.
  pass({ url: "data:text/plain;charset=utf-8,foo", pattern: ["data:*"] });
  pass({
    url: "about:blank",
    pattern: ["about:*"],
    options: { restrictSchemes: false },
  });
  pass({
    url: "view-source:https://example.com",
    pattern: ["view-source:*"],
    options: { restrictSchemes: false },
  });
  invalid({ pattern: ["chrome:*"], options: { restrictSchemes: false } });
  invalid({ pattern: "http:*" });

  // Matchers for unrecognized schemes.
  invalid({ pattern: "unknown-scheme:*" });
  pass({
    url: "unknown-scheme:foo",
    pattern: ["unknown-scheme:foo"],
    options: { restrictSchemes: false },
  });
  pass({
    url: "unknown-scheme:foo",
    pattern: ["unknown-scheme:*"],
    options: { restrictSchemes: false },
  });
  pass({
    url: "unknown-scheme://foo",
    pattern: ["unknown-scheme://foo"],
    options: { restrictSchemes: false },
  });
  pass({
    url: "unknown-scheme://foo",
    pattern: ["unknown-scheme://*"],
    options: { restrictSchemes: false },
  });
  pass({
    url: "unknown-scheme://foo",
    pattern: ["unknown-scheme:*"],
    options: { restrictSchemes: false },
  });
  fail({
    url: "unknown-scheme://foo",
    pattern: ["unknown-scheme:foo"],
    options: { restrictSchemes: false },
  });
  fail({
    url: "unknown-scheme:foo",
    pattern: ["unknown-scheme://foo"],
    options: { restrictSchemes: false },
  });
  fail({
    url: "unknown-scheme:foo",
    pattern: ["unknown-scheme://*"],
    options: { restrictSchemes: false },
  });

  // Matchers for IPv6
  pass({ url: "http://[::1]/", pattern: ["http://[::1]/"] });
  pass({
    url: "http://[2a03:4000:6:310e:216:3eff:fe53:99b]/",
    pattern: ["http://[2a03:4000:6:310e:216:3eff:fe53:99b]/"],
  });
  fail({
    url: "http://[2:4:6:3:2:3:f:b]/",
    pattern: ["http://[2a03:4000:6:310e:216:3eff:fe53:99b]/"],
  });

  // Before fixing Bug 1529230, the only way to match a specific IPv6 url is by droping the brackets in pattern,
  // thus we keep this pattern valid for the sake of backward compatibility
  pass({ url: "http://[::1]/", pattern: ["http://::1/"] });
  pass({
    url: "http://[2a03:4000:6:310e:216:3eff:fe53:99b]/",
    pattern: ["http://2a03:4000:6:310e:216:3eff:fe53:99b/"],
  });
});

add_task(async function test_MatchPattern_overlaps() {
  function test(filter, hosts, optional) {
    filter = Array.prototype.concat.call(filter);
    hosts = Array.prototype.concat.call(hosts);
    optional = Array.prototype.concat.call(optional);

    const set = new MatchPatternSet([...hosts, ...optional]);
    const pat = new MatchPatternSet(filter);
    return set.overlapsAll(pat);
  }

  function pass({ filter = [], hosts = [], optional = [] }) {
    ok(
      test(filter, hosts, optional),
      `Expected overlap: ${filter}, ${hosts} (${optional})`
    );
  }

  function fail({ filter = [], hosts = [], optional = [] }) {
    ok(
      !test(filter, hosts, optional),
      `Expected no overlap: ${filter}, ${hosts} (${optional})`
    );
  }

  // Direct comparison.
  pass({ hosts: "http://ab.cd/", filter: "http://ab.cd/" });
  fail({ hosts: "http://ab.cd/", filter: "ftp://ab.cd/" });

  // Wildcard protocol.
  pass({ hosts: "*://ab.cd/", filter: "https://ab.cd/" });
  fail({ hosts: "*://ab.cd/", filter: "ftp://ab.cd/" });

  // Wildcard subdomain.
  pass({ hosts: "http://*.ab.cd/", filter: "http://ab.cd/" });
  pass({ hosts: "http://*.ab.cd/", filter: "http://www.ab.cd/" });
  fail({ hosts: "http://*.ab.cd/", filter: "http://ab.cd.ef/" });
  fail({ hosts: "http://*.ab.cd/", filter: "http://www.cd/" });

  // Wildcard subsumed.
  pass({ hosts: "http://*.ab.cd/", filter: "http://*.cd/" });
  fail({ hosts: "http://*.cd/", filter: "http://*.xy/" });

  // Subdomain vs substring.
  fail({ hosts: "http://*.ab.cd/", filter: "http://fake-ab.cd/" });
  fail({ hosts: "http://*.ab.cd/", filter: "http://*.fake-ab.cd/" });

  // Wildcard domain.
  pass({ hosts: "http://*/", filter: "http://ab.cd/" });
  fail({ hosts: "http://*/", filter: "https://ab.cd/" });

  // Wildcard wildcards.
  pass({ hosts: "<all_urls>", filter: "ftp://ab.cd/" });
  fail({ hosts: "<all_urls>" });

  // Multiple hosts.
  pass({ hosts: ["http://ab.cd/"], filter: ["http://ab.cd/"] });
  pass({ hosts: ["http://ab.cd/", "http://ab.xy/"], filter: "http://ab.cd/" });
  pass({ hosts: ["http://ab.cd/", "http://ab.xy/"], filter: "http://ab.xy/" });
  fail({ hosts: ["http://ab.cd/", "http://ab.xy/"], filter: "http://ab.zz/" });

  // Multiple Multiples.
  pass({
    hosts: ["http://*.ab.cd/"],
    filter: ["http://ab.cd/", "http://www.ab.cd/"],
  });
  pass({
    hosts: ["http://ab.cd/", "http://ab.xy/"],
    filter: ["http://ab.cd/", "http://ab.xy/"],
  });
  fail({
    hosts: ["http://ab.cd/", "http://ab.xy/"],
    filter: ["http://ab.cd/", "http://ab.zz/"],
  });

  // Optional.
  pass({ hosts: [], optional: "http://ab.cd/", filter: "http://ab.cd/" });
  pass({
    hosts: "http://ab.cd/",
    optional: "http://ab.xy/",
    filter: ["http://ab.cd/", "http://ab.xy/"],
  });
  fail({
    hosts: "http://ab.cd/",
    optional: "https://ab.xy/",
    filter: "http://ab.xy/",
  });
});

add_task(async function test_MatchGlob() {
  function test(url, pattern) {
    let m = new MatchGlob(pattern[0]);
    return m.matches(Services.io.newURI(url).spec);
  }

  function pass({ url, pattern }) {
    ok(
      test(url, pattern),
      `Expected match: ${JSON.stringify(pattern)}, ${url}`
    );
  }

  function fail({ url, pattern }) {
    ok(
      !test(url, pattern),
      `Expected no match: ${JSON.stringify(pattern)}, ${url}`
    );
  }

  let moz = "http://mozilla.org";

  pass({ url: moz, pattern: ["*"] });
  pass({ url: moz, pattern: ["http://*"] });
  pass({ url: moz, pattern: ["*mozilla*"] });
  // pass({url: moz, pattern: ["*example*", "*mozilla*"]});

  pass({ url: moz, pattern: ["*://*"] });
  pass({ url: "https://mozilla.org", pattern: ["*://*"] });

  // Documentation example
  pass({
    url: "http://www.example.com/foo/bar",
    pattern: ["http://???.example.com/foo/*"],
  });
  pass({
    url: "http://the.example.com/foo/",
    pattern: ["http://???.example.com/foo/*"],
  });
  fail({
    url: "http://my.example.com/foo/bar",
    pattern: ["http://???.example.com/foo/*"],
  });
  fail({
    url: "http://example.com/foo/",
    pattern: ["http://???.example.com/foo/*"],
  });
  fail({
    url: "http://www.example.com/foo",
    pattern: ["http://???.example.com/foo/*"],
  });

  // Matches path
  let path = moz + "/abc/def";
  pass({ url: path, pattern: ["*def"] });
  pass({ url: path, pattern: ["*c/d*"] });
  pass({ url: path, pattern: ["*org/abc*"] });
  fail({ url: path + "/", pattern: ["*def"] });

  // Trailing slash
  pass({ url: moz, pattern: ["*.org/"] });
  fail({ url: moz, pattern: ["*.org"] });

  // Wrong TLD
  fail({ url: moz, pattern: ["*oz*.com/"] });
  // Case sensitive
  fail({ url: moz, pattern: ["*.ORG/"] });
});

add_task(async function test_MatchPattern_subsumes() {
  function test(oldPat, newPat) {
    let m = new MatchPatternSet(oldPat);
    return m.subsumes(new MatchPattern(newPat));
  }

  function pass({ oldPat, newPat }) {
    ok(test(oldPat, newPat), `${JSON.stringify(oldPat)} subsumes "${newPat}"`);
  }

  function fail({ oldPat, newPat }) {
    ok(
      !test(oldPat, newPat),
      `${JSON.stringify(oldPat)} doesn't subsume "${newPat}"`
    );
  }

  pass({ oldPat: ["<all_urls>"], newPat: "*://*/*" });
  pass({ oldPat: ["<all_urls>"], newPat: "http://*/*" });
  pass({ oldPat: ["<all_urls>"], newPat: "http://*.example.com/*" });

  pass({ oldPat: ["*://*/*"], newPat: "http://*/*" });
  pass({ oldPat: ["*://*/*"], newPat: "wss://*/*" });
  pass({ oldPat: ["*://*/*"], newPat: "http://*.example.com/*" });

  pass({ oldPat: ["*://*.example.com/*"], newPat: "http://*.example.com/*" });
  pass({ oldPat: ["*://*.example.com/*"], newPat: "*://sub.example.com/*" });

  pass({ oldPat: ["https://*/*"], newPat: "https://*.example.com/*" });
  pass({
    oldPat: ["http://*.example.com/*"],
    newPat: "http://subdomain.example.com/*",
  });
  pass({
    oldPat: ["http://*.sub.example.com/*"],
    newPat: "http://sub.example.com/*",
  });
  pass({
    oldPat: ["http://*.sub.example.com/*"],
    newPat: "http://sec.sub.example.com/*",
  });
  pass({
    oldPat: ["http://www.example.com/*"],
    newPat: "http://www.example.com/path/*",
  });
  pass({
    oldPat: ["http://www.example.com/path/*"],
    newPat: "http://www.example.com/*",
  });

  fail({ oldPat: ["*://*/*"], newPat: "<all_urls>" });
  fail({ oldPat: ["*://*/*"], newPat: "ftp://*/*" });
  fail({ oldPat: ["*://*/*"], newPat: "file://*/*" });

  fail({ oldPat: ["http://example.com/*"], newPat: "*://example.com/*" });
  fail({ oldPat: ["http://example.com/*"], newPat: "https://example.com/*" });
  fail({
    oldPat: ["http://example.com/*"],
    newPat: "http://otherexample.com/*",
  });
  fail({ oldPat: ["http://example.com/*"], newPat: "http://*.example.com/*" });
  fail({
    oldPat: ["http://example.com/*"],
    newPat: "http://subdomain.example.com/*",
  });

  fail({
    oldPat: ["http://subdomain.example.com/*"],
    newPat: "http://example.com/*",
  });
  fail({
    oldPat: ["http://subdomain.example.com/*"],
    newPat: "http://*.example.com/*",
  });
  fail({
    oldPat: ["http://sub.example.com/*"],
    newPat: "http://*.sub.example.com/*",
  });

  fail({ oldPat: ["ws://example.com/*"], newPat: "wss://example.com/*" });
  fail({ oldPat: ["http://example.com/*"], newPat: "ws://example.com/*" });
  fail({ oldPat: ["https://example.com/*"], newPat: "wss://example.com/*" });
});
