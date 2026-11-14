// SPDX-License-Identifier: MPL-2.0

import { assertEquals, assertStringIncludes } from "@std/assert";
import * as path from "@std/path";
import { savePrefs } from "./dev_env_manager.ts";

Deno.test("profile token isolation opts in only dev/test HTTP loader profiles", async () => {
  assertStringIncludes(await generateUserJs(true), 'user_pref("nora.dev.os_server.profile_token_file", true);');
  assertStringIncludes(await generateUserJs(false), 'user_pref("nora.dev.os_server.profile_token_file", false);');
});

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
    // the dev-only OS-Server port pref (KAN-135) is present even at offset 0
    assertStringIncludes(
      await generateUserJs(true),
      'user_pref("floorp.os.server.port", 5175);',
    );
  },
);

Deno.test(
  "savePrefs clears stale launch prefs from a reused prefs.js at offset 0",
  async () => {
    const root = await Deno.makeTempDir();
    try {
      const profileDir = path.join(root, "profile");
      Deno.mkdirSync(profileDir, { recursive: true });
      const prefsPath = path.join(profileDir, "prefs.js");
      Deno.writeTextFileSync(
        prefsPath,
        [
          'user_pref("browser.startup.homepage", "https://example.com");',
          'user_pref("marionette.port", 5280);',
          'user_pref("nora.dev.port_offset", 100);',
          'user_pref("floorp.os.server.port", 5275);',
        ].join("\n"),
      );

      savePrefs(profileDir, { allowBrowserHttpLoader: true, portOffset: 0 });
      const prefs = Deno.readTextFileSync(prefsPath);
      assertEquals(/marionette\.port/.test(prefs), false);
      assertEquals(/nora\.dev\.port_offset/.test(prefs), false);
      assertEquals(/floorp\.os\.server\.port/.test(prefs), false);
      assertStringIncludes(
        prefs,
        'user_pref("browser.startup.homepage", "https://example.com");',
      );
      assertEquals(
        /nora\.dev\.port_offset/.test(
          Deno.readTextFileSync(path.join(profileDir, "user.js")),
        ),
        false,
      );
    } finally {
      await Deno.remove(root, { recursive: true });
    }
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

Deno.test(
  "savePrefs writes the dev-only OS-Server port for every offset (KAN-135)",
  async () => {
    // offset 0 (and an omitted offset) bind the dev-only in-span port 5175
    assertStringIncludes(
      await generateUserJs(true),
      'user_pref("floorp.os.server.port", 5175);',
    );
    // nonzero offsets shift it inside the same dev span
    assertStringIncludes(
      await generateUserJs(true, 100),
      'user_pref("floorp.os.server.port", 5275);',
    );
    // the default and ladder offsets stay away from the packaged production
    // default 58261
    assertEquals(
      /floorp\.os\.server\.port", 58261/.test(
        await generateUserJs(true),
      ),
      false,
    );
    assertEquals(
      /floorp\.os\.server\.port", 58261/.test(
        await generateUserJs(true, 100),
      ),
      false,
    );
  },
);

Deno.test(
  "the hand-picked offset 53086 documents the one 58261 coincidence",
  async () => {
    // 5175 + 53086 = 58261: this offset coincides with the packaged
    // production default. It is not silently shared: the launch probe set
    // (launchPortsForOffset, used by auto resolution and the fdh guard)
    // rejects the rung while the port is held, and a direct bind fails with
    // the OS-Server's own port error.
    assertStringIncludes(
      await generateUserJs(true, 53086),
      'user_pref("floorp.os.server.port", 58261);',
    );
  },
);
