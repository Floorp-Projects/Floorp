// SPDX-License-Identifier: MPL-2.0

import type {
  HubPanelMenuM3eCaseResult,
  HubPanelMenuM3eCleanup,
  HubPanelMenuM3eProfileSnapshot,
  HubPanelMenuM3eVerdictInput,
} from "./hub_panel_menu_m3e_acceptance.types.ts";

export const HUB_PANEL_MENU_M3E_REQUIRED_CASE_IDS = [
  "startup-pinned",
  "startup-legacy-control",
  "startup-manual",
  "windows-private",
  "empty-acquisition-inert",
  "active-acquisition-inert",
  "stage-install-disabled",
  "enabled-command-opens-hub",
  "update-v2-generation-fencing",
  "new-window-v2-only",
  "rollback-retains-high-water",
  "broken-v3-restores-active-generation",
  "disable-reenable-close",
  "focus-restoration",
  "focus-pointer-cancellation",
  "focus-keyboard-cancellation",
  "focus-movement-cancellation",
  "focus-blur-cancellation",
  "focus-menu-close-cancellation",
  "focus-disable-cancellation",
  "focus-navigation-cancellation",
  "focus-window-close-intent-cancellation",
  "focus-bounded-expiry",
  "close-window-during-operation",
  "close-host-during-operation",
  "reopen-enabled-reverified",
  "reopen-disabled-no-evaluation",
  "reopen-corrupt-with-verified-rollback",
  "reopen-corrupt-without-eligible-recovery",
  "shared-profile-owned-cleanup",
] as const;

export const HUB_PANEL_MENU_M3E_FINAL_CLEANUP_CASE_ID =
  "shared-profile-owned-cleanup" as const;
export const HUB_PANEL_MENU_M3E_FUNCTIONAL_CASE_IDS =
  HUB_PANEL_MENU_M3E_REQUIRED_CASE_IDS.filter((id) =>
    id !== HUB_PANEL_MENU_M3E_FINAL_CLEANUP_CASE_ID
  );

export const HUB_PANEL_MENU_M3E_REQUIRED_CLEANUP_FIELDS = [
  "launchOwnershipProven",
  "ownedProcessesStopped",
  "ownedPortsClosed",
  "fixtureClosed",
  "fixtureObservationsBounded",
  "profilesRemoved",
  "sharedProfileUnchanged",
  "ownedCommandCleanupProven",
  "reportWritten",
] as const satisfies readonly (keyof HubPanelMenuM3eCleanup)[];

export function createHubPanelMenuM3eCaseResults(): HubPanelMenuM3eCaseResult[] {
  return HUB_PANEL_MENU_M3E_REQUIRED_CASE_IDS.map((id) => ({
    id,
    status: "not-run",
    startedAt: null,
    completedAt: null,
    failure: null,
    observations: {},
  }));
}

export function hubPanelMenuM3eProfileSnapshotsEqual(
  before: HubPanelMenuM3eProfileSnapshot | null,
  after: HubPanelMenuM3eProfileSnapshot | null,
): boolean {
  return before !== null && after !== null &&
    before.exists === after.exists && before.entries === after.entries &&
    before.byteLength === after.byteLength && before.sha256 === after.sha256;
}

export function hubPanelMenuM3eVerdict(
  input: HubPanelMenuM3eVerdictInput,
): boolean {
  if (
    !input.complete || input.runFailure !== null ||
    input.launchFailures.length > 0 ||
    !hubPanelMenuM3eProfileSnapshotsEqual(
      input.sharedProfileBefore,
      input.sharedProfileAfter,
    ) ||
    !hasRequiredCleanupProof(input.cleanup)
  ) return false;

  const required = new Set<string>(HUB_PANEL_MENU_M3E_REQUIRED_CASE_IDS);
  const seen = new Set<string>();
  for (const result of input.cases) {
    if (!required.has(result.id) || seen.has(result.id)) return false;
    if (result.status !== "passed") return false;
    seen.add(result.id);
  }
  return seen.size === required.size;
}

function hasRequiredCleanupProof(
  cleanup: HubPanelMenuM3eCleanup,
): boolean {
  const keys = Object.keys(cleanup);
  return keys.length === HUB_PANEL_MENU_M3E_REQUIRED_CLEANUP_FIELDS.length &&
    HUB_PANEL_MENU_M3E_REQUIRED_CLEANUP_FIELDS.every((field) =>
      Object.hasOwn(cleanup, field) && cleanup[field] === true
    );
}
