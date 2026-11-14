// SPDX-License-Identifier: MPL-2.0

export interface FakeGeckoState {
  os: string;
  mode: string;
  optIn: boolean;
}

export interface FakeGeckoOptions {
  tokenWriteGate?: Promise<void>;
  onTokenWriteStarted?: () => void;
  failOwnedRemoval?: boolean;
}

export interface BuiltTokenFileAdapter {
  publishTokenFile(token: string): Promise<void>;
}
