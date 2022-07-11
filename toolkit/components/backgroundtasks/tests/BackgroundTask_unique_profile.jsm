/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["runBackgroundTask"];

const { Subprocess } = ChromeUtils.import(
  "resource://gre/modules/Subprocess.jsm"
);

async function runBackgroundTask(commandLine) {
  let sentinel = commandLine.getArgument(0);
  let count =
    commandLine.length > 1
      ? Number.parseInt(commandLine.getArgument(1), 10)
      : 1;

  let main = await ChromeUtils.requestProcInfo();
  let info = [main.pid, Services.dirsvc.get("ProfD", Ci.nsIFile).path];

  // `dump` prints to the console without formatting.
  dump(`${count}: ${sentinel}${JSON.stringify(info)}${sentinel}\n`);

  // Maybe launch a child.
  if (count <= 1) {
    return 0;
  }

  let command = Services.dirsvc.get("XREExeF", Ci.nsIFile).path;
  let args = [
    "--backgroundtask",
    "unique_profile",
    sentinel,
    (count - 1).toString(),
  ];

  // We must assemble all of the string fragments from stdout.
  let stdoutChunks = [];
  let proc = await Subprocess.call({
    command,
    arguments: args,
    stderr: "stdout",
    // Don't inherit this task's profile path.
    environmentAppend: true,
    environment: { XRE_PROFILE_PATH: null },
  }).then(p => {
    p.stdin.close();
    const dumpPipe = async pipe => {
      let data = await pipe.readString();
      while (data) {
        data = await pipe.readString();
        stdoutChunks.push(data);
      }
    };
    dumpPipe(p.stdout);

    return p;
  });

  let { exitCode } = await proc.wait();

  let stdout = stdoutChunks.join("");
  for (let line of stdout.split(/\r\n|\r|\n/).slice(0, -1)) {
    dump(`${count}> ${line}\n`);
  }

  return exitCode;
}
