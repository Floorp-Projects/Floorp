// SPDX-License-Identifier: MPL-2.0

/** Attempt every cleanup phase, including protocol shutdown, after a failure. */
export async function runWorkspaceExternalContainerCleanup(
  steps: readonly (() => void | Promise<void>)[],
): Promise<void> {
  const errors: unknown[] = [];
  for (const step of steps) {
    try {
      await step();
    } catch (error) {
      errors.push(error);
    }
  }
  if (errors.length) {
    throw new AggregateError(
      errors,
      "external container fixture cleanup failed",
    );
  }
}
