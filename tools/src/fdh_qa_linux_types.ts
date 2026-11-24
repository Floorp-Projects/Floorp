// SPDX-License-Identifier: MPL-2.0

export type { GuestRunOutput, QaTransport } from "./fdh_qa_remote_types.ts";

/** Network given to the per-task container. */
export type LinuxQaNetwork = "none" | "bridge";

/** Target description for one Docker host (see fdh_qa_linux.ts). */
export interface LinuxQaTargetConfig {
  /** SSH destination of the Docker host, `user@host`. */
  hostSsh: string;
  /** Container name; one container per target, so the name is the lock. */
  containerName: string;
  /** Memory limit of the container in MiB (no swap beyond it). */
  memoryMb: number;
  /** CPU limit of the container. */
  cpus: number;
  network: LinuxQaNetwork;
  /** Marionette port inside the container (never published). */
  marionettePort: number;
  /** Minimum free bytes in the Docker root before creating a container. */
  minFreeBytes: number;
  /** Minimum MemAvailable on the host, in MiB, before creating a container. */
  minAvailableMemoryMb: number;
}

/** Linux payloads: a Firefox-style tarball (xz, bzip2 or gzip). */
export type LinuxPayloadKind = "tar.xz" | "tar.bz2" | "tar.gz";

export interface LinuxQaPreflight {
  freeBytes: number;
  availableMemoryMb: number;
  imagePresent: boolean;
  /** Task id of the running container, "-foreign" for one we did not make. */
  containerHolder: string | null;
  ready: boolean;
  blockers: string[];
}

export interface LinuxQaCleanup {
  containerRemoved: boolean;
  clean: boolean;
}
