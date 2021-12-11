/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  Assert.ok(!Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());

  Services.profiler.StartProfiler(1000, 10, []);

  // Default: Active and not paused.
  Assert.ok(Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(!Services.profiler.IsSamplingPaused());

  // Pause everything, implicitly pauses sampling.
  Services.profiler.Pause();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  // While fully paused, pause and resume sampling only, no expected changes.
  Services.profiler.PauseSampling();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  Services.profiler.ResumeSampling();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  // Resume everything.
  Services.profiler.Resume();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(!Services.profiler.IsSamplingPaused());

  // Pause sampling only.
  Services.profiler.PauseSampling();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  // While sampling is paused, pause everything.
  Services.profiler.Pause();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  // Resume, but sampling is still paused separately.
  Services.profiler.Resume();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(Services.profiler.IsSamplingPaused());

  // Resume sampling only.
  Services.profiler.ResumeSampling();

  Assert.ok(Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(!Services.profiler.IsSamplingPaused());

  Services.profiler.StopProfiler();
  Assert.ok(!Services.profiler.IsActive());
  // Stopping is not pausing.
  Assert.ok(!Services.profiler.IsPaused());
  Assert.ok(!Services.profiler.IsSamplingPaused());

  do_test_finished();
}
