// SPDX-License-Identifier: MPL-2.0

/** Target description for one macOS guest (see fdh_qa_macos.ts). */
export interface MacosQaTargetConfig {
  /** SSH destination of the Mac host, `user@host`. */
  hostSsh: string;
  /** Absolute directory on the host that holds the guest key files. */
  vmDir: string;
  /** File names inside vmDir (no slashes). */
  guestKeyFile: string;
  guestKnownHostsFile: string;
  /** Guest SSH account (needs passwordless sudo). */
  guestUser: string;
  /** Guest IPv4 address as seen from the host. */
  guestAddress: string;
  /** GUI account Floorp runs as; must be logged in for launch. */
  testUser: string;
  /** Absolute task root inside the guest, under /Users/<testUser>/. */
  qaRoot: string;
  /** Marionette port used inside the guest. */
  marionettePort: number;
  /** Minimum free bytes on the guest data volume before staging. */
  minFreeBytes: number;
}

export type MacosPayloadKind = "dmg" | "tar.gz";

export type MarionetteRequest =
  | { op: "ready"; connectTimeout?: number }
  | { op: "eval"; script: string }
  | { op: "screenshot" }
  | { op: "quit" };

export interface GuestRunOutput {
  code: number;
  stdout: string;
  stderr: string;
}

/** Runs a guest script; `stdin` streams an upload when present. */
export type MacosQaTransport = (
  script: string,
  stdin?: ReadableStream<Uint8Array>,
) => Promise<GuestRunOutput>;

export interface MacosQaPreflight {
  freeBytes: number;
  guiSession: boolean;
  lockHolder: string | null;
  marionettePortBusy: boolean;
  osVersion: string;
  ready: boolean;
  blockers: string[];
}

export interface MacosQaCleanup {
  residualProcesses: number;
  taskDirRemoved: boolean;
  lockReleased: boolean;
  clean: boolean;
}
