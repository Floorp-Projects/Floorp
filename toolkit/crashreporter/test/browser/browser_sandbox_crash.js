/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* global BrowserTestUtils, ok, gBrowser, add_task */

"use strict";

/**
 * Checks that we set the CPUMicrocodeVersion annotation.
 * This is a Windows-specific crash annotation.
 */
if (AppConstants.platform == "win") {
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
}

/**
 * Checks that Linux sandbox violations are reported as SIGSYS crashes with
 * the syscall number provided in the address field of the minidump's exception
 * stream.
 */
if (AppConstants.platform == "linux") {
  add_task(async function test_sandbox_violation_is_sigsys() {
    await BrowserTestUtils.withNewTab(
      {
        gBrowser,
      },
      async function (browser) {
        // Crash the tab
        let annotations = await BrowserTestUtils.crashFrame(
          browser,
          true,
          true,
          /* Default browsing context */ null,
          { crashType: "CRASH_SYSCALL" }
        );

        Assert.equal(
          annotations.StackTraces.crash_info.type,
          "SIGSYS",
          "The crash type is SIGSYS"
        );

        function chroot_syscall_number() {
          // We crash by calling chroot(), see BrowserTestUtilsChild.sys.mjs
          switch (Services.sysinfo.get("arch")) {
            case "x86-64":
              return "0xa1";
            case "aarch64":
              return "0x33";
            default:
              return "0x3d";
          }
        }

        Assert.equal(
          annotations.StackTraces.crash_info.address,
          chroot_syscall_number(),
          "The address corresponds to the chroot() syscall number"
        );
      }
    );
  });
}
