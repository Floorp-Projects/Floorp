/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  let testFlag = Services.telemetry.getHistogramById("TELEMETRY_TEST_FLAG");
  deepEqual(
    testFlag.snapshot().values,
    { 0: 1, 1: 0 },
    "Original value is correct"
  );
  testFlag.add(1);
  deepEqual(
    testFlag.snapshot().values,
    { 0: 0, 1: 1, 2: 0 },
    "Value is correct after ping"
  );
  testFlag.clear();
  deepEqual(
    testFlag.snapshot().values,
    { 0: 1, 1: 0 },
    "Value is correct after calling clear()"
  );
  testFlag.add(1);
  deepEqual(
    testFlag.snapshot().values,
    { 0: 0, 1: 1, 2: 0 },
    "Value is correct after ping"
  );
}
