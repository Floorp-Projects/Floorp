/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Components.utils.import("resource://gre/modules/MatchPattern.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

function test(url, pattern)
{
  let uri = Services.io.newURI(url, null, null);
  let m = new MatchPattern(pattern);
  return m.matches(uri);
}

function pass({url, pattern})
{
  do_check_true(test(url, pattern), `Expected match: ${JSON.stringify(pattern)}, ${url}`);
}

function fail({url, pattern})
{
  do_check_false(test(url, pattern), `Expected no match: ${JSON.stringify(pattern)}, ${url}`);
}

function run_test()
{
  // Invalid pattern.
  fail({url:"http://mozilla.org", pattern:""});

  // Pattern must include trailing slash.
  fail({url:"http://mozilla.org", pattern:"http://mozilla.org"});

  // Protocol not allowed.
  fail({url:"http://mozilla.org", pattern:"gopher://wuarchive.wustl.edu/"});

  pass({url:"http://mozilla.org", pattern:"http://mozilla.org/"});
  pass({url:"http://mozilla.org/", pattern:"http://mozilla.org/"});

  pass({url:"http://mozilla.org/", pattern:"*://mozilla.org/"});
  pass({url:"https://mozilla.org/", pattern:"*://mozilla.org/"});
  fail({url:"file://mozilla.org/", pattern:"*://mozilla.org/"});
  fail({url:"ftp://mozilla.org/", pattern:"*://mozilla.org/"});

  fail({url:"http://mozilla.com", pattern:"http://*mozilla.com*/"});
  fail({url:"http://mozilla.com", pattern:"http://mozilla.*/"});
  fail({url:"http://mozilla.com", pattern:"http:/mozilla.com/"});

  pass({url:"http://google.com", pattern:"http://*.google.com/"});
  pass({url:"http://docs.google.com", pattern:"http://*.google.com/"});

  pass({url:"http://mozilla.org:8080", pattern:"http://mozilla.org/"});
  pass({url:"http://mozilla.org:8080", pattern:"*://mozilla.org/"});
  fail({url:"http://mozilla.org:8080", pattern:"http://mozilla.org:8080/"});

  // Now try with * in the path.
  pass({url:"http://mozilla.org", pattern:"http://mozilla.org/*"});
  pass({url:"http://mozilla.org/", pattern:"http://mozilla.org/*"});

  pass({url:"http://mozilla.org/", pattern:"*://mozilla.org/*"});
  pass({url:"https://mozilla.org/", pattern:"*://mozilla.org/*"});
  fail({url:"file://mozilla.org/", pattern:"*://mozilla.org/*"});
  fail({url:"http://mozilla.com", pattern:"http://mozilla.*/*"});

  pass({url:"http://google.com", pattern:"http://*.google.com/*"});
  pass({url:"http://docs.google.com", pattern:"http://*.google.com/*"});

  // Check path stuff.
  fail({url:"http://mozilla.com/abc/def", pattern:"http://mozilla.com/"});
  pass({url:"http://mozilla.com/abc/def", pattern:"http://mozilla.com/*"});
  pass({url:"http://mozilla.com/abc/def", pattern:"http://mozilla.com/a*f"});
  pass({url:"http://mozilla.com/abc/def", pattern:"http://mozilla.com/a*"});
  pass({url:"http://mozilla.com/abc/def", pattern:"http://mozilla.com/*f"});
  fail({url:"http://mozilla.com/abc/def", pattern:"http://mozilla.com/*e"});
  fail({url:"http://mozilla.com/abc/def", pattern:"http://mozilla.com/*c"});

  fail({url:"http:///a.html", pattern:"http:///a.html"});
  pass({url:"file:///foo", pattern:"file:///foo*"});
  pass({url:"file:///foo/bar.html", pattern:"file:///foo*"});

  pass({url:"http://mozilla.org/a", pattern:"<all_urls>"});
  pass({url:"https://mozilla.org/a", pattern:"<all_urls>"});
  pass({url:"ftp://mozilla.org/a", pattern:"<all_urls>"});
  pass({url:"file:///a", pattern:"<all_urls>"});
  fail({url:"gopher://wuarchive.wustl.edu/a", pattern:"<all_urls>"});

  // Multiple patterns.
  pass({url:"http://mozilla.org", pattern:["http://mozilla.org/"]});
  pass({url:"http://mozilla.org", pattern:["http://mozilla.org/", "http://mozilla.com/"]});
  pass({url:"http://mozilla.com", pattern:["http://mozilla.org/", "http://mozilla.com/"]});
  fail({url:"http://mozilla.biz", pattern:["http://mozilla.org/", "http://mozilla.com/"]});
}
