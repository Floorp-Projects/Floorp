/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { AnimationFramePromise, PollPromise } = ChromeUtils.import(
  "chrome://remote/content/shared/Sync.jsm"
);

add_task(async function test_AnimationFramePromise() {
  let called = false;
  let win = {
    requestAnimationFrame(callback) {
      called = true;
      callback();
    },
  };
  await AnimationFramePromise(win);
  ok(called);
});

add_task(async function test_AnimationFramePromiseAbortWhenWindowClosed() {
  let win = {
    closed: true,
    requestAnimationFrame() {},
  };
  await AnimationFramePromise(win);
});

add_test(function test_executeSoon_callback() {
  // executeSoon() is already defined for xpcshell in head.js. As such import
  // our implementation into a custom namespace.
  let sync = {};
  ChromeUtils.import("chrome://remote/content/shared/Sync.jsm", sync);

  for (let func of ["foo", null, true, [], {}]) {
    Assert.throws(() => sync.executeSoon(func), /TypeError/);
  }

  let a;
  sync.executeSoon(() => {
    a = 1;
  });
  executeSoon(() => equal(1, a));

  run_next_test();
});

add_test(function test_PollPromise_funcTypes() {
  for (let type of ["foo", 42, null, undefined, true, [], {}]) {
    Assert.throws(() => new PollPromise(type), /TypeError/);
  }
  new PollPromise(() => {});
  new PollPromise(function() {});

  run_next_test();
});

add_test(function test_PollPromise_timeoutTypes() {
  for (let timeout of ["foo", true, [], {}]) {
    Assert.throws(() => new PollPromise(() => {}, { timeout }), /TypeError/);
  }
  for (let timeout of [1.2, -1]) {
    Assert.throws(() => new PollPromise(() => {}, { timeout }), /RangeError/);
  }
  for (let timeout of [null, undefined, 42]) {
    new PollPromise(resolve => resolve(1), { timeout });
  }

  run_next_test();
});

add_test(function test_PollPromise_intervalTypes() {
  for (let interval of ["foo", null, true, [], {}]) {
    Assert.throws(() => new PollPromise(() => {}, { interval }), /TypeError/);
  }
  for (let interval of [1.2, -1]) {
    Assert.throws(() => new PollPromise(() => {}, { interval }), /RangeError/);
  }
  new PollPromise(() => {}, { interval: 42 });

  run_next_test();
});

add_task(async function test_PollPromise_retvalTypes() {
  for (let typ of [true, false, "foo", 42, [], {}]) {
    strictEqual(typ, await new PollPromise(resolve => resolve(typ)));
  }
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
  let nevals = 0;
  await new PollPromise((resolve, reject) => {
    ++nevals;
    nevals < 100 ? reject() : resolve();
  });
  equal(100, nevals);
});

add_task(async function test_PollPromise_zeroTimeout() {
  // run at least once when timeout is 0
  let nevals = 0;
  let start = new Date().getTime();
  await new PollPromise(
    (resolve, reject) => {
      ++nevals;
      reject();
    },
    { timeout: 0 }
  );
  let end = new Date().getTime();
  equal(1, nevals);
  less(end - start, 500);
});

add_task(async function test_PollPromise_timeoutElapse() {
  let nevals = 0;
  let start = new Date().getTime();
  await new PollPromise(
    (resolve, reject) => {
      ++nevals;
      reject();
    },
    { timeout: 100 }
  );
  let end = new Date().getTime();
  lessOrEqual(nevals, 11);
  greaterOrEqual(end - start, 100);
});

add_task(async function test_PollPromise_interval() {
  let nevals = 0;
  await new PollPromise(
    (resolve, reject) => {
      ++nevals;
      reject();
    },
    { timeout: 100, interval: 100 }
  );
  equal(2, nevals);
});
