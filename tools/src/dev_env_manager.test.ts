// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertStringIncludes } from "@std/assert";
import * as path from "@std/path";
import { type DevEnvProfileOptions, savePrefs } from "./dev_env_manager.ts";

const BASELINE_CSP_PREF =
  'user_pref("security.chrome_baseline_csp.enabled", false);';

function countOccurrences(text: string, needle: string): number {
  return text.split(needle).length - 1;
}

async function generateUserJs(
  options: DevEnvProfileOptions = {},
): Promise<string> {
  const root = await Deno.makeTempDir();
  try {
    const profileDir = path.join(root, "profile");
    savePrefs(profileDir, options);
    return await Deno.readTextFile(path.join(profileDir, "user.js"));
  } finally {
    await Deno.remove(root, { recursive: true });
  }
}

Deno.test(
  "savePrefs enables chrome baseline CSP opt-out only for HTTP loader profiles",
  async () => {
    const userJs = await generateUserJs({ allowBrowserHttpLoader: true });

    assertStringIncludes(
      userJs,
      'user_pref("nora.dev.allow_http_loader", true);',
    );
    assertEquals(countOccurrences(userJs, BASELINE_CSP_PREF), 1);
    assertEquals(/network\.lna\./.test(userJs), false);
  },
);

Deno.test(
  "savePrefs omits chrome baseline CSP opt-out for stage profiles",
  async () => {
    const userJs = await generateUserJs({ allowBrowserHttpLoader: false });

    assertStringIncludes(
      userJs,
      'user_pref("nora.dev.allow_http_loader", false);',
    );
    assertEquals(countOccurrences(userJs, BASELINE_CSP_PREF), 0);
    assertEquals(/network\.lna\./.test(userJs), false);
  },
);

Deno.test("savePrefs leaves chrome stdout unchanged in default, dev and stage profiles", async () => {
  for (
    const options of [
      {},
      { allowBrowserHttpLoader: true },
      { allowBrowserHttpLoader: false },
    ]
  ) {
    const userJs = await generateUserJs(options);
    assertEquals(userJs.includes("devtools.console.stdout.chrome"), false);
  }
});

Deno.test("savePrefs explicitly suppresses only chrome stdout dumping", async () => {
  const options = { allowBrowserHttpLoader: true };
  const defaultUserJs = await generateUserJs(options);
  const userJs = await generateUserJs({
    ...options,
    chromeConsoleStdout: false,
  });
  const suppression = 'user_pref("devtools.console.stdout.chrome", false);\n';
  assertEquals(countOccurrences(userJs, suppression), 1);
  assertEquals(userJs.replace(suppression, ""), defaultUserJs);
});

Deno.test("savePrefs can explicitly enable chrome stdout dumping", async () => {
  const defaultUserJs = await generateUserJs();
  const userJs = await generateUserJs({ chromeConsoleStdout: true });
  const enable = 'user_pref("devtools.console.stdout.chrome", true);\n';
  assertEquals(countOccurrences(userJs, enable), 1);
  assertEquals(userJs.replace(enable, ""), defaultUserJs);
});
