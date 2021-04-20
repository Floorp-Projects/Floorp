/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const { AnimationFramePromise, PollPromise } = ChromeUtils.import(
  "chrome://remote/content/shared/Sync.jsm"
);

/**
 * Mimic a DOM node for listening for events.
 */
class MockElement {
  constructor() {
    this.capture = false;
    this.func = null;
    this.eventName = null;
    this.untrusted = false;
  }

  addEventListener(name, func, capture, untrusted) {
    this.eventName = name;
    this.func = func;
    if (capture != null) {
      this.capture = capture;
    }
    if (untrusted != null) {
      this.untrusted = untrusted;
    }
  }

  click() {
    if (this.func) {
      let details = {
        capture: this.capture,
        target: this,
        type: this.eventName,
        untrusted: this.untrusted,
      };
      this.func(details);
    }
  }

  removeEventListener(name, func) {
    this.capture = false;
    this.func = null;
    this.eventName = null;
    this.untrusted = false;
  }
}

/**
 * Mimic a message manager for sending messages.
 */
class MessageManager {
  constructor() {
    this.func = null;
    this.message = null;
  }

  addMessageListener(message, func) {
    this.func = func;
    this.message = message;
  }

  removeMessageListener(message) {
    this.func = null;
    this.message = null;
  }

  send(message, data) {
    if (this.func) {
      this.func({
        data,
        message,
        target: this,
      });
    }
  }
}

/**
 * Mimics nsITimer, but instead of using a system clock you can
 * preprogram it to invoke the callback after a given number of ticks.
 */
class MockTimer {
  constructor(ticksBeforeFiring) {
    this.goal = ticksBeforeFiring;
    this.ticks = 0;
    this.cancelled = false;
  }

  initWithCallback(cb, timeout, type) {
    this.ticks++;
    if (this.ticks >= this.goal) {
      cb();
    }
  }

  cancel() {
    this.cancelled = true;
  }
}

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
