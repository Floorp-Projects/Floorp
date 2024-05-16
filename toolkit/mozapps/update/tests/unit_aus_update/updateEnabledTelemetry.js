/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

"use strict";
const { TelemetryEnvironment } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryEnvironment.sys.mjs"
);

/**
 * This test ensures that packaged installations don't send an `update.enabled`
 * value of `true` in the telemetry environment.
 */

add_task(async function telemetryEnvironmentUpdateEnabled() {
  const origSysinfo = Services.sysinfo;
  registerCleanupFunction(() => {
    Services.sysinfo = origSysinfo;
  });

  const mockSysinfo = Object.create(origSysinfo, {
    getProperty: {
      configurable: true,
      enumerable: true,
      writable: false,
      value: prop => {
        if (prop == "isPackagedApp") {
          return true;
        }
        return origSysinfo.getProperty(prop);
      },
    },
  });
  Services.sysinfo = mockSysinfo;

  const environmentData = await TelemetryEnvironment.onInitialized();
  Assert.equal(
    environmentData.settings.update.enabled,
    false,
    "Update should not be reported as enabled in a packaged app"
  );
});
