/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that we can measure non-zero event delays
 */

add_task(async () => {
  const entries = 10000;
  const interval = 1;
  const threads = [];
  const features = [];

  await Services.profiler.StartProfiler(entries, interval, features, threads);

  await functionA();

  const profile = await stopNowAndGetProfile();
  const [thread] = profile.threads;
  const { samples } = thread;
  const message = "eventDelay > 0 not found.";
  let SAMPLE_STACK_SLOT = thread.samples.schema.eventDelay;

  for (let i = 0; i < samples.data.length; i++) {
    if (samples.data[i][SAMPLE_STACK_SLOT] > 0) {
      Assert.ok(true, message);
      return;
    }
  }
  Assert.ok(false, message);
});

function doSyncWork(milliseconds) {
  const start = Date.now();
  while (true) {
    this.n = 0;
    for (let i = 0; i < 1e5; i++) {
      this.n += Math.random();
    }
    if (Date.now() - start > milliseconds) {
      return;
    }
  }
}

async function functionA() {
  doSyncWork(100);
  return captureAtLeastOneJsSample();
}
