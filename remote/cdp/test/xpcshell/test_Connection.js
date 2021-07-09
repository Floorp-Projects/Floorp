/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { splitMethod } = ChromeUtils.import(
  "chrome://remote/content/cdp/CDPConnection.jsm"
);

add_test(function test_Connection_splitMethod() {
  for (const t of [42, null, true, {}, [], undefined]) {
    Assert.throws(() => splitMethod(t), /TypeError/, `${typeof t} throws`);
  }
  for (const s of ["", ".", "foo.", ".bar", "foo.bar.baz"]) {
    Assert.throws(
      () => splitMethod(s),
      /Invalid method format: '.*'/,
      `"${s}" throws`
    );
  }
  deepEqual(splitMethod("foo.bar"), {
    domain: "foo",
    command: "bar",
  });

  run_next_test();
});
