// SPDX-License-Identifier: MPL-2.0
import type {
  QaCollectionReport,
  QaCollectorOptions,
} from "./fdh_final_package_qa_collector_types.ts";

import type { QaDiscoveryCliTestDependencies } from "./fdh_qa_discovery_cli_types.ts";

/** Library test seam only. No CLI argument can select a collector or transport. */
export interface QaCliTestDependencies {
  readonly discover?: QaDiscoveryCliTestDependencies["discover"];
  readonly collect: (
    options: QaCollectorOptions,
  ) => Promise<QaCollectionReport>;
}

export interface QaCliResult {
  readonly exitCode: 0 | 1 | 2;
  /** A single safe JSON summary, or fixed help text; caller writes stdout only. */
  readonly stdout: string;
}
