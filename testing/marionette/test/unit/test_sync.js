/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {PollPromise} = ChromeUtils.import("chrome://marionette/content/sync.js", {});

const DEFAULT_TIMEOUT = 2000;

add_task(async function test_PollPromise_types() {
  for (let typ of [true, false, "foo", 42, [], {}]) {
    strictEqual(typ, await new PollPromise(resolve => resolve(typ)));
  }
});

add_task(async function test_PollPromise_timeoutElapse() {
  let nevals = 0;
  let start = new Date().getTime();
  await new PollPromise((resolve, reject) => {
    ++nevals;
    reject();
  });
  let end = new Date().getTime();
  greaterOrEqual((end - start), DEFAULT_TIMEOUT);
  greaterOrEqual(nevals, 15);
});

add_task(async function test_PollPromise_rethrowError() {
  let nevals = 0;
  let err;
  try {
    await PollPromise(() => {
      ++nevals;
      throw new Error();
    });
  } catch (e) {
    err = e;
  }
  equal(1, nevals);
  ok(err instanceof Error);
});

add_task(async function test_PollPromise_noTimeout() {
  // run at least once when timeout is 0
  let nevals = 0;
  let start = new Date().getTime();
  await new PollPromise((resolve, reject) => {
    ++nevals;
    reject();
  }, {timeout: 0});
  let end = new Date().getTime();
  equal(1, nevals);
  less((end - start), DEFAULT_TIMEOUT);
});

add_task(async function test_PollPromise_timeout() {
  let nevals = 0;
  let start = new Date().getTime();
  await new PollPromise((resolve, reject) => {
    ++nevals;
    reject();
  }, {timeout: 100});
  let end = new Date().getTime();
  greater(nevals, 1);
  greaterOrEqual((end - start), 100);
});

add_task(async function test_PollPromise_interval() {
  let nevals = 0;
  await new PollPromise((resolve, reject) => {
    ++nevals;
    reject();
  }, {timeout: 100, interval: 100});
  equal(2, nevals);
});
