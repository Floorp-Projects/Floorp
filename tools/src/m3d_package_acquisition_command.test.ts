// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";
import { superviseM3dOwnedCommandOperations } from "./m3d_package_acquisition_command.ts";
import type {
  M3dOwnedCommandOperations,
  M3dOwnedCommandStatus,
} from "./m3d_package_acquisition_command.types.ts";

function deferred<T>(): {
  promise: Promise<T>;
  resolve: (value: T) => void;
} {
  let resolve!: (value: T) => void;
  const promise = new Promise<T>((resolvePromise) => {
    resolve = resolvePromise;
  });
  return { promise, resolve };
}

Deno.test("early output failure drains pending owned command operations", async () => {
  const stderr = deferred<Uint8Array>();
  const status = deferred<M3dOwnedCommandStatus>();
  let stderrSettled = false;
  let statusSettled = false;
  let pendingAtTermination = false;
  let terminateCalls = 0;
  let stdoutCancelCalls = 0;
  let stderrCancelCalls = 0;
  stderr.promise.then(() => stderrSettled = true);
  status.promise.then(() => statusSettled = true);

  const operations: M3dOwnedCommandOperations = {
    stdout: {
      promise: Promise.reject(new Error("simulated stdout read failure")),
      cancel: () => {
        stdoutCancelCalls++;
        return Promise.resolve();
      },
    },
    stderr: {
      promise: stderr.promise,
      cancel: () => {
        stderrCancelCalls++;
        stderr.resolve(new Uint8Array());
        return Promise.resolve();
      },
    },
    status: status.promise,
    terminate: () => {
      terminateCalls++;
      pendingAtTermination = !stderrSettled && !statusSettled;
      status.resolve({ success: false });
    },
  };

  const result = await superviseM3dOwnedCommandOperations(
    operations,
    1_000,
    1_000,
  );

  assertEquals(result.completed, false);
  assertEquals(result.cleanupProven, true);
  assertEquals(pendingAtTermination, true);
  assertEquals(terminateCalls, 1);
  assertEquals(stdoutCancelCalls, 1);
  assertEquals(stderrCancelCalls, 1);
  assertEquals(stderrSettled, true);
  assertEquals(statusSettled, true);
});

Deno.test("failed reader cancellation prevents cleanup proof", async () => {
  let stdoutCancelAttempted = false;
  let stderrCancelAttempted = false;
  const operations: M3dOwnedCommandOperations = {
    stdout: {
      promise: Promise.reject(new Error("simulated stdout read failure")),
      cancel: () => {
        stdoutCancelAttempted = true;
        throw new Error("simulated cancel failure");
      },
    },
    stderr: {
      promise: Promise.resolve(new Uint8Array()),
      cancel: () => {
        stderrCancelAttempted = true;
        return Promise.resolve();
      },
    },
    status: Promise.resolve({ success: false }),
    terminate: () => {},
  };

  const result = await superviseM3dOwnedCommandOperations(
    operations,
    1_000,
    1_000,
  );

  assertEquals(result.completed, false);
  assertEquals(result.cleanupProven, false);
  assertEquals(stdoutCancelAttempted, true);
  assertEquals(stderrCancelAttempted, true);
});

Deno.test("pending reads and status fail cleanup proof at its deadline", async () => {
  let stdoutCancelAttempted = false;
  let stderrCancelAttempted = false;
  const pending = new Promise<Uint8Array>(() => {});
  const operations: M3dOwnedCommandOperations = {
    stdout: {
      promise: Promise.reject(new Error("simulated stdout read failure")),
      cancel: () => {
        stdoutCancelAttempted = true;
        return Promise.resolve();
      },
    },
    stderr: {
      promise: pending,
      cancel: () => {
        stderrCancelAttempted = true;
        return Promise.resolve();
      },
    },
    status: new Promise<M3dOwnedCommandStatus>(() => {}),
    terminate: () => {},
  };
  let testDeadline: ReturnType<typeof setTimeout> | undefined;
  const testTimeout = new Promise<never>((_, reject) => {
    testDeadline = setTimeout(
      () => reject(new Error("cleanup test did not return within its bound")),
      500,
    );
  });

  try {
    const result = await Promise.race([
      superviseM3dOwnedCommandOperations(operations, 1_000, 20),
      testTimeout,
    ]);
    assertEquals(result.completed, false);
    assertEquals(result.cleanupProven, false);
    assertEquals(stdoutCancelAttempted, true);
    assertEquals(stderrCancelAttempted, true);
  } finally {
    if (testDeadline !== undefined) clearTimeout(testDeadline);
  }
});
