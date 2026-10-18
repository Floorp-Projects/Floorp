// SPDX-License-Identifier: MPL-2.0
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
  tabs: LiveTab[];
}

export interface LiveIdentity {
  signedIn: boolean;
  verified: boolean;
  syncReady: boolean;
  uid: string | null;
}
