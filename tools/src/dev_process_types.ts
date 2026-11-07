// SPDX-License-Identifier: MPL-2.0

export interface DevProcessIdentity {
  pid: number;
  startTime: string;
  commandLine: string;
}

export interface DevProcessInfo {
  pid: number;
  ppid: number;
  name: string;
  commandLine: string;
  startTime: string;
  /** Available on Linux, where ps does not preserve argument boundaries. */
  argv?: string[];
}

export interface DevStopScope {
  platform: "windows" | "darwin" | "linux";
  projectRoot: string;
  profilePath: string;
  serverPid?: number;
  recordedProcesses: readonly DevProcessIdentity[];
  all?: boolean;
  excludePids?: readonly number[];
}
