// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects } from "@std/assert";
import { runWorkspaceExternalContainerCleanup } from "../os-test/workspace_external_containers_cleanup.ts";

Deno.test("a failed content cleanup still closes its tab and the client after restoring the profile", async () => {
  const calls: string[] = [];
  const error = new Error("fixture did not load");
  const result = await assertRejects(
    () =>
      runWorkspaceExternalContainerCleanup([
        () => {
          calls.push("restore profile");
        },
        () => {
          calls.push("clean content");
          throw error;
        },
        () => {
          calls.push("close tab");
        },
        () => {
          calls.push("close client");
        },
      ]),
    AggregateError,
  );
  assertEquals(calls, [
    "restore profile",
    "clean content",
    "close tab",
    "close client",
  ]);
  assertEquals(result.errors, [error]);
});

Deno.test("profile restoration and tab failures do not skip the remaining cleanup phases", async () => {
  const calls: string[] = [];
  const restoreError = new Error("main window unavailable");
  const tabError = new Error("tab already closed");
  const result = await assertRejects(
    () =>
      runWorkspaceExternalContainerCleanup([
        () => {
          calls.push("restore profile");
          throw restoreError;
        },
        () => {
          calls.push("clean content");
        },
        () => {
          calls.push("close tab");
          throw tabError;
        },
        () => {
          calls.push("close client");
        },
      ]),
    AggregateError,
  );
  assertEquals(calls, [
    "restore profile",
    "clean content",
    "close tab",
    "close client",
  ]);
  assertEquals(result.errors, [restoreError, tabError]);
});
