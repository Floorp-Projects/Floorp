// SPDX-License-Identifier: MPL-2.0

import type {
  M3dCommandReadOperation,
  M3dOwnedCommandStatus,
} from "./m3d_package_acquisition_command.types.ts";
import type { DevProcessIdentity } from "./dev_process_types.ts";

export interface HubPanelMenuM3eBoundedReadOperation
  extends M3dCommandReadOperation {
  readonly snapshot: () => Uint8Array;
}

export interface HubPanelMenuM3eOwnedCommandOperations {
  readonly stdout: HubPanelMenuM3eBoundedReadOperation;
  readonly stderr: HubPanelMenuM3eBoundedReadOperation;
  readonly status: Promise<M3dOwnedCommandStatus>;
  readonly terminate: () => void;
}

export interface HubPanelMenuM3eOwnedProcessRefresh {
  readonly identities: readonly DevProcessIdentity[];
  readonly liveIdentities: readonly DevProcessIdentity[];
  readonly ownershipProven: boolean;
}
