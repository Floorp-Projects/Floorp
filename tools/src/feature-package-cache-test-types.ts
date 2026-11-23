// SPDX-License-Identifier: MPL-2.0

import type { FeaturePackageCacheEntry } from "../../browser-features/modules/modules/feature-host/package/types.ts";

export type CacheFaultPoint =
  | "initialize"
  | "inspect"
  | "read"
  | "enumerate"
  | "writeStaging"
  | "moveStaging"
  | "removeStaging";

export interface CacheGate {
  readonly promise: Promise<void>;
  release(): void;
}

export interface MemoryCacheState {
  readonly files: Map<string, Uint8Array>;
  readonly specialEntries: Map<string, FeaturePackageCacheEntry["kind"]>;
  readonly failurePoints: Set<CacheFaultPoint>;
  initializeGate?: CacheGate;
  readGate?: CacheGate;
  stagingGate?: CacheGate;
  readStarted?: (name: string) => void;
  stagingStarted?: () => void;
  readReplacement?: Uint8Array;
}
