// SPDX-License-Identifier: MPL-2.0

import type { BrowserLaunch } from "./hotfix_process_recovery_types.ts";
import type {
  M3dPackageHttpsFixture,
  M3dPackageHttpsMode,
  M3dPackageHttpsSnapshot,
} from "./m3d_package_https_fixture.types.ts";
import type { FeaturePackagePolicy } from "../../browser-features/modules/modules/feature-host/package/types.ts";

export interface M3dCompositionPackage {
  readonly version: string;
  readonly releaseSequence: number;
  readonly packageSha256: string;
  readonly bytes: Uint8Array;
}

export type M3dCompositionObservations = Readonly<
  Record<string, boolean | number | string | null>
>;

export interface M3dCompositionNativeScripts {
  readonly lifecycle: string;
  readonly closeStart: string;
  readonly closeReading: string;
  readonly closeFacade: string;
  readonly closeFinish: string;
  readonly cleanup: string;
}

export interface M3dCompositionContext {
  readonly launch: BrowserLaunch;
  readonly fixture: M3dPackageHttpsFixture;
  readonly policy: FeaturePackagePolicy;
  readonly featureId: string;
  readonly validMode: M3dPackageHttpsMode;
  readonly stalledMode: M3dPackageHttpsMode;
  readonly packages: readonly [M3dCompositionPackage, M3dCompositionPackage];
  readonly sourceProbePref: string;
  readonly browserOperation: (
    body: string,
    args: unknown,
    timeoutMs?: number,
  ) => Promise<Record<string, unknown>>;
  readonly waitForActiveStall: (
    baseline: M3dPackageHttpsSnapshot,
    timeoutMs: number,
  ) => Promise<M3dPackageHttpsSnapshot | null>;
  readonly waitForFixtureIdle: (timeoutMs: number) => Promise<boolean>;
  readonly waitForNativeDiagnosticsZero: (
    timeoutMs: number,
  ) => Promise<boolean>;
  readonly record: (
    id: string,
    passed: boolean,
    observations: M3dCompositionObservations,
  ) => void;
}
