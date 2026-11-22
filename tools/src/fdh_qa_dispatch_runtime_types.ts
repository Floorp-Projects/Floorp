// SPDX-License-Identifier: MPL-2.0
import type { QaGhTransport } from "./fdh_qa_gh_transport_types.ts";
import type { QaDispatchWireTransport } from "./fdh_qa_dispatch_gh_types.ts";
export interface QaDispatchRuntimeTestDependencies {
  readonly github: QaGhTransport;
  readonly wire: QaDispatchWireTransport;
}
