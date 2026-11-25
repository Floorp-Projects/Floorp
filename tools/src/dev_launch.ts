// SPDX-License-Identifier: MPL-2.0

import { join } from "@std/path";
import { PROJECT_ROOT } from "./defines.ts";
import { settleWithin } from "./async_utils.ts";

export const DEV_STARTUP_STATUS_FILE = join(
  PROJECT_ROOT,
  "_dist",
  "dev-startup-status.json",
);

export function nativeDevLogTarget(platform: string): string {
  return platform === "windows" ? "NUL" : "/dev/null";
}

export class DevStartupFailure extends Error {
  constructor(
    readonly code:
      | "startup-timeout"
      | "registration-failed"
      | "port-conflict"
      | "startup-failed",
    readonly port: number | null = null,
  ) {
    super(code);
  }
}

export function safeDevStartupFailure(error: unknown) {
  return {
    state: "failed",
    code: error instanceof DevStartupFailure ? error.code : "startup-failed",
    port: error instanceof DevStartupFailure ? error.port : null,
  };
}

export async function reportDevStartupFailure(error: unknown): Promise<void> {
  await Deno.writeTextFile(
    DEV_STARTUP_STATUS_FILE,
    JSON.stringify(safeDevStartupFailure(error)),
    { mode: 0o600 },
  );
}

export async function createDevServerLog(
  file: string,
  native: boolean,
  server: string,
  port: number,
  pid: number,
) {
  let tail = "";
  let conflict = false;
  let pending = Promise.resolve();
  const metadata = {
    server: [
        "designs",
        "main",
        "modal-child",
        "newtab",
        "notes",
        "settings",
        "welcome",
        "profile-manager",
        "workflow-progress",
      ].includes(server)
      ? server
      : "unknown",
    port,
    launcherPid: pid,
  };
  const finish = async (
    phase: "starting" | "ready" | "failed",
    exitCode: number | null = null,
  ) => {
    await pending;
    if (native) {
      await Deno.writeTextFile(
        file,
        JSON.stringify({
          ...metadata,
          phase,
          exitCode,
          portConflict: conflict,
        }),
        { mode: 0o600 },
      );
    }
  };
  if (!native) await Deno.writeTextFile(file, "");
  await finish("starting");
  return {
    get portConflict() {
      return conflict;
    },
    async write(bytes: Uint8Array) {
      tail = (tail + new TextDecoder().decode(bytes)).slice(-4096);
      conflict ||= /EADDRINUSE|port\s+\d+\s+is already in use/i.test(tail);
      if (!native) {
        pending = pending.then(() =>
          Deno.writeFile(file, bytes, { append: true })
        );
        await pending;
      }
    },
    finish,
  };
}

export async function startDevBrowser(
  startServers: (signal: AbortSignal) => Promise<void>,
  launch: (onStarted: () => void, signal: AbortSignal) => Promise<void>,
  shutdown: () => void | Promise<void>,
  onExit: () => void,
  onFailure: (error: unknown) => void,
  timeoutMs = 90000,
  reportFailure: (error: unknown) => Promise<void> = () => Promise.resolve(),
  cleanupTimeoutMs = 5000,
): Promise<void> {
  if (
    !Number.isFinite(timeoutMs) || timeoutMs <= 0 ||
    !Number.isFinite(cleanupTimeoutMs) || cleanupTimeoutMs <= 0
  ) {
    throw new TypeError("Invalid startup timeout");
  }
  const controller = new AbortController();
  let started = false;
  const ready = Promise.withResolvers<void>();
  const timer = setTimeout(() => {
    const error = new DevStartupFailure("startup-timeout");
    controller.abort(error);
    ready.reject(error);
  }, timeoutMs);
  let pending: Promise<void> | undefined;
  try {
    pending = startServers(controller.signal);
    await Promise.race([pending, ready.promise]);
    controller.signal.throwIfAborted();
    pending = launch(() => {
      if (controller.signal.aborted) return;
      started = true;
      ready.resolve();
    }, controller.signal);
    void pending.then(() => {
      if (started) onExit();
      else {ready.reject(
          new Error("Browser exited before startup registration"),
        );}
    }, (error: unknown) => {
      if (started) {
        void shutdown();
        onFailure(error);
      } else ready.reject(error);
    });
    await ready.promise;
  } catch (error) {
    controller.abort(error);
    await settleWithin(
      Promise.allSettled([
        Promise.resolve().then(shutdown),
        pending,
      ]),
      cleanupTimeoutMs,
    );
    await settleWithin(
      Promise.resolve().then(() => reportFailure(error)),
      cleanupTimeoutMs,
    );
    throw error;
  } finally {
    clearTimeout(timer);
  }
}
