/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { sendStandalonePing } from "resource://gre/modules/TelemetrySend.sys.mjs";

export async function runBackgroundTask(commandLine) {
  let sends = [];
  for (let i = 0; i < commandLine.length; i += 2) {
    sends.push(
      sendPing(commandLine.getArgument(i), commandLine.getArgument(i + 1))
    );
  }

  return Promise.all(sends);
}

// The standalone pingsender utility had an allowlist of endpoints, which was
// added to prevent it from being used as a generic exfiltration utility by
// unrelated malware running on the same system. It's unclear whether a gecko-
// based pingsender would be similarly desirable for that use-case, but it's
// easy enough to implement an allowlist here as well.
const ALLOWED_ENDPOINTS = ["localhost", "incoming.telemetry.mozilla.org"];

async function sendPing(endpoint, path) {
  console.log(`pingsender: sending ${path} to ${endpoint}`);

  let hostname = new URL(endpoint).hostname;
  if (!ALLOWED_ENDPOINTS.includes(hostname)) {
    throw new Error(`pingsender: Endpoint ${endpoint} is not allowed`);
  }

  let json = await IOUtils.readUTF8(path);
  await sendStandalonePing(endpoint, json, {
    "User-Agent": "pingsender/2.0",
    "X-PingSender-Version": "2.0",
  });

  return IOUtils.remove(path);
}
