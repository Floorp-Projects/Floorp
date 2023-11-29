/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_createCommandLine() {
  const EXISTING_FILE = do_get_file("xpcshell.toml");

  // Test `arguments`.
  let cmdLine = Cu.createCommandLine(
    [],
    null,
    Ci.nsICommandLine.STATE_REMOTE_EXPLICIT
  );
  Assert.equal(cmdLine.length, 0);
  Assert.throws(() => cmdLine.workingDirectory, /NS_ERROR_NOT_INITIALIZED/);
  Assert.equal(cmdLine.state, Ci.nsICommandLine.STATE_REMOTE_EXPLICIT);

  cmdLine = Cu.createCommandLine(
    ["test"],
    null,
    Ci.nsICommandLine.STATE_REMOTE_EXPLICIT
  );
  Assert.equal(cmdLine.length, 1);
  Assert.equal(cmdLine.getArgument(0), "test");
  Assert.throws(() => cmdLine.getArgument(1), /NS_ERROR_ILLEGAL_VALUE/);
  Assert.throws(() => cmdLine.workingDirectory, /NS_ERROR_NOT_INITIALIZED/);
  Assert.equal(cmdLine.state, Ci.nsICommandLine.STATE_REMOTE_EXPLICIT);

  cmdLine = Cu.createCommandLine(
    ["test1", "test2"],
    null,
    Ci.nsICommandLine.STATE_REMOTE_EXPLICIT
  );
  Assert.equal(cmdLine.length, 2);
  Assert.equal(cmdLine.getArgument(0), "test1");
  Assert.equal(cmdLine.getArgument(1), "test2");
  Assert.throws(() => cmdLine.getArgument(2), /NS_ERROR_ILLEGAL_VALUE/);
  Assert.throws(() => cmdLine.workingDirectory, /NS_ERROR_NOT_INITIALIZED/);
  Assert.equal(cmdLine.state, Ci.nsICommandLine.STATE_REMOTE_EXPLICIT);

  // Test `workingDirectory`.
  cmdLine = Cu.createCommandLine(
    [],
    EXISTING_FILE.parent,
    Ci.nsICommandLine.STATE_REMOTE_AUTO
  );
  Assert.equal(cmdLine.length, 0);
  Assert.equal(cmdLine.workingDirectory.path, EXISTING_FILE.parent.path);
  Assert.equal(cmdLine.state, Ci.nsICommandLine.STATE_REMOTE_AUTO);

  // Test `state`.
  cmdLine = Cu.createCommandLine([], null, Ci.nsICommandLine.STATE_REMOTE_AUTO);
  Assert.equal(cmdLine.state, Ci.nsICommandLine.STATE_REMOTE_AUTO);

  cmdLine = Cu.createCommandLine(
    [],
    EXISTING_FILE.parent,
    Ci.nsICommandLine.STATE_REMOTE_EXPLICIT
  );
  Assert.equal(cmdLine.state, Ci.nsICommandLine.STATE_REMOTE_EXPLICIT);
});
