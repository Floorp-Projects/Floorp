/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://services-common/utils.js");

function run_test() {
    Assert.equal(null, CommonUtils.ensureMillisecondsTimestamp(null));
    Assert.equal(null, CommonUtils.ensureMillisecondsTimestamp(0));
    Assert.equal(null, CommonUtils.ensureMillisecondsTimestamp("0"));
    Assert.equal(null, CommonUtils.ensureMillisecondsTimestamp("000"));

    Assert.equal(null, CommonUtils.ensureMillisecondsTimestamp(999 * 10000000000));

    do_check_throws(function err() { CommonUtils.ensureMillisecondsTimestamp(-1); });
    do_check_throws(function err() { CommonUtils.ensureMillisecondsTimestamp(1); });
    do_check_throws(function err() { CommonUtils.ensureMillisecondsTimestamp(1.5); });
    do_check_throws(function err() { CommonUtils.ensureMillisecondsTimestamp(999 * 10000000000 + 0.5); });

    do_check_throws(function err() { CommonUtils.ensureMillisecondsTimestamp("-1"); });
    do_check_throws(function err() { CommonUtils.ensureMillisecondsTimestamp("1"); });
    do_check_throws(function err() { CommonUtils.ensureMillisecondsTimestamp("1.5"); });
    do_check_throws(function err() { CommonUtils.ensureMillisecondsTimestamp("" + (999 * 10000000000 + 0.5)); });
}
