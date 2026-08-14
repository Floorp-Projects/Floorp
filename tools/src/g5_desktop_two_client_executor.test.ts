// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert";
import type {
  IsolatedBrowserChild,
  IsolatedBrowserLaunchView,
} from "./browser_launcher.ts";
import type {
  G5DesktopCaptureRequest,
  G5DesktopLaunchSupervisor,
  G5DesktopTerminationRequest,
} from "./g5_desktop_process_controller.ts";
import {
  createOfflineFakeG5DesktopTwoClientExecutor,
  G5_DESKTOP_TWO_CLIENT_OFFLINE_FAKE_ONLY,
} from "./g5_desktop_two_client_executor.ts";

const RUN_ID = "g5-run-20260814-002";
const EXECUTOR_INSTANCE_ID = "g5-executor-20260814-002";
const PAIR_ID = "g5-pair-20260814-002";

interface FakeClientPlan {
  readonly captureProofId: string;
  readonly clientInstanceId: string;
  readonly rootProcessGeneration: string;
  readonly terminationProofId: string;
}

interface FakeClientSession {
  readonly child: IsolatedBrowserChild;
  readonly launch: IsolatedBrowserLaunchView;
}

interface FakeExecutorFixture {
  readonly abortRequests: G5DesktopTerminationRequest[];
  readonly captureRequests: G5DesktopCaptureRequest[];
  readonly executor: ReturnType<
    typeof createOfflineFakeG5DesktopTwoClientExecutor
  >;
  readonly startSlots: string[];
  readonly stopRequests: G5DesktopTerminationRequest[];
}

function generationFor(pid: number): string {
  return `pid-${pid}-generation-987654321`;
}

function child(pid: number): IsolatedBrowserChild {
  return Object.freeze({
    kill() {},
    pid,
    status: Promise.resolve({ code: 0, signal: null, success: true }),
  });
}

function launch(port: number): IsolatedBrowserLaunchView {
  return Object.freeze({
    command: Object.freeze(["/opt/floorp/Floorp"]),
    port,
    profilePath: `/private/fake-profile-${port}`,
  });
}

function clientPlan(
  clientInstanceId: string,
  pid: number,
  captureProofId: string,
  terminationProofId: string,
): FakeClientPlan {
  return {
    captureProofId,
    clientInstanceId,
    rootProcessGeneration: generationFor(pid),
    terminationProofId,
  };
}

function captureProof(
  request: G5DesktopCaptureRequest,
): Record<string, unknown> {
  return {
    descendantOwnership: "causally-complete",
    eventStream: "complete",
    executorInstanceId: request.executorInstanceId,
    marionettePort: request.marionettePort,
    operation: "capture",
    pidGeneration: "high-resolution",
    rootPid: request.rootPid,
    rootProcessGeneration: generationFor(request.rootPid),
    runId: request.runId,
  };
}

function terminationProof(
  request: G5DesktopTerminationRequest,
): Record<string, unknown> {
  return {
    capturedRootProcessGeneration: request.capturedRootProcessGeneration,
    descendantOwnership: "causally-complete",
    eventStream: "complete",
    executorInstanceId: request.executorInstanceId,
    marionettePort: request.marionettePort,
    marionettePortState: "absent",
    operation: request.operation,
    operationResult: request.operation === "abort" ? "aborted" : "stopped",
    ownedTree: "absent",
    pidGeneration: "high-resolution",
    rootPid: request.rootPid,
    rootProcessGeneration: request.capturedRootProcessGeneration ??
      generationFor(request.rootPid),
    runId: request.runId,
  };
}

function createFixture(overrides: {
  readonly clients?: readonly [FakeClientPlan, FakeClientPlan];
  readonly pairId?: string;
  readonly startClient?: (
    slot: "first" | "second",
  ) => Promise<FakeClientSession>;
  readonly supervisor?: Partial<G5DesktopLaunchSupervisor>;
} = {}): FakeExecutorFixture {
  const captureRequests: G5DesktopCaptureRequest[] = [];
  const abortRequests: G5DesktopTerminationRequest[] = [];
  const stopRequests: G5DesktopTerminationRequest[] = [];
  const startSlots: string[] = [];
  const sessions = {
    first: { child: child(4_201), launch: launch(28_291) },
    second: { child: child(4_202), launch: launch(28_292) },
  } as const;
  const supervisor: G5DesktopLaunchSupervisor = Object.freeze({
    abort(request: G5DesktopTerminationRequest) {
      abortRequests.push(request);
      return overrides.supervisor?.abort?.(request) ??
        Promise.resolve(terminationProof(request));
    },
    capture(request: G5DesktopCaptureRequest) {
      captureRequests.push(request);
      return overrides.supervisor?.capture?.(request) ??
        Promise.resolve(captureProof(request));
    },
    stop(request: G5DesktopTerminationRequest) {
      stopRequests.push(request);
      return overrides.supervisor?.stop?.(request) ??
        Promise.resolve(terminationProof(request));
    },
  });
  const startClient = async (
    { slot }: { readonly slot: "first" | "second" },
  ) => {
    startSlots.push(slot);
    return await overrides.startClient?.(slot) ?? sessions[slot];
  };
  return {
    abortRequests,
    captureRequests,
    executor: createOfflineFakeG5DesktopTwoClientExecutor({
      clients: overrides.clients ?? [
        clientPlan(
          "g5-client-a",
          4_201,
          "g5-capture-proof-a",
          "g5-termination-proof-a",
        ),
        clientPlan(
          "g5-client-b",
          4_202,
          "g5-capture-proof-b",
          "g5-termination-proof-b",
        ),
      ],
      dependencies: { startClient, supervisor },
      executorInstanceId: EXECUTOR_INSTANCE_ID,
      mode: G5_DESKTOP_TWO_CLIENT_OFFLINE_FAKE_ONLY,
      pairId: overrides.pairId ?? PAIR_ID,
      runId: RUN_ID,
    }),
    startSlots,
    stopRequests,
  };
}

Deno.test("offline fake executor binds a distinct pair and permanently withholds a G5 result", async () => {
  const fixture = createFixture();

  const result = await fixture.executor.runFakeLifecycle();

  assertEquals(fixture.startSlots, ["first", "second"]);
  assertEquals(
    fixture.captureRequests.map((request) => [
      request.rootPid,
      request.marionettePort,
    ]),
    [[4_201, 28_291], [4_202, 28_292]],
  );
  assertEquals(fixture.stopRequests.length, 2);
  assertEquals(fixture.abortRequests, []);
  assertEquals(result.mode, G5_DESKTOP_TWO_CLIENT_OFFLINE_FAKE_ONLY);
  assertEquals(result.execution_authorization, "not-granted");
  assertEquals(result.g5_result, "not-assessed");
  assertEquals(result.boundary.g5Result, "not-assessed");
  assertEquals(result.boundary.trustedExecutorVerification, "blocked");
  assertEquals(result.lifecycle.execution_authorization, "not-granted");
  assertEquals(result.lifecycle.g5_result, "not-assessed");
  assertEquals(result.lifecycle.lifecycle_validation, "accepted");
});

Deno.test("offline fake executor rejects overlapping pair, client, and proof identities before starting", () => {
  const invalidOverrides: ReadonlyArray<{
    readonly clients?: readonly [FakeClientPlan, FakeClientPlan];
    readonly pairId?: string;
  }> = [
    { pairId: RUN_ID },
    {
      clients: [
        clientPlan(
          "g5-client-a",
          4_201,
          "g5-capture-proof-a",
          "g5-termination-proof-a",
        ),
        clientPlan(
          "g5-client-a",
          4_202,
          "g5-capture-proof-b",
          "g5-termination-proof-b",
        ),
      ],
    },
    {
      clients: [
        clientPlan(
          "g5-client-a",
          4_201,
          "g5-capture-proof-a",
          "g5-termination-proof-a",
        ),
        clientPlan(
          "g5-client-b",
          4_202,
          "g5-capture-proof-a",
          "g5-termination-proof-b",
        ),
      ],
    },
  ];

  for (const overrides of invalidOverrides) {
    let startCalls = 0;
    assertThrows(
      () =>
        createFixture({
          ...overrides,
          startClient(slot) {
            startCalls += 1;
            return Promise.resolve(
              slot === "first"
                ? { child: child(4_201), launch: launch(28_291) }
                : { child: child(4_202), launch: launch(28_292) },
            );
          },
        }),
      Error,
      "G5 offline fake two-client identity invariants were rejected",
    );
    assertEquals(startCalls, 0);
  }
});

Deno.test("offline fake executor aborts its first fake client when second startup fails", async () => {
  const fixture = createFixture({
    startClient(slot) {
      if (slot === "second") {
        return Promise.reject(new Error("second fake startup failed"));
      }
      return Promise.resolve({ child: child(4_201), launch: launch(28_291) });
    },
  });

  await assertRejects(
    () => fixture.executor.runFakeLifecycle(),
    Error,
    "second fake startup failed",
  );
  assertEquals(fixture.startSlots, ["first", "second"]);
  assertEquals(fixture.abortRequests.length, 1);
  assertEquals(fixture.abortRequests[0].rootPid, 4_201);
  assertEquals(fixture.stopRequests, []);
});

Deno.test("offline fake executor propagates cleanup failure after second startup failure", async () => {
  const fixture = createFixture({
    startClient(slot) {
      if (slot === "second") {
        return Promise.reject(new Error("second fake startup failed"));
      }
      return Promise.resolve({ child: child(4_201), launch: launch(28_291) });
    },
    supervisor: {
      abort() {
        return Promise.reject(new Error("first fake cleanup failed"));
      },
    },
  });

  let failure: unknown;
  try {
    await fixture.executor.runFakeLifecycle();
  } catch (error) {
    failure = error;
  }

  assert(failure instanceof AggregateError);
  assertEquals(
    failure.errors.map((error) =>
      error instanceof Error ? error.message : error
    ),
    ["second fake startup failed", "G5 desktop supervisor operation failed"],
  );
  assertEquals(fixture.abortRequests.length, 1);
});

Deno.test("offline fake executor does not contain a live execution path", async () => {
  const source = await Deno.readTextFile(
    new URL("./g5_desktop_two_client_executor.ts", import.meta.url),
  );

  for (
    const forbidden of [
      "Deno.Command",
      ".spawn(",
      "Deno.env",
      "Deno.connect",
      "Deno.listen",
      "fetch(",
      "WebSocket",
      "startIsolatedBrowser",
      "createIsolatedBrowserLaunch",
      "Marionette",
      "FxA",
      "Sync",
      "credential",
      "password",
      "process.env",
    ]
  ) {
    assertEquals(source.includes(forbidden), false, forbidden);
  }
});
