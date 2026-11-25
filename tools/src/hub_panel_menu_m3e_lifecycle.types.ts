// SPDX-License-Identifier: MPL-2.0

import type { MarionetteClient } from "./browser_connector.ts";
import type {
  HubPanelMenuM3eFixtureBundle,
  HubPanelMenuM3eSignedPackage,
} from "./hub_panel_menu_m3e_acceptance.types.ts";
import type { M3dPackageHttpsFixture } from "./m3d_package_https_fixture.types.ts";

export interface HubPanelMenuM3eLifecycleExpected {
  readonly selected: HubPanelMenuM3eSignedPackage | null;
  readonly highWater: HubPanelMenuM3eSignedPackage | null;
  readonly enabled: boolean;
  readonly health: "verified" | "unverified";
  readonly normalWindows: number;
  readonly label: string;
  readonly closed?: boolean;
}

export type HubPanelMenuM3eLifecycleEvidence = Readonly<
  Record<string, boolean | number | string | null>
>;

export interface HubPanelMenuM3eLifecycleContext {
  readonly inputDiagnosticsPath: string;
  readonly client: MarionetteClient;
  readonly testId: string;
  readonly bundle: HubPanelMenuM3eFixtureBundle;
  readonly fixture: M3dPackageHttpsFixture;
  readonly label: string;
  readonly v1DownloadedBase64: string;
  readonly runCase: (
    id: string,
    run: () => Promise<HubPanelMenuM3eLifecycleEvidence>,
  ) => Promise<void>;
}
