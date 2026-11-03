// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
/// <reference path="../../../browser-features/chrome/@types/xul-window.d.ts" />
/// <reference path="../../../libs/@types/gecko/index.d.ts" />
import {
  assert,
  assertEquals,
  runTests,
} from "../../../browser-features/chrome/test/utils/test_harness.ts";
import { runAllTests as workspacesEnabled } from "../../../browser-features/chrome/common/workspaces/test/nativeLastTabWorkspacesEnabled.test.ts";
import { runAllTests as disabledKeepOpen } from "../../../browser-features/chrome/common/workspaces/test/nativeLastTabDisabledKeepOpen.test.ts";
import { runAllTests as disabledClose } from "../../../browser-features/chrome/common/workspaces/test/nativeLastTabDisabledClose.test.ts";
import { waitFor } from "../../../browser-features/chrome/common/workspaces/test/workspace-last-tab-test-utils.ts";
import type { ProductionWorkspaceWindow } from "./workspace-regression-types.ts";

export async function runAllTests(): Promise<void> {
  const owner = Services.wm.getMostRecentWindow("navigator:browser") as
    | ProductionWorkspaceWindow
    | null;
  assert(owner && !owner.closed, "Actual owning browser window is available");
  assert(
    typeof owner.OpenBrowserWindow === "function",
    "Actual browser exports its native window-opening command",
  );
  await waitFor(
    () =>
      Boolean(owner.gBrowserInit?.delayedStartupFinished) &&
      Services.prefs.getStringPref("nora.startup.mode", "") === "production",
    "Actual packaged Floorp production startup completes native window initialization",
    30_000,
  );
  assertEquals(
    Services.prefs.getStringPref("nora.startup.mode", ""),
    "production",
    "Workspace regression uses actual Floorp production startup",
  );
  assertEquals(
    Services.prefs.getStringPref("nora.startup.error", ""),
    "",
    "Actual production startup reports no initialization error",
  );
  assert(
    !Services.prefs.getBoolPref("nora.dev.allow_http_loader", false),
    "Production workspace regression does not enable the HTTP development loader",
  );

  // The runtime suite's module global is not the browser window. Adapt this one
  // missing harness command to the actual owning window; the existing suites
  // create real windows and restore their own workspace/native preferences.
  const previous = Object.getOwnPropertyDescriptor(
    globalThis,
    "OpenBrowserWindow",
  );
  assert(
    !previous || previous.configurable || previous.writable,
    "The suite can temporarily delegate its window-opening command",
  );
  Object.defineProperty(globalThis, "OpenBrowserWindow", {
    ...(previous && !previous.configurable ? previous : {
      configurable: true,
      enumerable: previous?.enumerable ?? false,
      writable: true,
    }),
    value: () => owner.OpenBrowserWindow(),
  });
  try {
    // Reuse all14 existing real-window regression cases, collecting failures
    // across the three suites without changing their behavior or assertions.
    await runTests("ProductionWorkspaces.test.mts", [
      {
        name: "existing workspaces-enabled native last-tab cases",
        fn: workspacesEnabled,
      },
      {
        name: "existing disabled keep-open native last-tab cases",
        fn: disabledKeepOpen,
      },
      {
        name: "existing disabled close-window native last-tab cases",
        fn: disabledClose,
      },
    ]);
  } finally {
    if (previous) {
      Object.defineProperty(globalThis, "OpenBrowserWindow", previous);
    } else {assert(
        Reflect.deleteProperty(globalThis, "OpenBrowserWindow"),
        "Temporary native window-opening delegate is removed",
      );}
  }
}
