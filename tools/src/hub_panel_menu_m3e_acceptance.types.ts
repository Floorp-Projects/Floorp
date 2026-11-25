// SPDX-License-Identifier: MPL-2.0

import type { FeaturePackagePolicy } from "../../browser-features/modules/modules/feature-host/package/types.ts";
import type { MarionetteClient } from "./browser_connector.ts";
import type { DevProcessIdentity } from "./dev_process_types.ts";

export interface HubPanelMenuM3eCleanup {
  readonly launchOwnershipProven: boolean;
  readonly ownedProcessesStopped: boolean;
  readonly ownedPortsClosed: boolean;
  readonly fixtureClosed: boolean;
  readonly fixtureObservationsBounded: boolean;
  readonly profilesRemoved: boolean;
  readonly sharedProfileUnchanged: boolean;
  readonly ownedCommandCleanupProven: boolean;
  readonly reportWritten: boolean;
}

export type MutableHubPanelMenuM3eCleanup = {
  -readonly [Field in keyof HubPanelMenuM3eCleanup]: boolean;
};

export interface HubPanelMenuM3eStartupEvidence {
  readonly startupSelection: "legacy" | "pinned" | "manual";
  readonly normalWindowCount: number;
  readonly registryConnections: number;
  readonly hubButtonCount: number;
  readonly hubButtonEnabled: boolean | null;
  readonly hubButtonImmediatelyAfterSettings: boolean | null;
  readonly hubButtonLabel: string | null;
  readonly pinnedStateVersion: string | null;
  readonly pinnedStateDigest: string | null;
  readonly pinnedStateDesiredEnabled: boolean | null;
  readonly pinnedStateHealth: "verified" | "unverified" | null;
  readonly pinnedStatePending: boolean | null;
  readonly pinnedStateBlocked: boolean | null;
  readonly pinnedStateOutcomeStatus: string | null;
}

export interface HubPanelMenuM3eStartupSession {
  readonly id: string;
  readonly profilePath: string;
  readonly client: MarionetteClient;
  readonly statusOutputPath: string;
  readonly launchRecord: HubPanelMenuM3eLaunchRecord;
  readonly startupEvidence: HubPanelMenuM3eStartupEvidence;
}

export interface HubPanelMenuM3eLaunchRecord {
  rowId: string;
  profilePath: string;
  identities: readonly DevProcessIdentity[];
  ports: readonly number[];
  failure: HubPanelMenuM3eFailure | null;
}

export interface HubPanelMenuM3eFailure {
  readonly category:
    | "launch"
    | "run"
    | "snapshot"
    | "report"
    | "cleanup"
    | "incomplete";
  readonly kind: "Error" | "NotFound" | "PermissionDenied" | "Other";
  readonly rowId?: string;
}

export type HubPanelMenuM3eCaseStatus = "not-run" | "passed" | "failed";

export interface HubPanelMenuM3eCaseResult {
  readonly id: string;
  readonly status: HubPanelMenuM3eCaseStatus;
  readonly startedAt: string | null;
  readonly completedAt: string | null;
  readonly failure: HubPanelMenuM3eFailure | null;
  readonly observations: Readonly<
    Record<string, boolean | number | string | null>
  >;
}

export interface HubPanelMenuM3eProfileSnapshot {
  readonly exists: boolean;
  readonly entries: number;
  readonly byteLength: number;
  readonly sha256: string;
}

export interface HubPanelMenuM3eVerdictInput {
  readonly complete: boolean;
  readonly runFailure: HubPanelMenuM3eFailure | null;
  readonly cases: readonly HubPanelMenuM3eCaseResult[];
  readonly launchFailures: readonly HubPanelMenuM3eFailure[];
  readonly sharedProfileBefore: HubPanelMenuM3eProfileSnapshot | null;
  readonly sharedProfileAfter: HubPanelMenuM3eProfileSnapshot | null;
  readonly cleanup: HubPanelMenuM3eCleanup;
}

export interface HubPanelMenuM3eOwnedCommandResult {
  readonly exitCode?: number | null;
  readonly completed: boolean;
  readonly success: boolean;
  readonly stdout: string;
  readonly stderr: string;
  readonly cleanupProven: boolean;
}

export interface HubPanelMenuM3eSignedPackage {
  readonly version: string;
  readonly releaseSequence: number;
  readonly packageSha256: string;
  readonly byteLength: number;
  readonly bytes: Uint8Array;
}

export interface HubPanelMenuM3eFixtureBundle {
  readonly policy: FeaturePackagePolicy;
  readonly packages: Readonly<Record<string, HubPanelMenuM3eSignedPackage>>;
}

export interface HubPanelMenuM3ePolicyBinding {
  readonly hostSchema: number;
  readonly hostBuildId: string;
  readonly runtimeBuildId: string;
  readonly capabilities: readonly string[];
}

export interface HubPanelMenuM3eAcceptanceReport {
  readonly schemaVersion: 1;
  readonly runId: string;
  readonly startedAt: string;
  readonly completedAt: string | null;
  readonly passed: boolean;
  readonly cases: readonly HubPanelMenuM3eCaseResult[];
  readonly launchFailures: readonly HubPanelMenuM3eFailure[];
  readonly runFailure: HubPanelMenuM3eFailure | null;
  readonly sharedProfileBefore: HubPanelMenuM3eProfileSnapshot | null;
  readonly sharedProfileAfter: HubPanelMenuM3eProfileSnapshot | null;
  readonly cleanup: HubPanelMenuM3eCleanup;
  readonly launches: readonly HubPanelMenuM3eLaunchRecord[];
  readonly packageMetadata: readonly {
    readonly version: string;
    readonly releaseSequence: number;
    readonly packageSha256: string;
    readonly byteLength: number;
  }[];
  readonly policyBinding: HubPanelMenuM3ePolicyBinding | null;
  readonly reportPath: string;
}
