// SPDX-License-Identifier: MPL-2.0

import type {
  IsolatedBrowserChild,
  IsolatedBrowserLaunchView,
  IsolatedBrowserProcessControl,
  IsolatedBrowserProcessOwnership,
  IsolatedBrowserSpawnDependencies,
} from "./browser_launcher.ts";

const G5_DESKTOP_PLATFORM_VALUES = [
  "aix",
  "darwin",
  "freebsd",
  "linux",
  "netbsd",
  "solaris",
  "windows",
] as const satisfies readonly (typeof Deno.build.os)[];

export type G5DesktopPlatform = (typeof G5_DESKTOP_PLATFORM_VALUES)[number];

export interface G5DesktopCaptureRequest {
  readonly executorInstanceId: string;
  readonly marionettePort: number;
  readonly operation: "capture";
  readonly rootPid: number;
  readonly runId: string;
}

export interface G5DesktopCaptureProof extends G5DesktopCaptureRequest {
  readonly descendantOwnership: "causally-complete";
  readonly eventStream: "complete";
  readonly pidGeneration: "high-resolution";
  readonly rootProcessGeneration: string;
}

export interface G5DesktopTerminationRequest {
  readonly capturedRootProcessGeneration: string | null;
  readonly executorInstanceId: string;
  readonly marionettePort: number;
  readonly operation: "abort" | "stop";
  readonly rootPid: number;
  readonly runId: string;
}

export interface G5DesktopTerminationProof extends G5DesktopTerminationRequest {
  readonly descendantOwnership: "causally-complete";
  readonly eventStream: "complete";
  readonly marionettePortState: "absent";
  readonly operationResult: "aborted" | "stopped";
  readonly ownedTree: "absent";
  readonly pidGeneration: "high-resolution";
  readonly rootProcessGeneration: string;
}

/**
 * A caller supplies the supervisor. This adapter neither launches nor inspects
 * a Desktop process; it accepts only explicit metadata-only proofs.
 */
export interface G5DesktopLaunchSupervisor {
  readonly abort: (
    request: G5DesktopTerminationRequest,
  ) => Promise<unknown>;
  readonly capture: (request: G5DesktopCaptureRequest) => Promise<unknown>;
  readonly stop: (
    request: G5DesktopTerminationRequest,
  ) => Promise<unknown>;
}

export interface G5DesktopProcessControllerOptions {
  readonly executorInstanceId: string;
  readonly runId: string;
  readonly supervisor: G5DesktopLaunchSupervisor;
}

interface ParsedSupervisor {
  readonly abort: G5DesktopLaunchSupervisor["abort"];
  readonly capture: G5DesktopLaunchSupervisor["capture"];
  readonly stop: G5DesktopLaunchSupervisor["stop"];
}

interface ParsedOptions {
  readonly executorInstanceId: string;
  readonly runId: string;
  readonly supervisor: ParsedSupervisor;
}

interface CaptureAttempt {
  readonly child: IsolatedBrowserChild;
  readonly executorInstanceId: string;
  readonly launch: IsolatedBrowserLaunchView;
  readonly marionettePort: number;
  readonly rootPid: number;
  readonly runId: string;
  rootProcessGeneration?: string;
  terminationState: "open" | "pending";
}

const OPAQUE_IDENTIFIER = /^[a-z0-9][a-z0-9._:-]{0,127}$/iu;

function exactDataProperties(
  value: unknown,
  expectedKeys: readonly string[],
): Record<string, unknown> | undefined {
  if (typeof value !== "object" || value === null || Array.isArray(value)) {
    return undefined;
  }
  try {
    const record = value as Record<string, unknown>;
    const keys = Reflect.ownKeys(record);
    if (
      keys.length !== expectedKeys.length ||
      !keys.every((key) =>
        typeof key === "string" && expectedKeys.includes(key)
      )
    ) {
      return undefined;
    }
    const copied: Record<string, unknown> = {};
    for (const key of expectedKeys) {
      const descriptor = Object.getOwnPropertyDescriptor(record, key);
      if (descriptor === undefined || !("value" in descriptor)) {
        return undefined;
      }
      copied[key] = descriptor.value;
    }
    return copied;
  } catch {
    return undefined;
  }
}

function isOpaqueRunId(value: unknown): value is string {
  return typeof value === "string" && OPAQUE_IDENTIFIER.test(value);
}

function isOpaqueExecutorInstanceId(value: unknown): value is string {
  return typeof value === "string" && OPAQUE_IDENTIFIER.test(value);
}

function isRootPid(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) && value > 0;
}

function isMarionettePort(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) &&
    value >= 1_024 && value <= 65_535;
}

function isPlatform(value: typeof Deno.build.os): value is G5DesktopPlatform {
  return G5_DESKTOP_PLATFORM_VALUES.includes(value as G5DesktopPlatform);
}

function isHighResolutionGeneration(
  value: unknown,
  rootPid: number,
): value is string {
  return typeof value === "string" &&
    new RegExp(`^pid-${rootPid}-generation-[0-9]{9,}$`, "u").test(value);
}

function parseOptions(value: unknown): ParsedOptions | undefined {
  const options = exactDataProperties(value, [
    "executorInstanceId",
    "runId",
    "supervisor",
  ]);
  if (
    options === undefined ||
    !isOpaqueExecutorInstanceId(options.executorInstanceId) ||
    !isOpaqueRunId(options.runId)
  ) {
    return undefined;
  }
  const supervisor = exactDataProperties(options.supervisor, [
    "abort",
    "capture",
    "stop",
  ]);
  if (
    supervisor === undefined || typeof supervisor.abort !== "function" ||
    typeof supervisor.capture !== "function" ||
    typeof supervisor.stop !== "function"
  ) {
    return undefined;
  }
  return {
    executorInstanceId: options.executorInstanceId,
    runId: options.runId,
    supervisor: {
      abort: supervisor.abort as G5DesktopLaunchSupervisor["abort"],
      capture: supervisor.capture as G5DesktopLaunchSupervisor["capture"],
      stop: supervisor.stop as G5DesktopLaunchSupervisor["stop"],
    },
  };
}

function captureInput(
  child: IsolatedBrowserChild,
  launch: IsolatedBrowserLaunchView,
): { readonly rootPid: number; readonly marionettePort: number } | undefined {
  try {
    if (!isRootPid(child.pid) || !isMarionettePort(launch.port)) {
      return undefined;
    }
    return { marionettePort: launch.port, rootPid: child.pid };
  } catch {
    return undefined;
  }
}

function parseCaptureProof(
  value: unknown,
  request: G5DesktopCaptureRequest,
): string | undefined {
  const proof = exactDataProperties(value, [
    "descendantOwnership",
    "eventStream",
    "executorInstanceId",
    "marionettePort",
    "operation",
    "pidGeneration",
    "rootPid",
    "rootProcessGeneration",
    "runId",
  ]);
  if (
    proof === undefined || proof.operation !== "capture" ||
    proof.runId !== request.runId ||
    proof.executorInstanceId !== request.executorInstanceId ||
    proof.rootPid !== request.rootPid ||
    proof.marionettePort !== request.marionettePort ||
    proof.pidGeneration !== "high-resolution" ||
    proof.eventStream !== "complete" ||
    proof.descendantOwnership !== "causally-complete" ||
    !isHighResolutionGeneration(proof.rootProcessGeneration, request.rootPid)
  ) {
    return undefined;
  }
  return proof.rootProcessGeneration;
}

function parseTerminationProof(
  value: unknown,
  request: G5DesktopTerminationRequest,
): boolean {
  const proof = exactDataProperties(value, [
    "capturedRootProcessGeneration",
    "descendantOwnership",
    "eventStream",
    "executorInstanceId",
    "marionettePort",
    "marionettePortState",
    "operation",
    "operationResult",
    "ownedTree",
    "pidGeneration",
    "rootPid",
    "rootProcessGeneration",
    "runId",
  ]);
  const expectedResult = request.operation === "stop" ? "stopped" : "aborted";
  return proof !== undefined && proof.operation === request.operation &&
    proof.operationResult === expectedResult && proof.runId === request.runId &&
    proof.executorInstanceId === request.executorInstanceId &&
    proof.rootPid === request.rootPid &&
    proof.marionettePort === request.marionettePort &&
    proof.pidGeneration === "high-resolution" &&
    proof.eventStream === "complete" &&
    proof.descendantOwnership === "causally-complete" &&
    proof.ownedTree === "absent" && proof.marionettePortState === "absent" &&
    proof.capturedRootProcessGeneration ===
      request.capturedRootProcessGeneration &&
    isHighResolutionGeneration(proof.rootProcessGeneration, request.rootPid) &&
    (request.capturedRootProcessGeneration === null ||
      proof.rootProcessGeneration === request.capturedRootProcessGeneration);
}

async function invokeSupervisor(
  invoke: (request: never) => Promise<unknown>,
  request: unknown,
): Promise<unknown> {
  try {
    return await invoke(request as never);
  } catch {
    throw new Error("G5 desktop supervisor operation failed");
  }
}

/**
 * Creates a pure adapter for an already injected lifecycle supervisor.
 *
 * It never authorizes an operation. A successful return means only that the
 * supervisor supplied a complete, matching ownership or termination proof.
 */
export function createG5DesktopProcessController(
  options: unknown,
): IsolatedBrowserProcessControl {
  const parsedOptions = parseOptions(options);
  if (parsedOptions === undefined) {
    throw new Error("G5 desktop process controller configuration is invalid");
  }
  const parsed: ParsedOptions = parsedOptions;

  const attemptsByChild = new WeakMap<IsolatedBrowserChild, CaptureAttempt>();
  const attemptsByOwnership = new WeakMap<
    IsolatedBrowserProcessOwnership,
    CaptureAttempt
  >();

  function matchingAttempt(
    child: IsolatedBrowserChild,
    launch: IsolatedBrowserLaunchView,
    ownership?: IsolatedBrowserProcessOwnership,
  ): CaptureAttempt | undefined {
    const attempt = ownership === undefined
      ? attemptsByChild.get(child)
      : attemptsByOwnership.get(ownership);
    if (
      attempt === undefined || attempt.child !== child ||
      attempt.launch !== launch ||
      attemptsByChild.get(child) !== attempt ||
      attempt.rootPid !== child.pid || attempt.marionettePort !== launch.port
    ) {
      return undefined;
    }
    return attempt;
  }

  async function terminate(
    operation: "abort" | "stop",
    child: IsolatedBrowserChild,
    launch: IsolatedBrowserLaunchView,
    ownership?: IsolatedBrowserProcessOwnership,
  ): Promise<void> {
    const attempt = matchingAttempt(child, launch, ownership);
    if (
      attempt === undefined ||
      (operation === "stop" && attempt.rootProcessGeneration === undefined)
    ) {
      throw new Error("G5 desktop process ownership is not captured");
    }
    if (attempt.terminationState !== "open") {
      throw new Error("G5 desktop process termination is already pending");
    }
    attempt.terminationState = "pending";
    const request: G5DesktopTerminationRequest = {
      capturedRootProcessGeneration: attempt.rootProcessGeneration ?? null,
      executorInstanceId: attempt.executorInstanceId,
      marionettePort: attempt.marionettePort,
      operation,
      rootPid: attempt.rootPid,
      runId: attempt.runId,
    };
    if (
      !isRootPid(request.rootPid) ||
      !isMarionettePort(request.marionettePort) ||
      !isOpaqueRunId(request.runId) ||
      !isOpaqueExecutorInstanceId(request.executorInstanceId) ||
      (request.capturedRootProcessGeneration !== null &&
        !isHighResolutionGeneration(
          request.capturedRootProcessGeneration,
          request.rootPid,
        ))
    ) {
      // This failure occurs before a supervisor invocation, so no external
      // action could have been requested and reopening the reservation is safe.
      attempt.terminationState = "open";
      throw new Error("G5 desktop process ownership is not captured");
    }
    // From this point forward, a rejected or malformed supervisor result may
    // follow a partial external action. Preserve the reservation to prevent a
    // duplicate termination request until an out-of-band recovery establishes
    // the final state.
    const proof = await invokeSupervisor(
      (operation === "stop"
        ? parsed.supervisor.stop
        : parsed.supervisor.abort) as (
          request: never,
        ) => Promise<unknown>,
      request,
    );
    if (!parseTerminationProof(proof, request)) {
      throw new Error("G5 desktop termination proof was rejected");
    }
    attemptsByChild.delete(child);
    if (ownership !== undefined) attemptsByOwnership.delete(ownership);
  }

  return Object.freeze({
    abort(
      child: IsolatedBrowserChild,
      launch: IsolatedBrowserLaunchView,
      _dependencies: IsolatedBrowserSpawnDependencies,
    ): Promise<void> {
      return terminate("abort", child, launch);
    },
    async capture(
      child: IsolatedBrowserChild,
      launch: IsolatedBrowserLaunchView,
      platform: typeof Deno.build.os,
    ): Promise<IsolatedBrowserProcessOwnership> {
      const input = captureInput(child, launch);
      if (
        input === undefined || !isPlatform(platform) ||
        attemptsByChild.has(child)
      ) {
        throw new Error("G5 desktop capture input was rejected");
      }
      const attempt: CaptureAttempt = {
        child,
        executorInstanceId: parsed.executorInstanceId,
        launch,
        marionettePort: input.marionettePort,
        rootPid: input.rootPid,
        runId: parsed.runId,
        terminationState: "open",
      };
      attemptsByChild.set(child, attempt);
      const request: G5DesktopCaptureRequest = {
        executorInstanceId: attempt.executorInstanceId,
        marionettePort: attempt.marionettePort,
        operation: "capture",
        rootPid: attempt.rootPid,
        runId: attempt.runId,
      };
      const proof = await invokeSupervisor(
        parsed.supervisor.capture as (request: never) => Promise<unknown>,
        request,
      );
      const rootProcessGeneration = parseCaptureProof(proof, request);
      if (rootProcessGeneration === undefined) {
        throw new Error("G5 desktop capture proof was rejected");
      }
      attempt.rootProcessGeneration = rootProcessGeneration;
      const ownership = Object.freeze({ platform, rootPid: attempt.rootPid });
      attemptsByOwnership.set(ownership, attempt);
      return ownership;
    },
    stop(
      child: IsolatedBrowserChild,
      launch: IsolatedBrowserLaunchView,
      ownership: IsolatedBrowserProcessOwnership,
      _dependencies: IsolatedBrowserSpawnDependencies,
    ): Promise<void> {
      return terminate("stop", child, launch, ownership);
    },
  });
}
