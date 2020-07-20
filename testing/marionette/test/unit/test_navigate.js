/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
XPCOMUtils.defineLazyGlobalGetters(this, ["URL"]);

const { navigate } = ChromeUtils.import(
  "chrome://marionette/content/navigate.js"
);

add_test(function test_isLoadEventExpected() {
  Assert.throws(
    () => navigate.isLoadEventExpected(undefined),
    /Expected at least one URL/
  );

  const data = [
    { cur: "http://a/", fut: undefined, expected: true },
    { cur: "http://a/", fut: "http://a/", expected: true },
    { cur: "http://a/", fut: "http://a/#", expected: true },
    { cur: "http://a/#", fut: "http://a/", expected: true },
    { cur: "http://a/#a", fut: "http://a/#A", expected: true },
    { cur: "http://a/#a", fut: "http://a/#a", expected: false },
    { cur: "http://a/", fut: "javascript:whatever", expected: false },
  ];

  for (const entry of data) {
    const current = entry.cur ? new URL(entry.cur) : undefined;
    const future = entry.fut ? new URL(entry.fut) : undefined;
    equal(navigate.isLoadEventExpected(current, future), entry.expected);
  }

  run_next_test();
});
