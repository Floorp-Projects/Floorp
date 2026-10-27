// SPDX-License-Identifier: MPL-2.0
import { createHash } from "node:crypto";
import { deepStrictEqual, strictEqual } from "node:assert";
import { Buffer } from "node:buffer";
import { readFileSync } from "node:fs";
import { test } from "node:test";
import { Script } from "node:vm";
import {
  pathProgram,
  sidebarProgram,
} from "../../browser-features/modules/modules/chrome-web-store/test/helpers/resource-paths/programs.mjs";
import { publicProgram } from "../../browser-features/modules/modules/chrome-web-store/test/helpers/offscreen-contexts/fixture-files.mjs";

import {
  messageEdgeProbeSource,
  messageJSONProbeSource,
  messageOverloadProbeSource,
} from "./native-message-json-sources.mjs";
import { responseRaceProbeSource } from "./native-response-race-sources.mjs";
import {
  incomingPortStateProbeSource,
  portStateStressProbeSource,
} from "./native-port-state-probe-sources.mjs";
import {
  quietNativeDocumentProbeSource,
  quietNativeWorkerProbeSource,
} from "./quiet-native-probe-sources.mjs";

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

for (
  const [name, program] of [
    ["native-message-json-probe.js.txt", messageJSONProbeSource],
    ["native-message-overload-probe.js.txt", messageOverloadProbeSource],
    ["native-message-edge-probe.js.txt", messageEdgeProbeSource],
    ["native-response-race-probe.js.txt", responseRaceProbeSource],
    ["native-port-state-stress-probe.js.txt", portStateStressProbeSource],
    ["native-port-state-incoming-probe.js.txt", incomingPortStateProbeSource],
    ["quiet-native-worker-probe.js.txt", quietNativeWorkerProbeSource],
    ["quiet-native-document-probe.js.txt", quietNativeDocumentProbeSource],
  ]
) {
  test(name, () => {
    const source = readFileSync(new URL(name, import.meta.url));
    deepStrictEqual(source, Buffer.from(program, "utf8"));
    new Script(source.toString("utf8"), { filename: name });
  });
}

// HTTP fixture URLs keep their original .js names and exact measured bytes.
for (
  const [name, expected] of [
    [
      "fixtures/content-worlds/observe-detail.js.txt",
      "6ee91b13ec87ed24f212973a02ed5af9d2ec43c48206d2e8b6fd0da3dcf0a0cd",
    ],
    [
      "fixtures/content-worlds/probe.js.txt",
      "e292bce14d70008d77cdea4138dea45acc848de63f4bd81a98d0c3c6366047bf",
    ],
    [
      "fixtures/content-worlds/publish.js.txt",
      "a72a27a6b46dd2eefca94e33a6db7f9b89d1883df060cb0fba1da0608e4b79fb",
    ],
    [
      "fixtures/custom-event-detail/observe-detail.js.txt",
      "4355ab6f02e75bfd73d9c672ce1e582bc181b9601ed2d5cca5c8b657a7ca85a3",
    ],
    [
      "fixtures/custom-event-detail/probe.js.txt",
      "49b055de21f916022538eff111cad412618c3dc14c7fc0dc1fff7722a322b6c6",
    ],
    [
      "fixtures/custom-event-detail/publish.js.txt",
      "a6e667aebce64772a3322860c489fb88c2aea98f2512656ceb480289d64e5aa4",
    ],
  ]
) {
  test(name, () => {
    const source = readFileSync(new URL(name, import.meta.url));
    strictEqual(createHash("sha256").update(source).digest("hex"), expected);
    new Script(source.toString("utf8"), { filename: name });
  });
}
