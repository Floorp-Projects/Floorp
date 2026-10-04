// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertStringIncludes } from "@std/assert";
import * as path from "@std/path";
import { savePrefs } from "./dev_env_manager.ts";

const BASELINE_CSP_PREF =
  'user_pref("security.chrome_baseline_csp.enabled", false);';

function countOccurrences(text: string, needle: string): number {
  return text.split(needle).length - 1;
}

async function generateUserJs(
  allowBrowserHttpLoader: boolean,
): Promise<string> {
  const root = await Deno.makeTempDir();
  try {
    const profileDir = path.join(root, "profile");
    savePrefs(profileDir, { allowBrowserHttpLoader });
    return await Deno.readTextFile(path.join(profileDir, "user.js"));
  } finally {
    await Deno.remove(root, { recursive: true });
  }
}

Deno.test(
  "savePrefs enables chrome baseline CSP opt-out only for HTTP loader profiles",
  async () => {
    const userJs = await generateUserJs(true);

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
    const userJs = await generateUserJs(false);

    assertStringIncludes(
      userJs,
      'user_pref("nora.dev.allow_http_loader", false);',
    );
    assertEquals(countOccurrences(userJs, BASELINE_CSP_PREF), 0);
    assertEquals(/network\.lna\./.test(userJs), false);
  },
);
