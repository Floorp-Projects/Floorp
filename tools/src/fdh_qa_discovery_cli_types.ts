// SPDX-License-Identifier: MPL-2.0
import type {
  QaDiscoveryOptions,
  QaDiscoveryReport,
} from "./fdh_qa_discovery_types.ts";

/** Library test seam only; no CLI option selects code, clock or transport. */
export interface QaDiscoveryCliTestDependencies {
  readonly discover: (
    options: QaDiscoveryOptions,
  ) => Promise<QaDiscoveryReport>;
}
