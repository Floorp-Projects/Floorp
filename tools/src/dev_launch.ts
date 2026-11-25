// SPDX-License-Identifier: MPL-2.0

import { join } from "@std/path";
import { PLATFORM, PROJECT_ROOT } from "./defines.ts";
import { splitDevCommandLine } from "./dev_process_selection.ts";
import type { DevProcessIdentity } from "./dev_process_types.ts";
import { settleWithin } from "./async_utils.ts";
import { MarionetteClient } from "./browser_connector.ts";

export const DEV_STARTUP_TIMEOUT_MS = 120_000;
export const DEV_STARTUP_FINALIZE_MS = 10_000;

export async function probeDevMarionettePort(
  readPort: () => string,
  signal: AbortSignal,
  remaining: () => number,
  register: (
    port: number,
    signal: AbortSignal,
    timeoutMs: number,
  ) => Promise<unknown> = () => Promise.resolve(),
  probe = waitForDevMarionette,
  priorReady: (port: string) => Promise<boolean> = () => Promise.resolve(false),
): Promise<string> {
  signal.throwIfAborted();
  let text: string;
  try {
    text = readPort().trim();
  } catch (error) {
    if (error instanceof Deno.errors.NotFound) return "";
    throw error;
  }
  if (!text) return "";
  const port = Number(text);
  if (
    !/^\d+$/.test(text) || !Number.isInteger(port) || port < 1 || port > 65535
  ) {
    throw new Error("Invalid Marionette readiness port");
  }
  const ready = await priorReady(text);
  signal.throwIfAborted();
  if (ready) return text;
  await register(port, signal, remaining());
  signal.throwIfAborted();
  await probe(port, signal, remaining);
  signal.throwIfAborted();
  return text;
}

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

async function readinessIdentity(
  identity: DevProcessIdentity,
  profile: string,
  port: string,
): Promise<string> {
  const bytes = new TextEncoder().encode(
    JSON.stringify([
      identity.pid,
      identity.startTime,
      identity.commandLine,
      profile,
      port,
    ]),
  );
  return Array.from(
    new Uint8Array(await crypto.subtle.digest("SHA-256", bytes)),
    (byte) => byte.toString(16).padStart(2, "0"),
  ).join("");
}

export async function devStartupReady(
  identities: readonly DevProcessIdentity[],
  serverPid: number,
  profile: string,
  port: string,
) {
  const selected = [
    ...new Map(
      identities.filter((identity) =>
        identity.pid === serverPid ||
        splitDevCommandLine(identity.commandLine, PLATFORM).includes(
          "--marionette",
        )
      ).map((identity) => [identity.pid, identity]),
    ).values(),
  ];
  const complete = selected.some((identity) => identity.pid === serverPid) &&
    selected.some((identity) => identity.pid !== serverPid);
  return {
    state: "ready",
    instances: complete
      ? await Promise.all(
        selected.map((identity) => readinessIdentity(identity, profile, port)),
      )
      : [],
  };
}

export async function hasDevStartupReady(
  record: unknown,
  processes: readonly DevProcessIdentity[],
  serverPid: number,
  profile: string,
  port: string,
): Promise<boolean> {
  if (
    typeof record !== "object" || record === null || !("state" in record) ||
    record.state !== "ready" || !("instances" in record) ||
    !Array.isArray(record.instances) || record.instances.length < 2 ||
    new Set(record.instances).size !== record.instances.length
  ) return false;
  const server = processes.find((process) => process.pid === serverPid);
  if (
    !server ||
    !record.instances.includes(await readinessIdentity(server, profile, port))
  ) return false;
  const live = new Set(
    await Promise.all(
      processes.map((process) => readinessIdentity(process, profile, port)),
    ),
  );
  return record.instances.every((identity: unknown) =>
    typeof identity === "string" && live.has(identity)
  );
}

export async function verifyDevStartupReady(
  record: unknown,
  serverPid: number,
  profile: string,
  port: string,
  signal: AbortSignal,
  remaining: () => number,
  inspect: (
    signal: AbortSignal,
    timeoutMs: number,
  ) => Promise<readonly DevProcessIdentity[]>,
  connect: (
    port: number,
    signal: AbortSignal,
  ) => Promise<Pick<Deno.TcpConn, "close">> = (port, signal) =>
    Deno.connect({ hostname: "127.0.0.1", port, signal }),
): Promise<boolean> {
  signal.throwIfAborted();
  const timeoutMs = Math.min(5000, remaining());
  if (timeoutMs <= 0) throw new DevStartupFailure("startup-timeout");
  const processes = await inspect(signal, timeoutMs);
  signal.throwIfAborted();
  if (!await hasDevStartupReady(record, processes, serverPid, profile, port)) {
    return false;
  }
  const cancelled = Promise.withResolvers<never>();
  const abort = () => cancelled.reject(signal.reason);
  signal.addEventListener("abort", abort, { once: true });
  try {
    signal.throwIfAborted();
    const pending = connect(Number(port), signal).then((connection) => {
      if (signal.aborted) {
        connection.close();
        signal.throwIfAborted();
      }
      return connection;
    });
    const connection = await Promise.race([pending, cancelled.promise]);
    connection.close();
    signal.throwIfAborted();
    return true;
  } finally {
    signal.removeEventListener("abort", abort);
  }
}

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
