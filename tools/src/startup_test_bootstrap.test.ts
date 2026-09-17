// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertStrictEquals } from "@std/assert";
import {
  runTestBootstrapWithFailureReporting,
  type TestBootstrapPrefs,
} from "../../bridge/startup/src/test_bootstrap_failure.ts";

class FakePrefs implements TestBootstrapPrefs {
  runId = "";
  readonly writes: Array<{ name: string; value: string }> = [];
  readonly saves: null[] = [];
  throwOnRead = false;
  throwOnSave = false;

  getStringPref(_name: string, _fallback?: string): string {
    if (this.throwOnRead) throw new Error("read failed");
    return this.runId;
  }

  setStringPref(name: string, value: string): void {
    this.writes.push({ name, value });
  }

  savePrefFile(prefFile: null): void {
    this.saves.push(prefFile);
    if (this.throwOnSave) throw new Error("save failed");
  }
}

async function captureRejection(promise: Promise<unknown>): Promise<unknown> {
  try {
    await promise;
  } catch (error) {
    return error;
  }
  throw new Error("Expected promise to reject");
}

Deno.test("test bootstrap success leaves failure diagnostics untouched", async () => {
  const prefs = new FakePrefs();
  prefs.runId = "run-success";
  const startupErrors: string[] = [];

  const result = await runTestBootstrapWithFailureReporting(
    prefs,
    () => Promise.resolve("loaded"),
    (error) => startupErrors.push(error),
  );

  assertEquals(result, "loaded");
  assertEquals(startupErrors, []);
  assertEquals(prefs.writes, []);
  assertEquals(prefs.saves, []);
});

Deno.test("test bootstrap publishes an exact failure state for the captured run", async () => {
  const prefs = new FakePrefs();
  prefs.runId = "captured-run";
  const original = new Error("loader blocked by CSP");
  const startupErrors: string[] = [];

  const rejection = await captureRejection(
    runTestBootstrapWithFailureReporting(
      prefs,
      () => {
        // Proves the helper captured the run before bootstrap's first action.
        prefs.runId = "later-run";
        return Promise.reject(original);
      },
      (error) => startupErrors.push(error),
    ),
  );

  assertStrictEquals(rejection, original);
  assertEquals(startupErrors, ["Error: loader blocked by CSP"]);
  assertEquals(prefs.saves, [null]);
  assertEquals(prefs.writes, [{
    name: "nora.tests.state",
    value: JSON.stringify({
      status: "error",
      results: [],
      discoveredFiles: [],
      runId: "captured-run",
      error: "Error: loader blocked by CSP",
    }),
  }]);
});

Deno.test("test bootstrap does not publish uncorrelated failure state", async () => {
  for (const runId of ["", "   "]) {
    const prefs = new FakePrefs();
    prefs.runId = runId;
    const original = new Error("startup failed");
    const startupErrors: string[] = [];

    const rejection = await captureRejection(
      runTestBootstrapWithFailureReporting(
        prefs,
        () => Promise.reject(original),
        (error) => startupErrors.push(error),
      ),
    );

    assertStrictEquals(rejection, original);
    assertEquals(startupErrors, ["Error: startup failed"]);
    assertEquals(prefs.writes, []);
    assertEquals(prefs.saves, []);
  }
});

Deno.test("diagnostic persistence failure cannot mask bootstrap failure", async () => {
  const prefs = new FakePrefs();
  prefs.runId = "run-save-failure";
  prefs.throwOnSave = true;
  const original = new Error("original startup failure");

  const rejection = await captureRejection(
    runTestBootstrapWithFailureReporting(
      prefs,
      () => Promise.reject(original),
      () => {
        throw new Error("marker failure");
      },
    ),
  );

  assertStrictEquals(rejection, original);
  assertEquals(prefs.writes.length, 1);
  assertEquals(prefs.saves, [null]);
});

Deno.test("run-id read failure remains uncorrelated and preserves startup failure", async () => {
  const prefs = new FakePrefs();
  prefs.throwOnRead = true;
  const original = new Error("loader failure");

  const rejection = await captureRejection(
    runTestBootstrapWithFailureReporting(
      prefs,
      () => Promise.reject(original),
      () => {},
    ),
  );

  assertStrictEquals(rejection, original);
  assertEquals(prefs.writes, []);
  assertEquals(prefs.saves, []);
});
