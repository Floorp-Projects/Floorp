// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertStrictEquals } from "@std/assert";
import {
  runTestBootstrapWithFailureReporting,
  type TestBootstrapPrefs,
} from "../../bridge/startup/src/test_bootstrap_failure.ts";
import {
  startTestBootstrap,
  waitForTestOwnerStartup,
} from "../../bridge/startup/src/test_bootstrap_lifecycle.ts";

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

function deferred() {
  let resolve!: () => void;
  let reject!: (error: unknown) => void;
  const promise = new Promise<void>((done, fail) => {
    resolve = done;
    reject = fail;
  });
  return { promise, resolve, reject };
}

Deno.test("detached startup returns before tests and orders feature and owner readiness", async () => {
  const prefs = new FakePrefs();
  prefs.runId = "detached-owner";
  const features = deferred();
  const ownerReady = deferred();
  const testsStarted = deferred();
  const testsFinished = deferred();
  const events: string[] = [];
  const errors: unknown[] = [];
  const returned = startTestBootstrap(prefs, {
    initializeFeatures() {
      events.push("features");
      return features.promise;
    },
    waitForOwnerStartup() {
      events.push("owner-ready-wait");
      return ownerReady.promise;
    },
    executeTests() {
      events.push("tests");
      testsStarted.resolve();
      return testsFinished.promise;
    },
    recordStartupError: (error) => errors.push(error),
    reportRejectedBootstrap: (error) => errors.push(error),
  });
  assertEquals(returned, undefined);
  assertEquals(events, ["features"]);
  features.resolve();
  await Promise.resolve();
  assertEquals(events, ["features", "owner-ready-wait"]);
  ownerReady.resolve();
  await testsStarted.promise;
  assertEquals(events, ["features", "owner-ready-wait", "tests"]);
  testsFinished.resolve();
  await Promise.resolve();
  assertEquals(errors, []);
  assertEquals(prefs.writes, []);
});

Deno.test("detached readiness rejection publishes the originally captured run and is handled", async () => {
  const prefs = new FakePrefs();
  prefs.runId = "original-owner";
  const ownerReady = deferred();
  const reported = deferred();
  const original = new Error("owner window failed to finish startup");
  let testsStarted = false;
  let rejected: unknown;
  startTestBootstrap(prefs, {
    initializeFeatures: () => Promise.resolve(),
    waitForOwnerStartup: () => ownerReady.promise,
    executeTests() {
      testsStarted = true;
      return Promise.resolve();
    },
    recordStartupError: () => {},
    reportRejectedBootstrap(error) {
      rejected = error;
      reported.resolve();
      throw new Error("reporting must not create an unhandled rejection");
    },
  });
  prefs.runId = "later-owner";
  await Promise.resolve();
  ownerReady.reject(original);
  await reported.promise;
  assertStrictEquals(rejected, original);
  assertEquals(testsStarted, false);
  assertEquals(JSON.parse(prefs.writes[0].value).runId, "original-owner");
  assertEquals(prefs.saves, [null]);
});

class FakeOwnerWindow extends EventTarget {
  closed = false;
  document = { readyState: "loading" };
  gBrowserInit = { delayedStartupFinished: false };
}

Deno.test("owner readiness requires load and that owner's delayed startup", async () => {
  const owner = new FakeOwnerWindow();
  const observers = new Set<(subject: unknown) => void>();
  const pending = waitForTestOwnerStartup(owner, (observer) => {
    observers.add(observer);
    return () => observers.delete(observer);
  });
  let ready = false;
  void pending.then(() => ready = true);
  owner.document.readyState = "complete";
  owner.dispatchEvent(new Event("load"));
  await Promise.resolve();
  assertEquals(ready, false);
  owner.gBrowserInit.delayedStartupFinished = true;
  for (const observer of observers) observer(new FakeOwnerWindow());
  await Promise.resolve();
  assertEquals(ready, false);
  for (const observer of observers) observer(owner);
  await pending;
  assertEquals(ready, true);
  assertEquals(observers.size, 0);
});

Deno.test("owner delayed startup cannot bypass an unfinished document load", async () => {
  const owner = new FakeOwnerWindow();
  const observers = new Set<(subject: unknown) => void>();
  const pending = waitForTestOwnerStartup(owner, (observer) => {
    observers.add(observer);
    return () => observers.delete(observer);
  });
  let ready = false;
  void pending.then(() => ready = true);
  owner.gBrowserInit.delayedStartupFinished = true;
  for (const observer of observers) observer(owner);
  await Promise.resolve();
  assertEquals(ready, false);
  owner.document.readyState = "complete";
  owner.dispatchEvent(new Event("load"));
  await pending;
  assertEquals(ready, true);
  assertEquals(observers.size, 0);
});

Deno.test("owner startup closure and timeout reject and release observers", async () => {
  for (const reason of ["closed", "timeout"]) {
    const owner = new FakeOwnerWindow();
    const observers = new Set<(subject: unknown) => void>();
    const pending = waitForTestOwnerStartup(owner, (observer) => {
      observers.add(observer);
      return () => observers.delete(observer);
    }, 5);
    if (reason === "closed") {
      owner.closed = true;
      owner.dispatchEvent(new Event("unload"));
    }
    const rejection = await captureRejection(pending);
    assertEquals(rejection instanceof Error, true);
    assertEquals(observers.size, 0);
  }
});
