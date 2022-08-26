/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test checks that we properly handle exceptions occuring when a
// network mapped drive is disconnected while we're reading from mmaped
// file on that drive.
// Because sharing folders requires network priviledges, this test cannot
// be completely automated. It will create the necessary files and wait
// while you run a `net share sharedfolder=...path...` in a CMD terminal
// with Administator priviledges.
// See bug 1551562 and bug 1707853

/* import-globals-from ../../zipwriter/test/unit/head_zipwriter.js */

function wrapInputStream(input) {
  let wrapper = Cc["@mozilla.org/scriptableinputstream;1"].createInstance(
    Ci.nsIScriptableInputStream
  );
  wrapper.init(input);
  return wrapper;
}

const { Subprocess } = ChromeUtils.import(
  "resource://gre/modules/Subprocess.jsm"
);

const XPI_NAME = "testjar.xpi";
const SHARE_NAME = "sharedfolder";

async function net(args, silent = false) {
  if (!silent) {
    info(`Executing "net ${args.join(" ")}"`);
  }
  let proc = await Subprocess.call({
    command: "C:\\Windows\\System32\\net.exe",
    arguments: args,
    environmentAppend: true,
    stderr: "stdout",
  });
  let { exitCode } = await proc.wait();
  let stdout = await proc.stdout.readString();
  if (!silent) {
    info(`stdout: ${stdout}`);
    equal(exitCode, 0);
  }

  // Introduce a small delay so we're sure the command effects are visible
  await new Promise(resolve => do_timeout(500, resolve));
  return stdout;
}

const time = 1199145600000; // Jan 1st 2008

add_task(async function test() {
  let tmpFile = do_get_profile().clone();
  tmpFile.append("sharedfolder");
  tmpFile.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  let tmpDir = tmpFile.clone();
  tmpFile.append(XPI_NAME);

  let DATA = "a".repeat(1024 * 1024 * 10); // 10Mb

  zipW.open(tmpFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);
  let stream = Cc["@mozilla.org/io/string-input-stream;1"].createInstance(
    Ci.nsIStringInputStream
  );
  stream.setData(DATA, DATA.length);
  zipW.addEntryStream(
    "data",
    time * PR_USEC_PER_MSEC,
    Ci.nsIZipWriter.COMPRESSION_NONE,
    stream,
    false
  );
  zipW.close();

  // find the computer name
  let lines = await net(["config", "workstation"], true);
  let COMPUTER_NAME;
  for (let l of lines.split("\n")) {
    if (l.startsWith("Computer name")) {
      COMPUTER_NAME = l.split("\\\\")[1].trim();
    }
  }
  ok(COMPUTER_NAME);

  dump(
    `\n\n-------\nNow in a CMD with Administrator priviledges please run:\nnet share ${SHARE_NAME}=${tmpDir.path.replaceAll(
      "\\\\",
      "\\"
    )} /grant:everyone,READ\n\n-------\n`
  );

  info("waiting while you run command");
  while (true) {
    let output = await net(["share"], true);
    if (output.includes(SHARE_NAME)) {
      break;
    }
  }

  // Map the network share to a X: drive
  await net(["use", "x:", `\\\\${COMPUTER_NAME}\\${SHARE_NAME}`]);

  // the build script have created the zip we can test on in the current dir.
  let file = Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);
  file.initWithPath(`X:\\${XPI_NAME}`);
  info(file.path);
  ok(file.exists());

  let zipreader = Cc["@mozilla.org/libjar/zip-reader;1"].createInstance(
    Ci.nsIZipReader
  );
  zipreader.open(file);
  stream = wrapInputStream(zipreader.getInputStream("data"));

  // Delete the X: drive
  await net(["use", "x:", "/delete", "/y"]);

  // Checks that we get the expected failure
  Assert.throws(
    () => {
      while (true) {
        Assert.ok(!!stream.read(1024).length);
      }
    },
    /NS_ERROR_FAILURE/,
    "Got fault handler exception"
  );

  // This part is optional, but it's good to clean up.
  dump(
    `\n\n-------\nNow in a CMD with Administrator priviledges please run:\nnet share ${SHARE_NAME} /delete\n\n-------\n`
  );
});
