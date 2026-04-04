// SPDX-License-Identifier: MPL-2.0

/**
 * Host-side module that collects browser test results via Marionette.
 *
 * The browser-side entry point (loader/test/index.ts) writes structured
 * results to globalThis.__TEST_RESULTS__. This module polls that global
 * using MarionetteClient.executeScript() until the status is "done" or
 * a timeout is reached.
 */

import { MarionetteClient } from "./browser_connector.ts";
import { sleep } from "./async_utils.ts";

const POLL_INTERVAL_MS = 500;
const DEFAULT_TIMEOUT_MS = 120_000;

const READ_TEST_STATE_SCRIPT = `
return (() => {
  if (globalThis.__TEST_RESULTS__) {
    return globalThis.__TEST_RESULTS__;
  }
  try {
    const raw = globalThis.Services?.prefs?.getStringPref("nora.tests.state", "") ?? "";
    return raw ? JSON.parse(raw) : null;
  } catch {
    return null;
  }
})();
`;

export interface BrowserTestResult {
  file: string;
  ok: boolean;
  durationMs: number;
  mode: "import" | "runAllTests";
  error?: string;
}

export interface BrowserTestCollection {
  results: BrowserTestResult[];
  discoveredFiles: string[];
}

interface TestState {
  status: "running" | "done" | "error";
  results: BrowserTestResult[];
  discoveredFiles?: string[];
  error?: string;
}

export async function collectBrowserTestResults(
  client: MarionetteClient,
  timeoutMs = DEFAULT_TIMEOUT_MS,
): Promise<BrowserTestCollection> {
  await client.setContext("chrome");

  // Wait for browser window to be ready — the browsing context may be
  // briefly unavailable during startup (window reload, HMR init, etc.)
  const WINDOW_READY_RETRIES = 10;
  const WINDOW_READY_DELAY_MS = 3_000;
  for (let i = 0; i < WINDOW_READY_RETRIES; i++) {
    try {
      await client.executeScript("return typeof Services !== 'undefined';");
      break; // window is ready
    } catch {
      if (i === WINDOW_READY_RETRIES - 1) {
        throw new Error(
          "Browser window not available after startup — browsing context was discarded",
        );
      }
      await sleep(WINDOW_READY_DELAY_MS);
    }
  }

  const deadline = Date.now() + timeoutMs;

  while (Date.now() < deadline) {
    let state: TestState | null = null;
    try {
      state = (await client.executeScript(
        READ_TEST_STATE_SCRIPT,
      )) as TestState | null;
    } catch (e: unknown) {
      // Connection may be lost if browser window reloaded — retry a few times
      const msg = e instanceof Error ? e.message : String(e);
      if (msg.includes("no such window") || msg.includes("discarded")) {
        await sleep(WINDOW_READY_DELAY_MS);
        continue;
      }
      throw new Error(
        `Lost connection to browser during test collection: ${msg}`,
      );
    }

    if (!state) {
      // Tests haven't started yet — browser may still be loading
      await sleep(POLL_INTERVAL_MS);
      continue;
    }

    if (state.status === "done") {
      return {
        results: state.results,
        discoveredFiles: state.discoveredFiles ?? [],
      };
    }

    if (state.status === "error") {
      throw new Error(
        `Browser test runner encountered a fatal error: ${state.error ?? "unknown"}`,
      );
    }

    // status === "running" — keep polling
    await sleep(POLL_INTERVAL_MS);
  }

  // Timeout — try to get partial results
  let partial: TestState | null = null;
  try {
    partial = (await client.executeScript(
      READ_TEST_STATE_SCRIPT,
    )) as TestState | null;
  } catch {
    // ignore — browser may have crashed
  }

  const completed = partial?.results?.length ?? 0;
  throw new Error(
    `Browser tests timed out after ${timeoutMs}ms. Completed: ${completed} test(s).`,
  );
}
