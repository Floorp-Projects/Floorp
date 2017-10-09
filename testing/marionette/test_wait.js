/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {utils: Cu} = Components;

Cu.import("chrome://marionette/content/wait.js");

add_task(async function test_until_types() {
  for (let typ of [true, false, "foo", 42, [], {}]) {
    strictEqual(typ, await wait.until(resolve => resolve(typ)));
  }
});

add_task(async function test_until_timeoutElapse() {
  let nevals = 0;
  let start = new Date().getTime();
  await wait.until((resolve, reject) => {
    ++nevals;
    reject();
  });
  let end = new Date().getTime();
  greaterOrEqual((end - start), 2000);
  greaterOrEqual(nevals, 15);
});

add_task(async function test_until_rethrowError() {
  let nevals = 0;
  let err;
  try {
    await wait.until(() => {
      ++nevals;
      throw new Error();
    });
  } catch (e) {
    err = e;
  }
  equal(1, nevals);
  ok(err instanceof Error);
});

add_task(async function test_until_noTimeout() {
  // run at least once when timeout is 0
  let nevals = 0;
  let start = new Date().getTime();
  await wait.until((resolve, reject) => {
    ++nevals;
    reject();
  }, 0);
  let end = new Date().getTime();
  equal(1, nevals);
  less((end - start), 2000);
});

add_task(async function test_until_timeout() {
  let nevals = 0;
  let start = new Date().getTime();
  await wait.until((resolve, reject) => {
    ++nevals;
    reject();
  }, 100);
  let end = new Date().getTime();
  greater(nevals, 1);
  greaterOrEqual((end - start), 100);
});

add_task(async function test_until_interval() {
  let nevals = 0;
  await wait.until((resolve, reject) => {
    ++nevals;
    reject();
  }, 100, 100);
  equal(2, nevals);
});
