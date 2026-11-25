// SPDX-License-Identifier: MPL-2.0

export type HubPanelMenuM3eBrowserScriptKind =
  | "profile-path"
  | "clear-task-state"
  | "lifecycle"
  | "focus"
  | "startup-observation"
  | "post-start-immutability"
  | "policy-binding"
  | "capture-inertness"
  | "download-package"
  | "create-host"
  | "reopen-host"
  | "host-operation"
  | "arm-barrier"
  | "start-gated-operation"
  | "release-barrier"
  | "await-operation"
  | "close-host";

export interface HubPanelMenuM3eBrowserScriptInput {
  readonly kind: HubPanelMenuM3eBrowserScriptKind;
  readonly payload?: Readonly<Record<string, unknown>>;
}
