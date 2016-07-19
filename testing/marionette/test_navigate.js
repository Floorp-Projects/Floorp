/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {utils: Cu} = Components;

Cu.importGlobalProperties(["URL"]);

Cu.import("chrome://marionette/content/navigate.js");

add_test(function test_isLoadEventExpected() {
  Assert.throws(() => navigate.isLoadEventExpected(undefined),
      /Expected at least one URL/);

  equal(true, navigate.isLoadEventExpected("http://a/"));
  equal(true, navigate.isLoadEventExpected("http://a/", "http://a/"));
  equal(true, navigate.isLoadEventExpected("http://a/", "http://a/b"));
  equal(true, navigate.isLoadEventExpected("http://a/", "http://b"));
  equal(true, navigate.isLoadEventExpected("http://a/", "data:text/html;charset=utf-8,foo"));
  equal(true, navigate.isLoadEventExpected("about:blank", "http://a/"));
  equal(true, navigate.isLoadEventExpected("http://a/", "about:blank"));
  equal(true, navigate.isLoadEventExpected("http://a/", "https://a/"));

  equal(false, navigate.isLoadEventExpected("http://a/", "javascript:whatever"));
  equal(false, navigate.isLoadEventExpected("http://a/", "http://a/#"));
  equal(false, navigate.isLoadEventExpected("http://a/", "http://a/#b"));
  equal(false, navigate.isLoadEventExpected("http://a/#b", "http://a/#b"));
  equal(false, navigate.isLoadEventExpected("http://a/#b", "http://a/#c"));
  equal(false, navigate.isLoadEventExpected("data:text/html;charset=utf-8,foo", "data:text/html;charset=utf-8,foo#bar"));
  equal(false, navigate.isLoadEventExpected("data:text/html;charset=utf-8,foo", "data:text/html;charset=utf-8,foo#"));

  run_next_test();
});

add_test(function test_IdempotentURL() {
  Assert.throws(() => new navigate.IdempotentURL(undefined));
  Assert.throws(() => new navigate.IdempotentURL(true));
  Assert.throws(() => new navigate.IdempotentURL({}));
  Assert.throws(() => new navigate.IdempotentURL(42));

  // propagated URL properties
  let u1 = new URL("http://a/b");
  let u2 = new navigate.IdempotentURL(u1);
  equal(u1.host, u2.host);
  equal(u1.hostname, u2.hostname);
  equal(u1.href, u2.href);
  equal(u1.origin, u2.origin);
  equal(u1.password, u2.password);
  equal(u1.port, u2.port);
  equal(u1.protocol, u2.protocol);
  equal(u1.search, u2.search);
  equal(u1.username, u2.username);

  // specialisations
  equal("#b", new navigate.IdempotentURL("http://a/#b").hash);
  equal("#", new navigate.IdempotentURL("http://a/#").hash);
  equal("", new navigate.IdempotentURL("http://a/").hash);
  equal("#bar", new navigate.IdempotentURL("data:text/html;charset=utf-8,foo#bar").hash);
  equal("#", new navigate.IdempotentURL("data:text/html;charset=utf-8,foo#").hash);
  equal("", new navigate.IdempotentURL("data:text/html;charset=utf-8,foo").hash);

  equal("/", new navigate.IdempotentURL("http://a/").pathname);
  equal("/", new navigate.IdempotentURL("http://a/#b").pathname);
  equal("text/html;charset=utf-8,foo", new navigate.IdempotentURL("data:text/html;charset=utf-8,foo#bar").pathname);

  run_next_test();
});
