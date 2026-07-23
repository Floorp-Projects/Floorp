// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertRejects } from "@std/assert";
import * as path from "@std/path";
import {
  type BrowserTestResult,
  collectBrowserTestResultsFromPrefs,
} from "./browser_test_collector.ts";

type TestState = {
  status: "running" | "done" | "error";
  results: BrowserTestResult[];
  discoveredFiles?: string[];
  runId?: string;
  error?: string;
};

function encodePrefState(state: TestState): string {
  return `user_pref("nora.tests.state", ${
    JSON.stringify(JSON.stringify(state))
  });\n`;
}

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

Deno.test("collectBrowserTestResultsFromPrefs returns matching runId done state", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const prefsPath = path.join(dir, "prefs.js");
    const state: TestState = {
      status: "done",
      runId: "fresh-run",
      discoveredFiles: ["browser-features/chrome/test/example.test.js"],
      results: [
        {
          file: "browser-features/chrome/test/example.test.js",
          ok: true,
          durationMs: 7,
          mode: "mozillaTasks",
          source: "downloaded-firefox",
          upstreamPath:
            "browser/base/content/test/general/browser_bug537474.js",
          manifestPath: "browser/base/content/test/general/browser.toml",
          tasks: [
            {
              index: 1,
              name: "test_bug537474",
              ok: true,
              durationMs: 6,
            },
          ],
        },
      ],
    };
    await Deno.writeTextFile(prefsPath, encodePrefState(state));

    const collection = await collectBrowserTestResultsFromPrefs(
      1000,
      "fresh-run",
      prefsPath,
    );

    assertEquals(collection.discoveredFiles, state.discoveredFiles);
    assertEquals(collection.results, state.results);
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("collectBrowserTestResultsFromPrefs rejects a matching error promptly", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const prefsPath = path.join(dir, "prefs.js");
    await Deno.writeTextFile(
      prefsPath,
      encodePrefState({
        status: "error",
        runId: "fresh-run",
        discoveredFiles: [],
        results: [],
        error: "correlated bootstrap failure",
      }),
    );

    await assertRejects(
      () =>
        collectBrowserTestResultsFromPrefs(
          200,
          "fresh-run",
          prefsPath,
        ),
      Error,
      "fatal error: correlated bootstrap failure",
    );
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("collectBrowserTestResultsFromPrefs ignores a stale mismatched error until fresh done", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const prefsPath = path.join(dir, "prefs.js");
    await Deno.writeTextFile(
      prefsPath,
      encodePrefState({
        status: "error",
        runId: "stale-run",
        discoveredFiles: ["browser-features/chrome/test/stale.test.js"],
        results: [],
        error: "stale failure must be ignored",
      }),
    );

    const writer = (async () => {
      await sleep(25);
      await Deno.writeTextFile(
        prefsPath,
        encodePrefState({
          status: "done",
          runId: "fresh-run",
          discoveredFiles: ["browser-features/chrome/test/fresh.test.js"],
          results: [
            {
              file: "browser-features/chrome/test/fresh.test.js",
              ok: true,
              durationMs: 5,
              mode: "mozillaTasks",
            },
          ],
        }),
      );
    })();

    const collection = await collectBrowserTestResultsFromPrefs(
      2500,
      "fresh-run",
      prefsPath,
    );
    await writer;

    assertEquals(collection.discoveredFiles, [
      "browser-features/chrome/test/fresh.test.js",
    ]);
    assertEquals(
      collection.results[0]?.file,
      "browser-features/chrome/test/fresh.test.js",
    );
    assertEquals(collection.results[0]?.mode, "mozillaTasks");
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});

Deno.test("collectBrowserTestResultsFromPrefs ignores a stale mismatched error until fresh error", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const prefsPath = path.join(dir, "prefs.js");
    await Deno.writeTextFile(
      prefsPath,
      encodePrefState({
        status: "error",
        runId: "stale-run",
        discoveredFiles: [],
        results: [],
        error: "stale failure must be ignored",
      }),
    );

    const writer = (async () => {
      await sleep(25);
      await Deno.writeTextFile(
        prefsPath,
        encodePrefState({
          status: "error",
          runId: "fresh-run",
          discoveredFiles: [],
          results: [],
          error: "fresh correlated failure",
        }),
      );
    })();

    try {
      await assertRejects(
        () =>
          collectBrowserTestResultsFromPrefs(
            2500,
            "fresh-run",
            prefsPath,
          ),
        Error,
        "fatal error: fresh correlated failure",
      );
    } finally {
      await writer;
    }
  } finally {
    await Deno.remove(dir, { recursive: true });
  }
});
