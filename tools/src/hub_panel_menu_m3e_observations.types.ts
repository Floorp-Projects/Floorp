// SPDX-License-Identifier: MPL-2.0

export interface HubPanelMenuM3eStartupExpectation {
  readonly profilePath: string;
  readonly featureEnabled: boolean;
  readonly manualHostEnabled: boolean;
  readonly pinnedRootExists: boolean;
  readonly selection: "legacy" | "pinned" | "manual";
}
