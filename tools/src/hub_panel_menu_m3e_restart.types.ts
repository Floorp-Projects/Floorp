// SPDX-License-Identifier: MPL-2.0

import type { MarionetteClient } from "./browser_connector.ts";
import type {
  HubPanelMenuM3eLifecycleContext,
  HubPanelMenuM3eLifecycleEvidence,
} from "./hub_panel_menu_m3e_lifecycle.types.ts";

export interface HubPanelMenuM3eRestartContext
  extends Omit<HubPanelMenuM3eLifecycleContext, "client"> {
  readonly profilePath: string;
  readonly windowDiagnosticsPath: string;
  readonly getClient: () => MarionetteClient;
  readonly restart: (
    mutate?: () => Promise<Readonly<Record<string, unknown>>>,
  ) => Promise<HubPanelMenuM3eLifecycleEvidence>;
}
