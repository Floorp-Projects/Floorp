/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Check time conversion utils.
 */

add_task(async function toDate() {
  Assert.throws(() => PlacesUtils.toDate(), /Invalid value/, "Should throw");
  Assert.throws(() => PlacesUtils.toDate(NaN), /Invalid value/, "Should throw");
  Assert.throws(
    () => PlacesUtils.toDate(null),
    /Invalid value/,
    "Should throw"
  );
  Assert.throws(() => PlacesUtils.toDate("1"), /Invalid value/, "Should throw");

  const now = Date.now();
  const usecs = now * 1000;
  Assert.deepEqual(PlacesUtils.toDate(usecs), new Date(now));
});

add_task(async function toPRTime() {
  Assert.throws(() => PlacesUtils.toPRTime(), /TypeError/, "Should throw");
  Assert.throws(() => PlacesUtils.toPRTime(null), /TypeError/, "Should throw");
  Assert.throws(
    () => PlacesUtils.toPRTime({}),
    /Invalid value/,
    "Should throw"
  );
  Assert.throws(
    () => PlacesUtils.toPRTime(NaN),
    /Invalid value/,
    "Should throw"
  );
  Assert.throws(
    () => PlacesUtils.toPRTime(new Date(NaN)),
    /Invalid value/,
    "Should throw"
  );
  Assert.throws(
    () => PlacesUtils.toPRTime(new URL("https://test.moz")),
    /Invalid value/,
    "Should throw"
  );

  const now = Date.now();
  const usecs = now * 1000;
  Assert.strictEqual(PlacesUtils.toPRTime(now), usecs);
  Assert.strictEqual(PlacesUtils.toPRTime(new Date(now)), usecs);
});
