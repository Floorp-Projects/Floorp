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
  portOffset?: number,
): Promise<string> {
  const root = await Deno.makeTempDir();
  try {
    const profileDir = path.join(root, "profile");
    savePrefs(profileDir, { allowBrowserHttpLoader, portOffset });
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

Deno.test(
  "savePrefs keeps the user.js byte-identical without a port offset",
  async () => {
    assertEquals(
      await generateUserJs(true),
      await generateUserJs(true, 0),
    );
    assertEquals(
      await generateUserJs(false),
      await generateUserJs(false, 0),
    );
    assertEquals(/marionette\.port/.test(await generateUserJs(true)), false);
    assertEquals(
      /nora\.dev\.port_offset/.test(await generateUserJs(true)),
      false,
    );
  },
);

Deno.test(
  "savePrefs writes the offset pref and a disjoint Marionette port",
  async () => {
    const userJs = await generateUserJs(true, 100);

    assertStringIncludes(userJs, 'user_pref("nora.dev.port_offset", 100);');
    // browserRemote (5180) + offset 100: Marionette cannot overlap the
    // concurrent offset-0 worktree or this launch's Vite servers.
    assertStringIncludes(userJs, 'user_pref("marionette.port", 5280);');
  },
);
