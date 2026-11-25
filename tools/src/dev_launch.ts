// SPDX-License-Identifier: MPL-2.0

import { join } from "@std/path";
import { PROJECT_ROOT } from "./defines.ts";
import { settleWithin } from "./async_utils.ts";
import { MarionetteClient } from "./browser_connector.ts";

export const DEV_STARTUP_TIMEOUT_MS = 120_000;
export const DEV_STARTUP_FINALIZE_MS = 10_000;

export async function waitForDevMarionette(
  port: number,
  signal: AbortSignal,
  remaining: () => number,
  connect = (
    port: number,
    timeoutMs: number,
    signal: AbortSignal,
  ): Promise<Pick<MarionetteClient, "sendWithTimeout" | "abort">> =>
    MarionetteClient.connect(port, timeoutMs, undefined, signal),
): Promise<void> {
  signal.throwIfAborted();
  const client = await connect(port, remaining(), signal);
  try {
    signal.throwIfAborted();
    await client.sendWithTimeout("WebDriver:DeleteSession", {}, remaining());
    signal.throwIfAborted();
  } finally {
    client.abort();
  }
}

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

export async function pollDevStartup(
  check: (
    signal: AbortSignal,
    remaining: () => number,
    attempt: number,
  ) => Promise<boolean>,
  deadline: number,
  now = () => performance.now(),
  sleep?: (ms: number) => Promise<void>,
): Promise<void> {
  if (!Number.isFinite(deadline)) {
    throw new TypeError("Invalid startup deadline");
  }
  const failure = new DevStartupFailure("startup-timeout");
  const remaining = () => Math.max(0, deadline - now());
  if (remaining() <= 0) throw failure;
  const controller = new AbortController();
  const expired = Promise.withResolvers<never>();
  const timer = setTimeout(() => {
    controller.abort(failure);
    expired.reject(failure);
  }, remaining());
  let pauseTimer: ReturnType<typeof setTimeout> | undefined;
  const pause = sleep ?? ((ms: number) =>
    new Promise<void>((resolve) => {
      pauseTimer = setTimeout(resolve, ms);
    }));
  let pending: Promise<boolean> | undefined;
  try {
    for (let attempt = 0;; attempt++) {
      if (remaining() <= 0) throw failure;
      await Promise.race([pause(Math.min(1000, remaining())), expired.promise]);
      if (remaining() <= 0) throw failure;
      pending = Promise.resolve().then(() => {
        controller.signal.throwIfAborted();
        if (remaining() <= 0) throw failure;
        return check(controller.signal, remaining, attempt);
      });
      const ready = await Promise.race([pending, expired.promise]);
      if (remaining() <= 0) throw failure;
      if (ready) return;
    }
  } catch (error) {
    controller.abort(error);
    throw error;
  } finally {
    clearTimeout(timer);
    clearTimeout(pauseTimer);
    controller.abort();
    if (pending) await settleWithin(pending, 1000);
  }
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
