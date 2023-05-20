/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env node */

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

module.exports = async function (context, commands) {
  context.log.info("Starting an interactive browsertime test");
  let page_cycles = context.options.browsertime.page_cycles;
  let post_startup_delay = context.options.browsertime.post_startup_delay;
  let input_cmds = context.options.browsertime.commands;

  context.log.info(
    "Waiting for %d ms (post_startup_delay)",
    post_startup_delay
  );
  await commands.wait.byTime(post_startup_delay);

  // unpack commands from python
  let cmds = input_cmds.split(";;;");

  for (let count = 0; count < page_cycles; count++) {
    context.log.info("Navigating to about:blank w/nav, count: " + count);
    await commands.navigate("about:blank");

    let pages_visited = [];
    for (let cmdstr of cmds) {
      let [cmd, ...args] = cmdstr.split(":::");

      if (cmd == "measure.start") {
        if (args[0] != "") {
          pages_visited.push(args[0]);
        }
      }

      let [func, parent_mod] = await get_command_function(cmd, commands);

      try {
        await func.call(parent_mod, ...args);
      } catch (e) {
        context.log.info(
          `Exception found while running \`commands.${cmd}(${args})\`: `
        );
        context.log.info(e.stack);
      }
    }

    // Log the number of pages visited for results parsing
    context.log.info("[] metrics: pages_visited: " + pages_visited);
  }

  context.log.info("Browsertime pageload ended.");
  return true;
};
