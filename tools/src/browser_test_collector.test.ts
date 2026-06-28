// SPDX-License-Identifier: MPL-2.0

import { assertEquals } from "@std/assert";
import * as path from "@std/path";
import { collectBrowserTestResultsFromPrefs } from "./browser_test_collector.ts";

type TestState = {
  status: "running" | "done" | "error";
  results: Array<{
    file: string;
    ok: boolean;
    durationMs: number;
    mode: "import" | "runAllTests" | "mozillaTasks";
  }>;
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

Deno.test("collectBrowserTestResultsFromPrefs ignores stale runId results", async () => {
  const dir = await Deno.makeTempDir();
  try {
    const prefsPath = path.join(dir, "prefs.js");
    await Deno.writeTextFile(
      prefsPath,
      encodePrefState({
        status: "done",
        runId: "stale-run",
        discoveredFiles: ["browser-features/chrome/test/stale.test.js"],
        results: [
          {
            file: "browser-features/chrome/test/stale.test.js",
            ok: true,
            durationMs: 1,
            mode: "import",
          },
        ],
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
