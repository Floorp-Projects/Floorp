/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_resolveFile() {
  const EXISTING_FILE = do_get_file("xpcshell.toml");
  // We explicitly do not initialize this with a working dir.
  let cmdLine = Cu.createCommandLine(
    [],
    null,
    Ci.nsICommandLine.STATE_REMOTE_EXPLICIT
  );
  let fileByPath = cmdLine.resolveFile(EXISTING_FILE.path);
  info("Resolved: " + fileByPath.path);
  Assert.ok(EXISTING_FILE.equals(fileByPath), "Should find the same file");

  Assert.ok(
    !cmdLine.resolveFile("xpcshell.toml"),
    "Should get null for relative files."
  );

  // Now create a commandline with a working dir:
  cmdLine = Cu.createCommandLine(
    [],
    EXISTING_FILE.parent,
    Ci.nsICommandLine.STATE_REMOTE_EXPLICIT
  );
  let resolvedTxtFile = cmdLine.resolveFile("xpcshell.toml");

  info("Resolved: " + resolvedTxtFile.path);
  Assert.ok(
    EXISTING_FILE.equals(resolvedTxtFile),
    "Should resolve relative file."
  );
});
