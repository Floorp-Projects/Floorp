/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* global BrowserTestUtils, ok, gBrowser, add_task */

"use strict";

/**
 * Checks that we set the CPUMicrocodeVersion annotation.
 */
add_task(async function test_cpu_microcode_version_annotation() {
  await BrowserTestUtils.withNewTab(
    {
      gBrowser,
    },
    async function (browser) {
      // Crash the tab
      let annotations = await BrowserTestUtils.crashFrame(browser);

      ok(
        "CPUMicrocodeVersion" in annotations,
        "contains CPU microcode version"
      );
    }
  );
});
