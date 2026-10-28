// SPDX-License-Identifier: MPL-2.0
import type { WorkspaceContainerRef } from "../../browser-features/modules/common/workspace-container-types.ts";

export interface LiveContainer {
  userContextId: number;
  name: string;
  color: string;
  icon: string;
}
export interface LiveClient {
  name: string;
  profile: string;
  marionettePort: number;
  app: string;
  binary: string;
}
export interface LiveSetup {
  clients: LiveClient[];
  buildCommit: string;
  buildTree: string;
  engineSha256: string;
  buildWorkingTreeDirty?: boolean;
  resourceSha256?: Record<string, string>;
}
export interface LiveTab {
  id: string;
  url: string;
  title: string;
  pinned: boolean;
  userContextId: number;
}
export interface LiveSnapshot {
  exists: boolean;
  name: string;
  selected: boolean;
  error: string | null;
  defaultUserContextId: number;
  desiredDefaultContainer: WorkspaceContainerRef | null;
  desiredTabs: {
    id: string;
    container?: WorkspaceContainerRef;
    deleted: boolean;
  }[];
  containerStates: {
    containerId: string;
    status: "resolved" | "pending";
    reason?: string;
    userContextId?: number;
  }[];
  tabs: LiveTab[];
}

export interface LiveIdentity {
  signedIn: boolean;
  verified: boolean;
  syncReady: boolean;
  uid: string | null;
}
