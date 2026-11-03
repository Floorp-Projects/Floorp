// SPDX-License-Identifier: MPL-2.0
export interface PortStateConfig {
  token: string;
  platform: "linux" | "windows" | "mac";
  outputPath: string;
  hostEventsPath: string;
  forbiddenLaunchPath: string;
  hosts: { allowed: string; forbidden: string; missing: string };
}
export interface PortStateError {
  name: string;
  message: string;
}
export interface PortStateDisconnect {
  index?: number;
  initialTask?: boolean;
  samePort: boolean;
  error: string | null;
  postThrow: PortStateError | null;
  callbackActive?: boolean;
}
export interface PortStateCase {
  kind: string;
  variant: string;
  timeout?: boolean;
  postThrow?: PortStateError | null;
  initialPostThrow?: PortStateError | null;
  eventualPostThrow?: PortStateError | null;
  cyclicThrow?: PortStateError | null;
  bigIntThrow?: PortStateError | null;
  listeners?: PortStateDisconnect[];
  disconnect?: PortStateDisconnect;
  echo?: { command: string; value: string; list: string[] } | null;
  hostCloseRequestedAt?: number;
  postAt?: number;
}
export interface PortStateResult {
  realm: "document" | "worker";
  runtimeId: string;
  cases: PortStateCase[];
}
export interface PortStateIncomingSender {
  realm: "document" | "worker";
  runtimeId: string;
  timeout?: boolean;
  localDisconnectAt: number;
  strategy?: "independent-worker-current-task";
  workerWorkMs?: number;
  connectReturnedAt?: number;
  workStartedAt?: number;
  workFinishedAt?: number;
  workElapsedMs?: number;
  localDisconnectReturnedAt?: number;
  localPostThrow?: PortStateError | null;
}
export interface PortStateIncoming {
  name: string;
  realm: "document" | "worker";
  callbacks: {
    index: number;
    startedAt: number;
    finishedAt: number;
    initialPostThrow: PortStateError | null;
    afterWorkPostThrow: PortStateError | null;
  }[];
  disconnects: PortStateDisconnect[];
  thenGetterReads: number;
}
export interface PortStateResponse {
  requestId: string;
  result?: PortStateResult | PortStateIncomingSender | PortStateIncoming | null;
  error?: string;
}
export interface PortStateHostEvent {
  event: "start" | "eof" | "delayed-exit";
  at: number;
  pid: number;
}
export interface PortStateEvidence {
  passed: boolean;
  token: string;
  runId: string;
  stage: "authored-actual-port-local-state-stress";
  states: PortStateResult[];
  incoming: {
    receiver: "document" | "worker";
    sender: PortStateIncomingSender;
    incoming: PortStateIncoming;
  }[];
  hostEvents: PortStateHostEvent[];
  error?: string;
}
