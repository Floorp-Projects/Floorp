// SPDX-License-Identifier: MPL-2.0
import type {
  QaDispatchOutcome,
  QaDispatchRequest,
} from "./fdh_qa_dispatch_types.ts";
export interface QaDispatchCliTestDependencies {
  readonly execute: (
    request: QaDispatchRequest,
    action: "execute" | "reconcile",
  ) => Promise<QaDispatchOutcome>;
}
