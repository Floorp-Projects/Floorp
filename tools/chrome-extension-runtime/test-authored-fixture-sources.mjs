// SPDX-License-Identifier: MPL-2.0
import { deepStrictEqual } from "node:assert";
import { Buffer } from "node:buffer";
import { readFileSync } from "node:fs";
import { test } from "node:test";
import { Script } from "node:vm";
import {
  pathProgram,
  sidebarProgram,
} from "../../browser-features/modules/modules/chrome-web-store/test/helpers/resource-paths/programs.mjs";
import { publicProgram } from "../../browser-features/modules/modules/chrome-web-store/test/helpers/offscreen-contexts/fixture-files.mjs";

// These sources execute in extension realms. Keep their readable witnesses out
// of host TypeScript program discovery, while checking their exact emitted bytes
// and JavaScript syntax without evaluating extension APIs in the host realm.
const helpers = new URL(
  "../../browser-features/modules/modules/chrome-web-store/test/helpers/",
  import.meta.url,
);
for (
  const [name, program] of [
    ["resource-paths/programs-source.js.txt", pathProgram],
    ["resource-paths/sidebar-source.js.txt", sidebarProgram],
    ["offscreen-contexts/public-program.js.txt", publicProgram],
  ]
) {
  test(name, () => {
    const source = readFileSync(new URL(name, helpers));
    deepStrictEqual(source, Buffer.from(program, "utf8"));
    new Script(source.toString("utf8"), { filename: name });
  });
}
