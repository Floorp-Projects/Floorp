// SPDX-License-Identifier: MPL-2.0

import { assert, assertEquals, assertRejects, assertThrows } from "@std/assert";
import type {
  IsolatedBrowserChild,
  IsolatedBrowserLaunchView,
  IsolatedBrowserProcessControl,
  IsolatedBrowserProcessOwnership,
} from "./browser_launcher.ts";
import {
  createG5DesktopProcessController,
  type G5DesktopCaptureRequest,
  type G5DesktopPlatform,
  type G5DesktopTerminationRequest,
} from "./g5_desktop_process_controller.ts";

const RUN_ID = "g5-run-20260814-001";
const EXECUTOR_INSTANCE_ID = "g5-executor-20260814-001";
const PROCESS_GENERATION = "pid-4201-generation-987654321";
const G5_DESKTOP_PLATFORM_TYPE_FIXTURE = {
  aix: "aix",
  darwin: "darwin",
  freebsd: "freebsd",
  linux: "linux",
  netbsd: "netbsd",
  solaris: "solaris",
  windows: "windows",
} satisfies Record<G5DesktopPlatform, G5DesktopPlatform>;
// @ts-expect-error Android is intentionally not a supported Desktop runner.
const UNSUPPORTED_G5_DESKTOP_PLATFORM: G5DesktopPlatform = "android";

function child(pid = 4_201): IsolatedBrowserChild {
  return {
    kill() {},
    pid,
    status: Promise.resolve({ code: 0, signal: null, success: true }),
  };
}

function launch(port = 28_291): IsolatedBrowserLaunchView {
  return Object.freeze({
    command: Object.freeze(["/opt/floorp/Floorp"]),
    port,
    profilePath: "/private/profile",
  });
}

function captureProof(
  request: G5DesktopCaptureRequest,
  overrides: Record<string, unknown> = {},
): Record<string, unknown> {
  return {
    descendantOwnership: "causally-complete",
    eventStream: "complete",
    executorInstanceId: request.executorInstanceId,
    marionettePort: request.marionettePort,
    operation: "capture",
    pidGeneration: "high-resolution",
    rootPid: request.rootPid,
    rootProcessGeneration: PROCESS_GENERATION,
    runId: request.runId,
    ...overrides,
  };
}

function terminationProof(
  request: G5DesktopTerminationRequest,
  overrides: Record<string, unknown> = {},
): Record<string, unknown> {
  return {
    descendantOwnership: "causally-complete",
    eventStream: "complete",
    capturedRootProcessGeneration: request.capturedRootProcessGeneration,
    executorInstanceId: request.executorInstanceId,
    marionettePort: request.marionettePort,
    marionettePortState: "absent",
    operation: request.operation,
    operationResult: request.operation === "stop" ? "stopped" : "aborted",
    ownedTree: "absent",
    pidGeneration: "high-resolution",
    rootPid: request.rootPid,
    rootProcessGeneration: request.capturedRootProcessGeneration ??
      PROCESS_GENERATION,
    runId: request.runId,
    ...overrides,
  };
}

function createController(overrides: {
  abort?: (request: G5DesktopTerminationRequest) => Promise<unknown>;
  capture?: (request: G5DesktopCaptureRequest) => Promise<unknown>;
  stop?: (request: G5DesktopTerminationRequest) => Promise<unknown>;
} = {}): IsolatedBrowserProcessControl {
  return createG5DesktopProcessController({
    executorInstanceId: EXECUTOR_INSTANCE_ID,
    runId: RUN_ID,
    supervisor: {
      abort: overrides.abort ??
        ((request) => Promise.resolve(terminationProof(request))),
      capture: overrides.capture ??
        ((request) => Promise.resolve(captureProof(request))),
      stop: overrides.stop ??
        ((request) => Promise.resolve(terminationProof(request))),
    },
  });
}

Deno.test("G5 desktop controller captures only complete high-resolution ownership proof", async () => {
  const requests: G5DesktopCaptureRequest[] = [];
  const controller = createController({
    capture(request) {
      requests.push(request);
      return Promise.resolve(captureProof(request));
    },
  });
  const browser = child();
  const browserLaunch = launch();

  const ownership = await controller.capture(browser, browserLaunch, "darwin");

  assertEquals(requests, [{
    executorInstanceId: EXECUTOR_INSTANCE_ID,
    marionettePort: 28_291,
    operation: "capture",
    rootPid: 4_201,
    runId: RUN_ID,
  }]);
  assertEquals(ownership, { platform: "darwin", rootPid: 4_201 });
  assert(Object.isFrozen(ownership));
});

Deno.test("G5 desktop controller fails closed for incomplete capture proof", async () => {
  const incompleteProofs: Array<(request: G5DesktopCaptureRequest) => unknown> =
    [
      (request) =>
        captureProof(request, { pidGeneration: "coarse-or-unknown" }),
      (request) => captureProof(request, { eventStream: "lost-or-unknown" }),
      (request) =>
        captureProof(request, { descendantOwnership: "escaped-or-unprovable" }),
      (request) => ({ ...captureProof(request), rootProcessGeneration: "" }),
      (request) => ({ ...captureProof(request), unexpected: "not-accepted" }),
    ];

  for (const incompleteProof of incompleteProofs) {
    const controller = createController({
      capture(request) {
        return Promise.resolve(incompleteProof(request));
      },
    });

    await assertRejects(
      () => controller.capture(child(), launch(), "darwin"),
      Error,
      "G5 desktop capture proof was rejected",
    );
  }
});

Deno.test("G5 desktop controller stops only after its supervisor proves tree and port absence", async () => {
  const requests: G5DesktopTerminationRequest[] = [];
  const controller = createController({
    stop(request) {
      requests.push(request);
      return Promise.resolve(terminationProof(request));
    },
  });
  const browser = child();
  const browserLaunch = launch();
  const ownership = await controller.capture(browser, browserLaunch, "darwin");

  await controller.stop(browser, browserLaunch, ownership, {});

  assertEquals(requests, [{
    capturedRootProcessGeneration: PROCESS_GENERATION,
    executorInstanceId: EXECUTOR_INSTANCE_ID,
    marionettePort: 28_291,
    operation: "stop",
    rootPid: 4_201,
    runId: RUN_ID,
  }]);
});

Deno.test("G5 desktop controller reserves a captured tree before concurrent termination", async () => {
  const stopStarted = Promise.withResolvers<void>();
  const stopReleased = Promise.withResolvers<void>();
  let stopCalls = 0;
  const controller = createController({
    stop(request) {
      stopCalls += 1;
      stopStarted.resolve();
      return stopReleased.promise.then(() => terminationProof(request));
    },
  });
  const browser = child();
  const browserLaunch = launch();
  const ownership = await controller.capture(browser, browserLaunch, "darwin");

  const firstStop = controller.stop(browser, browserLaunch, ownership, {});
  await stopStarted.promise;
  const secondStop = controller.stop(browser, browserLaunch, ownership, {});
  const concurrentAbort = controller.abort(browser, browserLaunch, {});

  await assertRejects(
    () => secondStop,
    Error,
    "G5 desktop process termination is already pending",
  );
  await assertRejects(
    () => concurrentAbort,
    Error,
    "G5 desktop process termination is already pending",
  );
  assertEquals(stopCalls, 1);
  stopReleased.resolve();
  await firstStop;
  assertEquals(stopCalls, 1);
});

Deno.test("G5 desktop controller keeps a supervisor failure reserved and rejects duplicate action", async () => {
  let stopCalls = 0;
  const controller = createController({
    stop() {
      stopCalls += 1;
      return Promise.reject(new Error("must not be surfaced"));
    },
  });
  const browser = child();
  const browserLaunch = launch();
  const ownership = await controller.capture(browser, browserLaunch, "darwin");

  await assertRejects(
    () => controller.stop(browser, browserLaunch, ownership, {}),
    Error,
    "G5 desktop supervisor operation failed",
  );
  await assertRejects(
    () => controller.stop(browser, browserLaunch, ownership, {}),
    Error,
    "G5 desktop process termination is already pending",
  );
  assertEquals(stopCalls, 1);
});

Deno.test("G5 desktop controller keeps a port-residual proof reserved and rejects duplicate action", async () => {
  let stopCalls = 0;
  const controller = createController({
    stop(request) {
      stopCalls += 1;
      return Promise.resolve(
        terminationProof(request, { marionettePortState: "present" }),
      );
    },
  });
  const browser = child();
  const browserLaunch = launch();
  const ownership = await controller.capture(browser, browserLaunch, "darwin");

  await assertRejects(
    () => controller.stop(browser, browserLaunch, ownership, {}),
    Error,
    "G5 desktop termination proof was rejected",
  );
  await assertRejects(
    () => controller.abort(browser, browserLaunch, {}),
    Error,
    "G5 desktop process termination is already pending",
  );
  assertEquals(stopCalls, 1);
});

Deno.test("G5 desktop controller rejects malformed termination proof", async () => {
  const controller = createController({
    stop(request) {
      return Promise.resolve({
        ...terminationProof(request),
        capturedRootProcessGeneration: undefined,
      });
    },
  });
  const browser = child();
  const browserLaunch = launch();
  const ownership = await controller.capture(browser, browserLaunch, "darwin");

  await assertRejects(
    () => controller.stop(browser, browserLaunch, ownership, {}),
    Error,
    "G5 desktop termination proof was rejected",
  );
});

Deno.test("G5 desktop controller keeps an owned-tree-residual proof reserved", async () => {
  const controller = createController({
    abort(request) {
      return Promise.resolve(
        terminationProof(request, { ownedTree: "present" }),
      );
    },
  });
  const browser = child();
  const browserLaunch = launch();
  await controller.capture(browser, browserLaunch, "darwin");

  await assertRejects(
    () => controller.abort(browser, browserLaunch, {}),
    Error,
    "G5 desktop termination proof was rejected",
  );
  await assertRejects(
    () => controller.abort(browser, browserLaunch, {}),
    Error,
    "G5 desktop process termination is already pending",
  );
});

Deno.test("G5 desktop controller invalidates captured ownership after a successful abort", async () => {
  const controller = createController();
  const browser = child();
  const browserLaunch = launch();
  const ownership = await controller.capture(browser, browserLaunch, "darwin");

  await controller.abort(browser, browserLaunch, {});

  await assertRejects(
    () => controller.stop(browser, browserLaunch, ownership, {}),
    Error,
    "G5 desktop process ownership is not captured",
  );
});

Deno.test("G5 desktop controller rejects forged or unknown ownership before stopping", async () => {
  let stopCalls = 0;
  const controller = createController({
    stop(request) {
      stopCalls += 1;
      return Promise.resolve(terminationProof(request));
    },
  });
  const browser = child();
  const browserLaunch = launch();
  const ownership = await controller.capture(browser, browserLaunch, "darwin");
  const forged: IsolatedBrowserProcessOwnership = {
    platform: ownership.platform,
    rootPid: ownership.rootPid,
  };

  await assertRejects(
    () => controller.stop(browser, browserLaunch, forged, {}),
    Error,
    "G5 desktop process ownership is not captured",
  );
  assertEquals(stopCalls, 0);
  await controller.stop(browser, browserLaunch, ownership, {});
  assertEquals(stopCalls, 1);
});

Deno.test("G5 desktop controller requires non-secret opaque run and executor identities", () => {
  const supervisor = {
    abort: (request: G5DesktopTerminationRequest) =>
      Promise.resolve(terminationProof(request)),
    capture: (request: G5DesktopCaptureRequest) =>
      Promise.resolve(captureProof(request)),
    stop: (request: G5DesktopTerminationRequest) =>
      Promise.resolve(terminationProof(request)),
  };

  for (
    const options of [
      { executorInstanceId: "", runId: RUN_ID, supervisor },
      {
        executorInstanceId: EXECUTOR_INSTANCE_ID,
        runId: "contains space",
        supervisor,
      },
      { executorInstanceId: "contains@symbol", runId: RUN_ID, supervisor },
      {
        executorInstanceId: EXECUTOR_INSTANCE_ID,
        runId: RUN_ID,
        supervisor: {},
      },
    ]
  ) {
    assertThrows(
      () => createG5DesktopProcessController(options),
      Error,
      "G5 desktop process controller configuration is invalid",
    );
  }
});

Deno.test("G5 desktop controller has an exact compile-time desktop platform contract", () => {
  assertEquals(
    Object.values(G5_DESKTOP_PLATFORM_TYPE_FIXTURE).sort(),
    ["aix", "darwin", "freebsd", "linux", "netbsd", "solaris", "windows"],
  );
  assertEquals(UNSUPPORTED_G5_DESKTOP_PLATFORM, "android");
});

Deno.test("G5 desktop controller rejects unsupported Deno targets before requesting capture proof", async () => {
  let captureCalls = 0;
  const controller = createController({
    capture(request) {
      captureCalls += 1;
      return Promise.resolve(captureProof(request));
    },
  });

  for (const platform of ["android", "illumos"] as const) {
    await assertRejects(
      () => controller.capture(child(), launch(), platform),
      Error,
      "G5 desktop capture input was rejected",
    );
  }
  assertEquals(captureCalls, 0);
});

Deno.test("G5 desktop controller is a pure injected lifecycle adapter", async () => {
  const source = await Deno.readTextFile(
    new URL("./g5_desktop_process_controller.ts", import.meta.url),
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
      "console.",
      "browser_connector",
      "MarionetteClient",
      "startIsolatedBrowser",
      "createIsolatedBrowserLaunch",
      "executeScript",
      "screenshot",
      "credential",
      "password",
      "notes",
      "eval(",
      "Function(",
      "g5_execution_boundary",
      "trustedExecutorVerification",
      "g5Result",
    ]
  ) {
    assertEquals(source.includes(forbidden), false, forbidden);
  }
});
