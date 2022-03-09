/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async () => {
  Assert.ok(!Services.profiler.IsActive());

  let startPromise = Services.profiler.StartProfiler(10, 100, []);

  Assert.ok(Services.profiler.IsActive());

  await startPromise;
  Assert.ok(Services.profiler.IsActive());

  let stopPromise = Services.profiler.StopProfiler();

  Assert.ok(!Services.profiler.IsActive());

  await stopPromise;
  Assert.ok(!Services.profiler.IsActive());
});
