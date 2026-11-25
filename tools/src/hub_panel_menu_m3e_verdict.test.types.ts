// SPDX-License-Identifier: MPL-2.0

import type {
  HubPanelMenuM3eCaseResult,
  HubPanelMenuM3eCleanup,
  HubPanelMenuM3eVerdictInput,
} from "./hub_panel_menu_m3e_acceptance.types.ts";

export type MutableHubPanelMenuM3eVerdictInput =
  & Omit<
    HubPanelMenuM3eVerdictInput,
    "cases" | "launchFailures" | "runFailure" | "cleanup"
  >
  & {
    cases: HubPanelMenuM3eCaseResult[];
    launchFailures: string[];
    runFailure: string | null;
    cleanup: { -readonly [Field in keyof HubPanelMenuM3eCleanup]: boolean };
  };
