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

// Regex to extract the "nora.tests.state" pref value from prefs.js.
// Firefox writes string prefs as: user_pref("name", "value") where
// value may contain \" for quotes and \\ for backslashes.
// Uses [\s\S] instead of . so the match spans multiple lines in case
// Firefox wraps a long pref value.
const PREF_REGEX =
  /user_pref\("nora\.tests\.state",\s*"((?:[^"\\]|\\[\s\S])*)"\)/;

/**
 * Collect browser test results by reading the `nora.tests.state` pref
 * from the profile's `prefs.js` file on disk.
 *
 * This does **not** require a Marionette connection and works even if
 * the browser shuts down before or during collection.
 */
export async function collectBrowserTestResultsFromPrefs(
  timeoutMs = PREFS_FILE_DEFAULT_TIMEOUT_MS,
): Promise<BrowserTestCollection> {
  const prefsPath = PATHS.profile_test + "/prefs.js";
  const deadline = Date.now() + timeoutMs;
  let lastStatus = "(none)";

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
        // Firefox may hex-encode certain characters (\xNN notation).
        // We convert those back before the double JSON.parse.
        //
        // Strategy: first replace \x5c (backslash) with a temporary
        // sentinel so that the generic hex decoder does not produce
        // premature escape sequences. Then apply the generic decoder
        // and restore the backslash as \\ (JSON-escaped) so JSON.parse
        // correctly produces a single literal backslash.
        const BACKSLASH_SENTINEL = "\x00BS";
        const raw = match[1]
          .replace(/\\x5c/g, BACKSLASH_SENTINEL)
          .replace(/\\x([0-9a-fA-F]{2})/g, (_, hex) =>
            String.fromCharCode(parseInt(hex, 16)),
          )
          .replace(/\\u([0-9a-fA-F]{4})/g, (_, hex) =>
            String.fromCharCode(parseInt(hex, 16)),
          )
          .replaceAll(BACKSLASH_SENTINEL, "\\\\");
        const jsonString: string = JSON.parse(`"${raw}"`);
        const state: TestState = JSON.parse(jsonString);
        lastStatus = state.status;

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
