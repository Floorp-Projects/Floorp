/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that job scheduler orders the jobs correctly.
 */

let { JobScheduler } = devtools.require("devtools/server/actors/tracer");

function run_test()
{
  do_test_pending();

  test_in_order();
  test_shuffled();

  do_timeout(0, do_test_finished);
}

function test_in_order() {
  let jobScheduler = new JobScheduler();
  let testArray = [0];

  let first = jobScheduler.schedule();
  let second = jobScheduler.schedule();
  let third = jobScheduler.schedule();

  first(() => testArray.push(1));
  second(() => testArray.push(2));
  third(() => testArray.push(3));

  do_timeout(0, () => {
    do_check_eq(testArray[0], 0);
    do_check_eq(testArray[1], 1);
    do_check_eq(testArray[2], 2);
    do_check_eq(testArray[3], 3);
  });
}

function test_shuffled() {
  let jobScheduler = new JobScheduler();
  let testArray = [0];

  let first = jobScheduler.schedule();
  let second = jobScheduler.schedule();
  let third = jobScheduler.schedule();

  third(() => testArray.push(3));
  first(() => testArray.push(1));
  second(() => testArray.push(2));

  do_timeout(0, () => {
    do_check_eq(testArray[0], 0);
    do_check_eq(testArray[1], 1);
    do_check_eq(testArray[2], 2);
    do_check_eq(testArray[3], 3);
  });
}
