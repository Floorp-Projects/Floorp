// SPDX-License-Identifier: MPL-2.0
import { assertEquals, assertThrows } from "@std/assert";
import { userScriptsDeclaration } from "../chrome-extension-runtime/corpus-user-scripts-consent.mjs";

Deno.test("only exact original MV3 declarations authorize the consent UI step", () => {
  assertEquals(
    userScriptsDeclaration({
      manifest_version: 3,
      permissions: ["userScripts"],
    }),
    "required",
  );
  assertEquals(
    userScriptsDeclaration({
      manifest_version: 3,
      optional_permissions: ["userScripts"],
    }),
    "optional",
  );
  assertEquals(
    userScriptsDeclaration({
      manifest_version: 3,
      permissions: ["userScripts"],
      optional_permissions: ["userScripts"],
    }),
    "required",
  );
  for (
    const manifest of [
      { manifest_version: 2, permissions: ["userScripts"] },
      { manifest_version: 3, permissions: ["scripting", "storage"] },
      { manifest_version: 3, permissions: ["userScriptsOther"] },
      { manifest_version: 3 },
    ]
  ) assertEquals(userScriptsDeclaration(manifest), null);
});

Deno.test("malformed authenticated manifest permissions fail closed", () => {
  for (
    const manifest of [
      null,
      [],
      { manifest_version: 3, permissions: "userScripts" },
      { manifest_version: 3, optional_permissions: ["userScripts", 1] },
    ]
  ) assertThrows(() => userScriptsDeclaration(manifest));
});
