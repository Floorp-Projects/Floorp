// SPDX-License-Identifier: MPL-2.0

import type { DevProcessIdentity } from "./dev_process_types.ts";

/** Windows process snapshot row with the executable path needed for browser identity. */
export interface DevBrowserProcessRecord {
  pid: number;
  ppid: number;
  name: string;
  executablePath: string | null;
  commandLine: string | null;
  startTime: string;
}

/** A Windows TCP listener row, narrowed to identity-relevant fields. */
export interface DevBrowserListenerRecord {
  localPort: number;
  state: string;
  owningProcess: number;
}

export interface WindowsBrowserListenerSelection {
  port: number;
  expectedBinaryPath: string;
  expectedProfilePath: string;
  recordedProcesses: readonly DevProcessIdentity[];
  listeners: readonly DevBrowserListenerRecord[];
  processes: readonly DevBrowserProcessRecord[];
}

/** The actual browser listener root, captured from the current generation. */
export interface WindowsBrowserListenerIdentity extends DevProcessIdentity {
  executablePath: string;
}

export interface WindowsBrowserIdentityTarget {
  port: number;
  expectedBinaryPath: string;
  expectedProfilePath: string;
  recordedProcesses: readonly DevProcessIdentity[];
  retryCount?: number;
  retryDelayMs?: number;
}

/** Typed dependencies keep capture tests off PowerShell and the real registry. */
export interface WindowsBrowserIdentityDeps {
  listListeners(port: number): Promise<DevBrowserListenerRecord[]>;
  listProcesses(): Promise<DevBrowserProcessRecord[]>;
  recordIdentity(identity: WindowsBrowserListenerIdentity): Promise<boolean>;
  sleep(ms: number): Promise<void>;
}
