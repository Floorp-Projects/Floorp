// SPDX-License-Identifier: MPL-2.0

import {
  assessG5ExecutionBoundary,
  type G5BoundaryAssessment,
} from "./g5_execution_boundary.ts";
import {
  createG5DesktopProcessController,
  type G5DesktopLaunchSupervisor,
} from "./g5_desktop_process_controller.ts";
import {
  assessG5DesktopTwoClientLifecycleEvidence,
  type G5DesktopTwoClientLifecycleAssessment,
} from "./g5_desktop_two_client_lifecycle_contract.ts";
import type {
  IsolatedBrowserChild,
  IsolatedBrowserLaunchView,
  IsolatedBrowserProcessControl,
  IsolatedBrowserProcessOwnership,
} from "./browser_launcher.ts";

export const G5_DESKTOP_TWO_CLIENT_OFFLINE_FAKE_ONLY =
  "offline-fake-only-v1" as const;

export interface G5DesktopOfflineFakeClientPlan {
  readonly captureProofId: string;
  readonly clientInstanceId: string;
  readonly rootProcessGeneration: string;
  readonly terminationProofId: string;
}

export interface G5DesktopOfflineFakeStartRequest {
  readonly client: G5DesktopOfflineFakeClientPlan;
  readonly executorInstanceId: string;
  readonly pairId: string;
  readonly runId: string;
  readonly slot: "first" | "second";
}

export interface G5DesktopOfflineFakeClientSession {
  readonly child: IsolatedBrowserChild;
  readonly launch: IsolatedBrowserLaunchView;
}

export interface G5DesktopOfflineFakeDependencies {
  readonly startClient: (
    request: G5DesktopOfflineFakeStartRequest,
  ) => Promise<unknown>;
  readonly supervisor: G5DesktopLaunchSupervisor;
}

export interface G5DesktopOfflineFakeTwoClientExecutor {
  readonly runFakeLifecycle: () => Promise<G5DesktopOfflineFakeResult>;
}

export interface G5DesktopOfflineFakeResult {
  readonly boundary: G5BoundaryAssessment;
  readonly execution_authorization: "not-granted";
  readonly g5_result: "not-assessed";
  readonly lifecycle: G5DesktopTwoClientLifecycleAssessment;
  readonly mode: typeof G5_DESKTOP_TWO_CLIENT_OFFLINE_FAKE_ONLY;
}

interface ParsedOptions {
  readonly clients: readonly [
    G5DesktopOfflineFakeClientPlan,
    G5DesktopOfflineFakeClientPlan,
  ];
  readonly dependencies: G5DesktopOfflineFakeDependencies;
  readonly executorInstanceId: string;
  readonly pairId: string;
  readonly runId: string;
}

interface ParsedFakeClientSession {
  readonly child: IsolatedBrowserChild;
  readonly launch: IsolatedBrowserLaunchView;
}

interface StartedFakeClient {
  readonly plan: G5DesktopOfflineFakeClientPlan;
  readonly session: ParsedFakeClientSession;
}

interface CapturedFakeClient extends StartedFakeClient {
  readonly ownership: IsolatedBrowserProcessOwnership;
}

const OPAQUE_IDENTIFIER = /^[a-z0-9][a-z0-9._:-]{0,127}$/iu;
const INERT_SPAWN_DEPENDENCIES = Object.freeze({});

function exactDataProperties(
  value: unknown,
  expectedKeys: readonly string[],
): Record<string, unknown> | undefined {
  if (
    typeof value !== "object" || value === null || Array.isArray(value)
  ) {
    return undefined;
  }
  try {
    const record = value as Record<string, unknown>;
    if (Object.getPrototypeOf(record) !== Object.prototype) return undefined;
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

function isOpaqueIdentifier(value: unknown): value is string {
  return typeof value === "string" && OPAQUE_IDENTIFIER.test(value);
}

function isRootPid(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) && value > 0;
}

function isPort(value: unknown): value is number {
  return typeof value === "number" && Number.isSafeInteger(value) &&
    value >= 1_024 && value <= 65_535;
}

function isHighResolutionGeneration(
  value: unknown,
  rootPid: number,
): value is string {
  return typeof value === "string" &&
    new RegExp(`^pid-${rootPid}-generation-[0-9]{9,}$`, "u").test(value);
}

function parseClientPlan(
  value: unknown,
): G5DesktopOfflineFakeClientPlan | undefined {
  const plan = exactDataProperties(value, [
    "captureProofId",
    "clientInstanceId",
    "rootProcessGeneration",
    "terminationProofId",
  ]);
  if (
    plan === undefined || !isOpaqueIdentifier(plan.captureProofId) ||
    !isOpaqueIdentifier(plan.clientInstanceId) ||
    !isOpaqueIdentifier(plan.rootProcessGeneration) ||
    !isOpaqueIdentifier(plan.terminationProofId)
  ) {
    return undefined;
  }
  return Object.freeze({
    captureProofId: plan.captureProofId,
    clientInstanceId: plan.clientInstanceId,
    rootProcessGeneration: plan.rootProcessGeneration,
    terminationProofId: plan.terminationProofId,
  });
}

function parseSupervisor(
  value: unknown,
): G5DesktopLaunchSupervisor | undefined {
  const supervisor = exactDataProperties(value, ["abort", "capture", "stop"]);
  if (
    supervisor === undefined || typeof supervisor.abort !== "function" ||
    typeof supervisor.capture !== "function" ||
    typeof supervisor.stop !== "function"
  ) {
    return undefined;
  }
  return Object.freeze({
    abort: supervisor.abort as G5DesktopLaunchSupervisor["abort"],
    capture: supervisor.capture as G5DesktopLaunchSupervisor["capture"],
    stop: supervisor.stop as G5DesktopLaunchSupervisor["stop"],
  });
}

function parseOptions(value: unknown): ParsedOptions | undefined {
  const options = exactDataProperties(value, [
    "clients",
    "dependencies",
    "executorInstanceId",
    "mode",
    "pairId",
    "runId",
  ]);
  if (
    options === undefined ||
    options.mode !== G5_DESKTOP_TWO_CLIENT_OFFLINE_FAKE_ONLY ||
    !isOpaqueIdentifier(options.executorInstanceId) ||
    !isOpaqueIdentifier(options.pairId) || !isOpaqueIdentifier(options.runId) ||
    !Array.isArray(options.clients) || options.clients.length !== 2
  ) {
    return undefined;
  }
  const first = parseClientPlan(options.clients[0]);
  const second = parseClientPlan(options.clients[1]);
  const dependencies = exactDataProperties(options.dependencies, [
    "startClient",
    "supervisor",
  ]);
  const supervisor = dependencies === undefined
    ? undefined
    : parseSupervisor(dependencies.supervisor);
  if (
    first === undefined || second === undefined || dependencies === undefined ||
    typeof dependencies.startClient !== "function" || supervisor === undefined
  ) {
    return undefined;
  }
  return Object.freeze({
    clients: Object.freeze([first, second]) as readonly [
      G5DesktopOfflineFakeClientPlan,
      G5DesktopOfflineFakeClientPlan,
    ],
    dependencies: Object.freeze({
      startClient: dependencies.startClient as G5DesktopOfflineFakeDependencies[
        "startClient"
      ],
      supervisor,
    }),
    executorInstanceId: options.executorInstanceId,
    pairId: options.pairId,
    runId: options.runId,
  });
}

function hasDistinctIdentityBindings(options: ParsedOptions): boolean {
  const [first, second] = options.clients;
  const identifiers = [
    options.executorInstanceId,
    options.pairId,
    options.runId,
    first.captureProofId,
    first.clientInstanceId,
    first.terminationProofId,
    second.captureProofId,
    second.clientInstanceId,
    second.terminationProofId,
  ];
  return first.clientInstanceId < second.clientInstanceId &&
    new Set(identifiers).size === identifiers.length;
}

function parseFakeClientSession(
  value: unknown,
): ParsedFakeClientSession | undefined {
  const session = exactDataProperties(value, ["child", "launch"]);
  const fakeChild = session === undefined
    ? undefined
    : exactDataProperties(session.child, ["kill", "pid", "status"]);
  const fakeLaunch = session === undefined
    ? undefined
    : exactDataProperties(session.launch, ["command", "port", "profilePath"]);
  if (
    session === undefined || fakeChild === undefined ||
    fakeLaunch === undefined ||
    typeof fakeChild.kill !== "function" || !isRootPid(fakeChild.pid) ||
    !Array.isArray(fakeLaunch.command) ||
    !fakeLaunch.command.every((argument) => typeof argument === "string") ||
    !isPort(fakeLaunch.port) || typeof fakeLaunch.profilePath !== "string"
  ) {
    return undefined;
  }
  return Object.freeze({
    child: session.child as IsolatedBrowserChild,
    launch: session.launch as IsolatedBrowserLaunchView,
  });
}

function assertSessionMatchesPlan(
  session: ParsedFakeClientSession,
  plan: G5DesktopOfflineFakeClientPlan,
): void {
  if (
    !isHighResolutionGeneration(plan.rootProcessGeneration, session.child.pid)
  ) {
    throw new Error("G5 offline fake client session was rejected");
  }
}

function blockedBoundary(
  runId: string,
  executorInstanceId: string,
): G5BoundaryAssessment {
  return assessG5ExecutionBoundary(JSON.stringify({
    host: "unknown",
    runId,
    supervision: {
      descendants: "escaped-or-unprovable",
      eventStream: "lost-or-unknown",
      kind: "unknown",
      pidGeneration: "coarse-or-unknown",
    },
    executorInstanceId,
  }));
}

function lifecycleJson(
  options: ParsedOptions,
  clients: readonly [CapturedFakeClient, CapturedFakeClient],
): string {
  return JSON.stringify({
    clients: clients.map(({ plan, session }) => ({
      captureProof: {
        clientInstanceId: plan.clientInstanceId,
        descendantOwnership: "causally-complete",
        eventStream: "complete",
        executorInstanceId: options.executorInstanceId,
        marionettePort: session.launch.port,
        operation: "capture",
        pairId: options.pairId,
        pidGeneration: "high-resolution",
        proofId: plan.captureProofId,
        rootPid: session.child.pid,
        rootProcessGeneration: plan.rootProcessGeneration,
        runId: options.runId,
      },
      terminationProof: {
        captureProofId: plan.captureProofId,
        clientInstanceId: plan.clientInstanceId,
        descendantOwnership: "causally-complete",
        eventStream: "complete",
        executorInstanceId: options.executorInstanceId,
        marionettePort: session.launch.port,
        marionettePortState: "absent",
        operation: "stop",
        operationResult: "stopped",
        ownedTree: "absent",
        pairId: options.pairId,
        pidGeneration: "high-resolution",
        proofId: plan.terminationProofId,
        rootPid: session.child.pid,
        rootProcessGeneration: plan.rootProcessGeneration,
        runId: options.runId,
      },
    })),
    executorInstanceId: options.executorInstanceId,
    pairId: options.pairId,
    runId: options.runId,
    schemaVersion: "floorp-g5-desktop-two-client-lifecycle-v1",
  });
}

async function abortStartedClients(
  controller: IsolatedBrowserProcessControl,
  clients: readonly StartedFakeClient[],
): Promise<readonly unknown[]> {
  const results = await Promise.allSettled(
    [...clients].reverse().map(({ session }) =>
      controller.abort(
        session.child,
        session.launch,
        INERT_SPAWN_DEPENDENCIES,
      )
    ),
  );
  return results.flatMap((result) =>
    result.status === "rejected" ? [result.reason] : []
  );
}

async function startAndCapture(
  options: ParsedOptions,
  controller: IsolatedBrowserProcessControl,
  plan: G5DesktopOfflineFakeClientPlan,
  slot: "first" | "second",
  started: StartedFakeClient[],
): Promise<CapturedFakeClient> {
  const rawSession = await options.dependencies.startClient(Object.freeze({
    client: plan,
    executorInstanceId: options.executorInstanceId,
    pairId: options.pairId,
    runId: options.runId,
    slot,
  }));
  const session = parseFakeClientSession(rawSession);
  if (session === undefined) {
    throw new Error("G5 offline fake client session was rejected");
  }
  assertSessionMatchesPlan(session, plan);
  const startedClient = Object.freeze({ plan, session });
  started.push(startedClient);
  const ownership = await controller.capture(
    session.child,
    session.launch,
    "darwin",
  );
  return Object.freeze({ ...startedClient, ownership });
}

async function stopCapturedClients(
  controller: IsolatedBrowserProcessControl,
  clients: readonly [CapturedFakeClient, CapturedFakeClient],
): Promise<void> {
  const results = await Promise.allSettled(
    clients.map(({ ownership, session }) =>
      controller.stop(
        session.child,
        session.launch,
        ownership,
        INERT_SPAWN_DEPENDENCIES,
      )
    ),
  );
  const failures = results.flatMap((result) =>
    result.status === "rejected" ? [result.reason] : []
  );
  if (failures.length > 0) {
    throw new AggregateError(
      failures,
      "Failed to stop offline fake G5 desktop clients",
    );
  }
}

/**
 * Creates a single-use, data-only lifecycle exercise. Its only interactions
 * are the supplied fake callbacks; it has no built-in execution capability.
 */
export function createOfflineFakeG5DesktopTwoClientExecutor(
  options: unknown,
): G5DesktopOfflineFakeTwoClientExecutor {
  const parsed = parseOptions(options);
  if (parsed === undefined) {
    throw new Error(
      "G5 offline fake two-client executor configuration is invalid",
    );
  }
  if (!hasDistinctIdentityBindings(parsed)) {
    throw new Error(
      "G5 offline fake two-client identity invariants were rejected",
    );
  }
  const controller = createG5DesktopProcessController({
    executorInstanceId: parsed.executorInstanceId,
    runId: parsed.runId,
    supervisor: parsed.dependencies.supervisor,
  });
  let consumed = false;

  return Object.freeze({
    async runFakeLifecycle(): Promise<G5DesktopOfflineFakeResult> {
      if (consumed) {
        throw new Error("G5 offline fake two-client executor is single-use");
      }
      consumed = true;
      const boundary = blockedBoundary(parsed.runId, parsed.executorInstanceId);
      const started: StartedFakeClient[] = [];
      let first: CapturedFakeClient;
      let second: CapturedFakeClient;
      try {
        first = await startAndCapture(
          parsed,
          controller,
          parsed.clients[0],
          "first",
          started,
        );
        second = await startAndCapture(
          parsed,
          controller,
          parsed.clients[1],
          "second",
          started,
        );
      } catch (startupFailure) {
        const cleanupFailures = await abortStartedClients(controller, started);
        if (cleanupFailures.length > 0) {
          throw new AggregateError(
            [startupFailure, ...cleanupFailures],
            "Failed to clean up offline fake G5 desktop startup",
          );
        }
        throw startupFailure;
      }

      const clients: readonly [CapturedFakeClient, CapturedFakeClient] = [
        first,
        second,
      ];
      await stopCapturedClients(controller, clients);
      const lifecycle = assessG5DesktopTwoClientLifecycleEvidence(
        lifecycleJson(parsed, clients),
      );
      if (lifecycle.lifecycle_validation !== "accepted") {
        throw new Error("G5 offline fake lifecycle data was rejected");
      }
      return Object.freeze({
        boundary,
        execution_authorization: "not-granted" as const,
        g5_result: "not-assessed" as const,
        lifecycle,
        mode: G5_DESKTOP_TWO_CLIENT_OFFLINE_FAKE_ONLY,
      });
    },
  });
}
