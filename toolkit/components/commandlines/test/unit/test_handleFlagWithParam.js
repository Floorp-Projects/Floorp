/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

add_task(async function test_handleFlagWithParam() {
  let goodInputs = [
    ["-arg", "value"],
    ["--arg", "value"],
    ["-arg=value"],
    ["--arg=value"],
  ];
  let badInputs = [["-arg", "-value"]];
  // Accepted only on Windows.  Perhaps surprisingly, "/arg=value" is not accepted.
  let windowsInputs = [["/arg", "value"], ["/arg:value"]];

  if (AppConstants.platform == "win") {
    goodInputs.push(...windowsInputs);
  }

  for (let args of goodInputs) {
    let cmdLine = Cu.createCommandLine(
      args,
      null,
      Ci.nsICommandLine.STATE_REMOTE_EXPLICIT
    );
    Assert.equal(
      cmdLine.handleFlagWithParam("arg", false),
      "value",
      `${JSON.stringify(args)} yields 'value' for 'arg'`
    );
  }

  for (let args of badInputs) {
    let cmdLine = Cu.createCommandLine(
      args,
      null,
      Ci.nsICommandLine.STATE_REMOTE_EXPLICIT
    );
    Assert.throws(
      () => cmdLine.handleFlagWithParam("arg", false),
      /NS_ERROR_ILLEGAL_VALUE/,
      `${JSON.stringify(args)} throws for 'arg'`
    );
  }

  if (AppConstants.platform != "win") {
    // No special meaning on non-Windows platforms.
    for (let args of windowsInputs) {
      let cmdLine = Cu.createCommandLine(
        args,
        null,
        Ci.nsICommandLine.STATE_REMOTE_EXPLICIT
      );
      Assert.equal(
        cmdLine.handleFlagWithParam("arg", false),
        null,
        `${JSON.stringify(args)} yields null for 'arg'`
      );
    }
  }
});
