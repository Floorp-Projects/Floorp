// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals } from "@std/assert";
import type {
  HubPanelMenuM3eCaseResult,
} from "./hub_panel_menu_m3e_acceptance.types.ts";
import type { MutableHubPanelMenuM3eVerdictInput } from "./hub_panel_menu_m3e_verdict.test.types.ts";
import {
  createHubPanelMenuM3eCaseResults,
  HUB_PANEL_MENU_M3E_FINAL_CLEANUP_CASE_ID,
  HUB_PANEL_MENU_M3E_FUNCTIONAL_CASE_IDS,
  HUB_PANEL_MENU_M3E_REQUIRED_CASE_IDS,
  hubPanelMenuM3eVerdict,
} from "./hub_panel_menu_m3e_verdict.ts";

function passingInput(): MutableHubPanelMenuM3eVerdictInput {
  const cases: HubPanelMenuM3eCaseResult[] = createHubPanelMenuM3eCaseResults()
    .map((result) => ({
      ...result,
      status: "passed" as const,
      startedAt: "start",
      completedAt: "done",
    }));
  const snapshot = {
    exists: true,
    entries: 1,
    byteLength: 1,
    sha256: "a".repeat(64),
  };
  return {
    complete: true,
    runFailure: null,
    cases,
    launchFailures: [],
    sharedProfileBefore: snapshot,
    sharedProfileAfter: snapshot,
    cleanup: {
      launchOwnershipProven: true,
      ownedProcessesStopped: true,
      ownedPortsClosed: true,
      fixtureClosed: true,
      fixtureObservationsBounded: true,
      profilesRemoved: true,
      sharedProfileUnchanged: true,
      ownedCommandCleanupProven: true,
      reportWritten: true,
    },
  };
}

Deno.test("M3e verdict accepts each required case exactly once", () => {
  const input = passingInput();
  assertEquals(
    HUB_PANEL_MENU_M3E_FUNCTIONAL_CASE_IDS.length,
    HUB_PANEL_MENU_M3E_REQUIRED_CASE_IDS.length - 1,
  );
  assertEquals(
    HUB_PANEL_MENU_M3E_FUNCTIONAL_CASE_IDS.join("|").includes(
      HUB_PANEL_MENU_M3E_FINAL_CLEANUP_CASE_ID,
    ),
    false,
  );
  assertEquals(
    new Set(HUB_PANEL_MENU_M3E_REQUIRED_CASE_IDS).size,
    HUB_PANEL_MENU_M3E_REQUIRED_CASE_IDS.length,
  );
  assertEquals(input.cases.map((result) => result.id), [
    ...HUB_PANEL_MENU_M3E_REQUIRED_CASE_IDS,
  ]);
  assert(hubPanelMenuM3eVerdict(input));
});

Deno.test("M3e verdict fails closed for missing, duplicate, or not-run cases", () => {
  const missing = passingInput();
  missing.cases.pop();
  assertEquals(hubPanelMenuM3eVerdict(missing), false);

  const duplicate = passingInput();
  duplicate.cases.push(duplicate.cases[0]);
  assertEquals(hubPanelMenuM3eVerdict(duplicate), false);

  const notRun = passingInput();
  notRun.cases[3] = { ...notRun.cases[3], status: "not-run" };
  assertEquals(hubPanelMenuM3eVerdict(notRun), false);
});

Deno.test("M3e verdict rejects launch failures and incomplete cleanup", () => {
  const failedLaunch = passingInput();
  failedLaunch.launchFailures.push({ category: "launch", kind: "Error" });
  assertEquals(hubPanelMenuM3eVerdict(failedLaunch), false);

  const incompleteCleanup = passingInput();
  incompleteCleanup.cleanup.ownedPortsClosed = false;
  assertEquals(hubPanelMenuM3eVerdict(incompleteCleanup), false);

  const missingCleanup = passingInput();
  delete (missingCleanup.cleanup as Partial<typeof missingCleanup.cleanup>)
    .ownedPortsClosed;
  assertEquals(hubPanelMenuM3eVerdict(missingCleanup), false);

  const nullCleanup = passingInput();
  (nullCleanup.cleanup as unknown as Record<string, unknown>).ownedPortsClosed =
    null;
  assertEquals(hubPanelMenuM3eVerdict(nullCleanup), false);

  const unknownCleanup = passingInput();
  (unknownCleanup.cleanup as unknown as Record<string, boolean>).unrelated =
    true;
  assertEquals(hubPanelMenuM3eVerdict(unknownCleanup), false);

  const failedRun = passingInput();
  failedRun.runFailure = { category: "run", kind: "Error" };
  assertEquals(hubPanelMenuM3eVerdict(failedRun), false);

  const setupFailedBeforeStart = passingInput();
  setupFailedBeforeStart.runFailure = { category: "run", kind: "Error" };
  assertEquals(hubPanelMenuM3eVerdict(setupFailedBeforeStart), false);
});
