// SPDX-License-Identifier: MPL-2.0

import type {
  M3dOwnedCommandOperations,
  M3dOwnedCommandResult,
} from "./m3d_package_acquisition_command.types.ts";

function waitWithin<T>(promise: Promise<T>, timeoutMs: number): Promise<T> {
  let timer: ReturnType<typeof setTimeout> | undefined;
  const timeout = new Promise<never>((_, reject) => {
    timer = setTimeout(
      () => reject(new Error("owned command operation timed out")),
      timeoutMs,
    );
  });
  return Promise.race([promise, timeout]).finally(() => {
    if (timer !== undefined) clearTimeout(timer);
  });
}

async function cancelRead(
  operation: M3dOwnedCommandOperations["stdout"],
): Promise<void> {
  await operation.cancel();
}

export async function superviseM3dOwnedCommandOperations(
  operations: M3dOwnedCommandOperations,
  operationTimeoutMs: number,
  cleanupTimeoutMs: number,
): Promise<M3dOwnedCommandResult> {
  const output = Promise.all([
    operations.stdout.promise,
    operations.stderr.promise,
    operations.status,
  ]);
  try {
    const [stdout, _stderr, status] = await waitWithin(
      output,
      operationTimeoutMs,
    );
    return {
      completed: true,
      stdout,
      success: status.success,
      cleanupProven: true,
    };
  } catch {
    try {
      operations.terminate();
    } catch {
      // Drain the owned operations even if termination reports an error.
    }
    const cleanup = Promise.allSettled([
      cancelRead(operations.stdout),
      cancelRead(operations.stderr),
      operations.stdout.promise,
      operations.stderr.promise,
      operations.status,
    ]);
    let cleanupProven = false;
    try {
      const settled = await waitWithin(cleanup, cleanupTimeoutMs);
      cleanupProven = settled[0]?.status === "fulfilled" &&
        settled[1]?.status === "fulfilled" &&
        settled[4]?.status === "fulfilled";
    } catch {
      cleanupProven = false;
    }
    return {
      completed: false,
      stdout: null,
      success: null,
      cleanupProven,
    };
  }
}
