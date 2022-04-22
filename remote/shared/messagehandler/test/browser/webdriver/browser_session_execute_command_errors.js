/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { WebDriverSession } = ChromeUtils.import(
  "chrome://remote/content/shared/webdriver/Session.jsm"
);

const { error } = ChromeUtils.import(
  "chrome://remote/content/shared/webdriver/Errors.jsm"
);

add_task(async function test_execute_missing_command_error() {
  const session = new WebDriverSession();

  info("Attempt to execute an unknown protocol command");
  await Assert.rejects(
    session.execute("command", "missingCommand"),
    err =>
      err.name == "UnknownCommandError" &&
      err.message == `command.missingCommand`
  );
});

add_task(async function test_execute_missing_internal_command_error() {
  const session = new WebDriverSession();

  info(
    "Attempt to execute a protocol command which relies on an unknown internal method"
  );
  await Assert.rejects(
    session.execute("command", "testMissingIntermediaryMethod"),
    err =>
      err.name == "UnsupportedCommandError" &&
      err.message ==
        `command.missingMethod not supported for destination ROOT` &&
      !error.isWebDriverError(err)
  );
});
