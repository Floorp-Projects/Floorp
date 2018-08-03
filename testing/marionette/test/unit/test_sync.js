/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const {
  PollPromise,
  TimedPromise,
} = ChromeUtils.import("chrome://marionette/content/sync.js", {});

const DEFAULT_TIMEOUT = 2000;

add_test(function test_PollPromise_funcTypes() {
  for (let type of ["foo", 42, null, undefined, true, [], {}]) {
    Assert.throws(() => new PollPromise(type), /TypeError/);
  }
  new PollPromise(() => {});
  new PollPromise(function() {});

  run_next_test();
});

add_test(function test_PollPromise_timeoutTypes() {
  for (let timeout of ["foo", null, true, [], {}]) {
    Assert.throws(() => new PollPromise(() => {}, {timeout}), /TypeError/);
  }
  for (let timeout of [1.2, -1]) {
    Assert.throws(() => new PollPromise(() => {}, {timeout}), /RangeError/);
  }
  new PollPromise(() => {}, {timeout: 42});

  run_next_test();
});

add_test(function test_PollPromise_intervalTypes() {
  for (let interval of ["foo", null, true, [], {}]) {
    Assert.throws(() => new PollPromise(() => {}, {interval}), /TypeError/);
  }
  for (let interval of [1.2, -1]) {
    Assert.throws(() => new PollPromise(() => {}, {interval}), /RangeError/);
  }
  new PollPromise(() => {}, {interval: 42});

  run_next_test();
});

add_task(async function test_PollPromise_retvalTypes() {
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

add_test(function test_TimedPromise_funcTypes() {
  for (let type of ["foo", 42, null, undefined, true, [], {}]) {
    Assert.throws(() => new TimedPromise(type), /TypeError/);
  }
  new TimedPromise(resolve => resolve());
  new TimedPromise(function(resolve) { resolve(); });

  run_next_test();
});

add_test(function test_TimedPromise_timeoutTypes() {
  for (let timeout of ["foo", null, true, [], {}]) {
    Assert.throws(() => new TimedPromise(resolve => resolve(), {timeout}), /TypeError/);
  }
  for (let timeout of [1.2, -1]) {
    Assert.throws(() => new TimedPromise(resolve => resolve(), {timeout}), /RangeError/);
  }
  new TimedPromise(resolve => resolve(), {timeout: 42});

  run_next_test();
});
