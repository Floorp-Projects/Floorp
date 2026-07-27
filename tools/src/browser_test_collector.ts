// SPDX-License-Identifier: MPL-2.0

/**
 * Host-side module that collects browser test results from disk.
 *
 * The browser-side entry point (loader/test/index.ts) writes structured
 * results to the Firefox pref "nora.tests.state" and flushes it to
 * the profile's prefs.js file. This module polls that file until the
 * test status is "done" or a timeout is reached.
 */

import { sleep } from "./async_utils.ts";
import { PATHS } from "./defines.ts";

// Polling intervals for file-based collection.
const PREFS_FILE_POLL_INTERVAL_MS = 1_000;
const PREFS_FILE_DEFAULT_TIMEOUT_MS = 300_000;

export interface BrowserTestResult {
  file: string;
  ok: boolean;
  durationMs: number;
  mode: "import" | "runAllTests" | "mozillaTasks";
  source?: "downloaded-firefox";
  upstreamPath?: string;
  manifestPath?: string;
  tasks?: Array<{
    index: number;
    name: string;
    ok: boolean;
    durationMs: number;
    error?: string;
  }>;
  error?: string;
  timedOut?: boolean;
}

export interface BrowserTestCollection {
  results: BrowserTestResult[];
  discoveredFiles: string[];
  aborted?: boolean;
  abortReason?: string;
}

interface TestState {
  status: "running" | "done" | "error";
  results: BrowserTestResult[];
  discoveredFiles?: string[];
  runId?: string;
  aborted?: boolean;
  abortReason?: string;
  error?: string;
}

// Regex to extract the "nora.tests.state" pref value from prefs.js.
// Firefox writes string prefs as: user_pref("name", "value") where
// value may contain \" for quotes and \\ for backslashes.
// Uses [\s\S] instead of . so the match spans multiple lines in case
// Firefox wraps a long pref value.
const PREF_REGEX =
  /user_pref\("nora\.tests\.state",\s*"((?:[^"\\]|\\[\s\S])*)"\)/;

function normalizePrefsJsHexEscapesForJson(raw: string): string {
  let output = "";

  for (let index = 0; index < raw.length;) {
    if (raw[index] !== "\\") {
      output += raw[index];
      index++;
      continue;
    }

    const slashStart = index;
    while (raw[index] === "\\") index++;
    const slashCount = index - slashStart;
    const hex = raw.slice(index + 1, index + 3);

    if (
      raw[index] === "x" &&
      /^[0-9a-fA-F]{2}$/.test(hex) &&
      slashCount % 2 === 1
    ) {
      output += "\\".repeat(slashCount - 1);
      output += `\\u00${hex}`;
      index += 3;
      continue;
    }

    output += "\\".repeat(slashCount);
  }

  return output;
}

/**
 * Collect browser test results by reading the `nora.tests.state` pref
 * from the profile's `prefs.js` file on disk.
 *
 * This does **not** require a Marionette connection and works even if
 * the browser shuts down before or during collection.
 *
 * To avoid returning stale results from a previous test run, the
 * collector first waits until the pref transitions away from "done"
 * (i.e. the browser-side runner has started a new run and set status
 * to "running").  Once the fresh "running" status is observed, it
 * then waits for the final "done" or "error" status.
 */
export async function collectBrowserTestResultsFromPrefs(
  timeoutMs = PREFS_FILE_DEFAULT_TIMEOUT_MS,
  expectedRunId?: string,
  prefsPath = PATHS.profile_test + "/prefs.js",
): Promise<BrowserTestCollection> {
  const deadline = Date.now() + timeoutMs;
  let lastStatus = "(none)";
  let sawRunning = false;

  while (Date.now() < deadline) {
    try {
      const content = await Deno.readTextFile(prefsPath);
      const match = content.match(PREF_REGEX);
      if (match?.[1]) {
        // Firefox stores pref string values with escaped quotes (\").
        // The captured group contains the raw pref value where \" represents
        // a literal quote character.  To recover the original JSON string we
        // wrap it in double-quotes and parse it as a JSON string, which
        // unescapes the \".  Then we parse the resulting string as JSON to
        // get the TestState object.
        //
        // Firefox may use JavaScript-only \xNN escapes in prefs.js. JSON
        // accepts the equivalent \u00NN spelling, so translate only the
        // escape syntax before parsing the outer string. Existing \uNNNN
        // escapes must remain syntactic until JSON.parse handles them;
        // decoding quotes or backslashes early would corrupt that wrapper.
        const raw = normalizePrefsJsHexEscapesForJson(match[1]);
        const jsonString: string = JSON.parse(`"${raw}"`);
        const state: TestState = JSON.parse(jsonString);
        lastStatus = state.status;

        if (expectedRunId) {
          if (state.runId !== expectedRunId) {
            await sleep(PREFS_FILE_POLL_INTERVAL_MS);
            continue;
          }

          if (state.status === "done") {
            return {
              results: state.results,
              discoveredFiles: state.discoveredFiles ?? [],
              aborted: state.aborted,
              abortReason: state.abortReason,
            };
          }

          if (state.status === "error") {
            throw new Error(
              `Browser test runner encountered a fatal error: ${
                state.error ?? "unknown"
              }`,
            );
          }

          await sleep(PREFS_FILE_POLL_INTERVAL_MS);
          continue;
        }

        // Phase 1: Wait for the browser-side runner to start a fresh run.
        // If the pref already contains "done" from a previous run we must
        // NOT return immediately — keep polling until the runner resets
        // the status to "running" for the current invocation.
        if (!sawRunning) {
          if (state.status === "running") {
            sawRunning = true;
          }
          // Whether the old status was "done", "error", or anything else,
          // we keep polling until we see "running" from the fresh run.
          await sleep(PREFS_FILE_POLL_INTERVAL_MS);
          continue;
        }

        // Phase 2: We have seen "running" — now wait for completion.
        if (state.status === "done") {
          return {
            results: state.results,
            discoveredFiles: state.discoveredFiles ?? [],
            aborted: state.aborted,
            abortReason: state.abortReason,
          };
        }

        if (state.status === "error") {
          throw new Error(
            `Browser test runner encountered a fatal error: ${
              state.error ?? "unknown"
            }`,
          );
        }
        // status === "running" — keep polling
      }
    } catch (e: unknown) {
      if (e instanceof Error && e.message.includes("fatal error")) {
        throw e;
      }
      // File might not exist yet, or Firefox may be mid-write (prefs.js
      // is written atomically on most platforms but the read can race with
      // a partial flush on Windows). The next poll iteration will retry.
    }

    await sleep(PREFS_FILE_POLL_INTERVAL_MS);
  }

  throw new Error(
    `Browser tests timed out after ${timeoutMs}ms (last status: ${lastStatus}). ` +
      `Check ${prefsPath} for partial results.`,
  );
}
