/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env node */
"use strict";

async function pageload_test(context, commands) {
  let testUrl = context.options.browsertime.url;
  let secondaryUrl = context.options.browsertime.secondary_url;
  let testName = context.options.browsertime.testName;

  // Wait for browser to settle
  await commands.wait.byTime(1000);

  await commands.measure.start(testUrl);
  commands.screenshot.take("test_url_" + testName);

  if (secondaryUrl !== null) {
    // Wait for browser to settle
    await commands.wait.byTime(1000);

    await commands.measure.start(secondaryUrl);
    commands.screenshot.take("secondary_url_" + testName);
  }

  // Wait for browser to settle
  await commands.wait.byTime(1000);
}

async function get_command_function(cmd, commands) {
  /*
  Converts a string such as `measure.start` into the actual
  function that is found in the `commands` module.

  XXX: Find a way to share this function between
  perftest_record.js and browsertime_interactive.js
  */
  if (cmd == "") {
    throw new Error("A blank command was given.");
  } else if (cmd.endsWith(".")) {
    throw new Error(
      "An extra `.` was found at the end of this command: " + cmd
    );
  }

  // `func` will hold the actual method that needs to be called,
  // and the `parent_mod` is the context required to run the `func`
  // method. Without that context, `this` becomes undefined in the browsertime
  // classes.
  let func = null;
  let parent_mod = null;
  for (let func_part of cmd.split(".")) {
    if (func_part == "") {
      throw new Error(
        "An empty function part was found in the command: " + cmd
      );
    }

    if (func === null) {
      parent_mod = commands;
      func = commands[func_part];
    } else if (func !== undefined) {
      parent_mod = func;
      func = func[func_part];
    } else {
      break;
    }
  }

  if (func == undefined) {
    throw new Error(
      "The given command could not be found as a function: " + cmd
    );
  }

  return [func, parent_mod];
}

async function interactive_test(input_cmds, context, commands) {
  let cmds = input_cmds.split(";;;");

  await commands.navigate("about:blank");

  for (let cmdstr of cmds) {
    let [cmd, ...args] = cmdstr.split(":::");
    let [func, parent_mod] = await get_command_function(cmd, commands);

    try {
      await func.call(parent_mod, ...args);
    } catch (e) {
      context.log.info(
        `Exception found while running \`commands.${cmd}(${args})\`: ` + e
      );
    }
  }
}

async function test(context, commands) {
  let input_cmds = context.options.browsertime.commands;
  if (input_cmds !== undefined) {
    await interactive_test(input_cmds, context, commands);
  } else {
    await pageload_test(context, commands);
  }
  return true;
}

module.exports = {
  test,
  owner: "Bebe fstrugariu@mozilla.com",
  name: "Mozproxy recording generator",
  component: "raptor",
  description: ` This test generates fresh MozProxy recordings. It iterates through a list of 
      websites provided in *_sites.json and for each one opens a browser and 
      records all the associated HTTP traffic`,
  usage:
    "mach perftest --proxy --hooks testing/raptor/recorder/hooks.py testing/raptor/recorder/perftest_record.js",
};
