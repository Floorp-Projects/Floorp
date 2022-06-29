/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { navigate } = ChromeUtils.import(
  "chrome://remote/content/marionette/navigate.js"
);

const mockTopContext = {
  get children() {
    return [mockNestedContext];
  },
  id: 7,
  get top() {
    return this;
  },
};

const mockNestedContext = {
  id: 8,
  parent: mockTopContext,
  top: mockTopContext,
};

add_test(function test_isLoadEventExpectedForCurrent() {
  Assert.throws(
    () => navigate.isLoadEventExpected(undefined),
    /Expected at least one URL/
  );

  ok(navigate.isLoadEventExpected(new URL("http://a/")));

  run_next_test();
});

add_test(function test_isLoadEventExpectedForFuture() {
  const data = [
    { current: "http://a/", future: undefined, expected: true },
    { current: "http://a/", future: "http://a/", expected: true },
    { current: "http://a/", future: "http://a/#", expected: true },
    { current: "http://a/#", future: "http://a/", expected: true },
    { current: "http://a/#a", future: "http://a/#A", expected: true },
    { current: "http://a/#a", future: "http://a/#a", expected: false },
    { current: "http://a/", future: "javascript:whatever", expected: false },
  ];

  for (const entry of data) {
    const current = new URL(entry.current);
    const future = entry.future ? new URL(entry.future) : undefined;
    equal(navigate.isLoadEventExpected(current, { future }), entry.expected);
  }

  run_next_test();
});

add_test(function test_isLoadEventExpectedForTarget() {
  for (const target of ["_parent", "_top"]) {
    Assert.throws(
      () => navigate.isLoadEventExpected(new URL("http://a"), { target }),
      /Expected browsingContext when target is _parent or _top/
    );
  }

  const data = [
    { cur: "http://a/", target: "", expected: true },
    { cur: "http://a/", target: "_blank", expected: false },
    { cur: "http://a/", target: "_parent", bc: mockTopContext, expected: true },
    {
      cur: "http://a/",
      target: "_parent",
      bc: mockNestedContext,
      expected: false,
    },
    { cur: "http://a/", target: "_self", expected: true },
    { cur: "http://a/", target: "_top", bc: mockTopContext, expected: true },
    {
      cur: "http://a/",
      target: "_top",
      bc: mockNestedContext,
      expected: false,
    },
  ];

  for (const entry of data) {
    const current = entry.cur ? new URL(entry.cur) : undefined;
    equal(
      navigate.isLoadEventExpected(current, {
        target: entry.target,
        browsingContext: entry.bc,
      }),
      entry.expected
    );
  }

  run_next_test();
});
