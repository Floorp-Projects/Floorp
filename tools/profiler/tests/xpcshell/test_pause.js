/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async () => {
  Assert.ok(!Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());

  let startPromise = Services.profiler.StartProfiler(1000, 10, []);

  // Default: Active and not paused.
  Assert.ok(Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(!Services.profiler.IsSamplingPaused());

  await startPromise;
  Assert.ok(Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(!Services.profiler.IsSamplingPaused());

  // Pause everything, implicitly pauses sampling.
  let pausePromise = Services.profiler.Pause();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  await pausePromise;
  Assert.ok(Services.profiler.IsActive());
  Assert.ok(Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  // While fully paused, pause and resume sampling only, no expected changes.
  let pauseSamplingPromise = Services.profiler.PauseSampling();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  await pauseSamplingPromise;
  Assert.ok(Services.profiler.IsActive());
  Assert.ok(Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  let resumeSamplingPromise = Services.profiler.ResumeSampling();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  await resumeSamplingPromise;
  Assert.ok(Services.profiler.IsActive());
  Assert.ok(Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  // Resume everything.
  let resumePromise = Services.profiler.Resume();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(!Services.profiler.IsSamplingPaused());

  await resumePromise;
  Assert.ok(Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(!Services.profiler.IsSamplingPaused());

  // Pause sampling only.
  let pauseSampling2Promise = Services.profiler.PauseSampling();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  await pauseSampling2Promise;
  Assert.ok(Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  // While sampling is paused, pause everything.
  let pause2Promise = Services.profiler.Pause();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  await pause2Promise;
  Assert.ok(Services.profiler.IsActive());
  Assert.ok(Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  // Resume, but sampling is still paused separately.
  let resume2promise = Services.profiler.Resume();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  await resume2promise;
  Assert.ok(Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  // Resume sampling only.
  let resumeSampling2Promise = Services.profiler.ResumeSampling();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(!Services.profiler.IsSamplingPaused());

  await resumeSampling2Promise;
  Assert.ok(Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(!Services.profiler.IsSamplingPaused());

  let stopPromise = Services.profiler.StopProfiler();
  Assert.ok(!Services.profiler.IsActive());
  // Stopping is not pausing.
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(!Services.profiler.IsSamplingPaused());

  await stopPromise;
  Assert.ok(!Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(!Services.profiler.IsSamplingPaused());
});
