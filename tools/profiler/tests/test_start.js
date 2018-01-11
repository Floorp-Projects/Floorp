/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  if (!AppConstants.MOZ_GECKO_PROFILER) {
    return;
  }

  Assert.ok(!Services.profiler.IsActive());

  Services.profiler.StartProfiler(10, 100, [], 0);

  Assert.ok(Services.profiler.IsActive());

  Services.profiler.StopProfiler();

  Assert.ok(!Services.profiler.IsActive());
}
