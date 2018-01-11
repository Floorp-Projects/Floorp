/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  if (!AppConstants.MOZ_GECKO_PROFILER) {
    return;
  }

  Assert.ok(!Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());

  Services.profiler.StartProfiler(1000, 10, [], 0);

  Assert.ok(Services.profiler.IsActive());

  Services.profiler.PauseSampling();

  Assert.ok(Services.profiler.IsPaused());

  Services.profiler.ResumeSampling();

  Assert.ok(!Services.profiler.IsPaused());

  Services.profiler.StopProfiler();
  Assert.ok(!Services.profiler.IsActive());
  Assert.ok(!Services.profiler.IsPaused());
  do_test_finished();
}
