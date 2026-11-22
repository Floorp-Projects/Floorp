// SPDX-License-Identifier: MPL-2.0
import { assertEquals, assertThrows } from "@std/assert";
import { workspaceProbeRedirect } from "./fdh_workspace_auth_cli_validation.ts";
for (const port of [1024, 19281, 65535]) {
  Deno.test(
    "probe redirect accepts exact unprivileged literal loopback " + port,
    () => {
      const raw = "http://127.0.0.1:" + port + "/oauth/callback";
      assertEquals(workspaceProbeRedirect(raw).href, raw);
    },
  );
}
for (
  const raw of [
    "",
    "http://localhost:19281/oauth/callback",
    "http://[::1]:19281/oauth/callback",
    "http://0.0.0.0:19281/oauth/callback",
    "http://127.1:19281/oauth/callback",
    "http://2130706433:19281/oauth/callback",
    "http://127.0.0.1.:19281/oauth/callback",
    "https://127.0.0.1:19281/oauth/callback",
    "http://evil.app:19281/oauth/callback",
    "http://127.0.0.1:0/oauth/callback",
    "http://127.0.0.1:80/oauth/callback",
    "http://127.0.0.1:1023/oauth/callback",
    "http://127.0.0.1:65536/oauth/callback",
    "http://127.0.0.1/oauth/callback",
    "http://user:password@127.0.0.1:19281/oauth/callback",
    "http://127.0.0.1:19281/oauth/callback?state=x",
    "http://127.0.0.1:19281/oauth/callback#fragment",
    "http://127.0.0.1:19281/other",
    "http://127.0.0.1:19281/oauth/callback/",
    "http://127.0.0.1:19281/oauth/%63allback",
    "http://127.0.0.1:19281/other/../oauth/callback",
  ]
) {
  Deno.test(
    "probe redirect rejects normalization ambiguity or unsafe listener " + raw,
    () => {
      assertThrows(() => workspaceProbeRedirect(raw));
    },
  );
}
