/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* eslint-env node */
"use strict";

const fs = require("fs");
const http = require("http");

const URL = "/secrets/v1/secret/project/perftest/gecko/level-";
const SECRET = "/perftest-login";
const DEFAULT_SERVER = "https://firefox-ci-tc.services.mozilla.com";

const SCM_1_LOGIN_SITES = ["facebook", "netflix"];

/**
 * This function obtains the perftest secret from Taskcluster.
 *
 * It will NOT work locally. Please see the get_logins function, you
 * will need to define a JSON file and set the RAPTOR_LOGINS
 * env variable to its path.
 */
async function get_tc_secrets(context) {
  const MOZ_AUTOMATION = process.env.MOZ_AUTOMATION;
  if (!MOZ_AUTOMATION) {
    throw Error(
      "Not running in CI. Set RAPTOR_LOGINS to a JSON file containing the logins."
    );
  }

  let TASKCLUSTER_PROXY_URL = process.env.TASKCLUSTER_PROXY_URL
    ? process.env.TASKCLUSTER_PROXY_URL
    : DEFAULT_SERVER;

  let MOZ_SCM_LEVEL = process.env.MOZ_SCM_LEVEL ? process.env.MOZ_SCM_LEVEL : 1;

  const url = TASKCLUSTER_PROXY_URL + URL + MOZ_SCM_LEVEL + SECRET;

  const data = await new Promise((resolve, reject) => {
    context.log.info("Obtaining secrets for login...");

    http.get(
      url,
      {
        headers: {
          "Content-Type": "application/json",
          Accept: "application/json",
        },
      },
      res => {
        let data = "";
        context.log.info(`Secret status code: ${res.statusCode}`);

        res.on("data", d => {
          data += d.toString();
        });

        res.on("end", () => {
          resolve(data);
        });

        res.on("error", error => {
          context.log.error(error);
          reject(error);
        });
      }
    );
  });

  return JSON.parse(data);
}

/**
 * This function gets the login information required.
 *
 * It starts by looking for a local file whose path is defined
 * within RAPTOR_LOGINS. If we don't find this file, then we'll
 * attempt to get the login information from our Taskcluster secret.
 * If MOZ_AUTOMATION is undefined, then the test will fail, Taskcluster
 * secrets can only be obtained in CI.
 */
async function get_logins(context) {
  let logins;

  let RAPTOR_LOGINS = process.env.RAPTOR_LOGINS;
  if (RAPTOR_LOGINS) {
    // Get logins from a local file
    if (!RAPTOR_LOGINS.endsWith(".json")) {
      throw Error(
        `File given for logins does not end in '.json': ${RAPTOR_LOGINS}`
      );
    }

    let logins_file = null;
    try {
      logins_file = await fs.readFileSync(RAPTOR_LOGINS, "utf8");
    } catch (err) {
      throw Error(`Failed to read the file ${RAPTOR_LOGINS}: ${err}`);
    }

    logins = await JSON.parse(logins_file);
  } else {
    // Get logins from a perftest Taskcluster secret
    logins = await get_tc_secrets(context);
  }

  return logins;
}

/**
 * This function returns the type of login to do.
 *
 * This function returns "single-form" when we find a single form. If we only
 * find a single input field, we assume that there is one page per input
 * and return "multi-page". Otherwise, we return null.
 */
async function get_login_type(context, commands) {
  /*
    Determine if there's a password field visible with this
    query selector. Some sites use `tabIndex` to hide the password
    field behind other elements. In this case, we are searching
    for any password-type field that has a tabIndex of 0 or undefined and
    is not hidden.
  */
  let input_length = await commands.js.run(`
    return document.querySelectorAll(
      "input[type=password][tabIndex='0']:not([type=hidden])," +
      "input[type=password]:not([tabIndex]):not([type=hidden])"
    ).length;
  `);
  if (input_length == 0) {
    context.log.info("Found a multi-page login");
    return multi_page_login;
  } else if (input_length == 1) {
    context.log.info("Found a single-page login");
    return single_page_login;
  }

  if (
    (await commands.js.run(
      `return document.querySelectorAll("form").length;`
    )) >= 1
  ) {
    context.log.info("Found a single-form login");
    return single_form_login;
  }

  return null;
}

/**
 * This function sets up the login for a single form.
 *
 * The username field is defined as the field which immediately precedes
 * the password field. We have to do this in two steps because we need
 * to make sure that the event we emit from the change has the `isTrusted`
 * field set to `true`. Otherwise, some websites will ignore the input and
 * the form submission.
 */
async function single_page_login(login_info, context, commands, prefix = "") {
  // Get the first input field in the form that is not hidden and add the
  // username. Assumes that email/username is always the first input field.
  await commands.addText.bySelector(
    login_info.username,
    `${prefix}input:not([type=hidden]):not([type=password])`
  );

  // Get the password field and ensure it's not hidden.
  await commands.addText.bySelector(
    login_info.password,
    `${prefix}input[type=password]:not([type=hidden])`
  );

  return undefined;
}

/**
 * See single_page_login.
 */
async function single_form_login(login_info, context, commands) {
  return single_page_login(login_info, context, commands, "form ");
}

/**
 * Login to a website that uses multiple pages for the login.
 *
 * WARNING: Assumes that the first page is for the username.
 */
async function multi_page_login(login_info, context, commands) {
  const driver = context.selenium.driver;
  const webdriver = context.selenium.webdriver;

  const username_field = await driver.findElement(
    webdriver.By.css(`input:not([type=hidden]):not([type=password])`)
  );
  await username_field.sendKeys(login_info.username);
  await username_field.sendKeys(webdriver.Key.ENTER);
  await commands.wait.byTime(5000);

  let password_field;
  try {
    password_field = await driver.findElement(
      webdriver.By.css(`input[type=password]:not([type=hidden])`)
    );
  } catch (err) {
    if (err.toString().includes("NoSuchElementError")) {
      // Sometimes we're suspicious (i.e. they think we're a bot/bad-actor)
      let name_field = await driver.findElement(
        webdriver.By.css(`input:not([type=hidden]):not([type=password])`)
      );
      await name_field.sendKeys(login_info.suspicious_answer);
      await name_field.sendKeys(webdriver.Key.ENTER);
      await commands.wait.byTime(5000);

      // Try getting the password field again
      password_field = await driver.findElement(
        webdriver.By.css(`input[type=password]:not([type=hidden])`)
      );
    } else {
      throw err;
    }
  }

  await password_field.sendKeys(login_info.password);

  return async function() {
    password_field.sendKeys(webdriver.Key.ENTER);
    await commands.wait.byTime(5000);
  };
}

/**
 * This function sets up the login.
 *
 * This is done by first the login type, and then performing the
 * actual login setup. The return is a possible button to click
 * to perform the login.
 */
async function setup_login(login_info, context, commands) {
  let login_func = await get_login_type(context, commands);
  if (!login_func) {
    throw Error("Could not determine the type of login page.");
  }

  try {
    return await login_func(login_info, context, commands);
  } catch (err) {
    throw Error(`Could not setup login information: ${err}`);
  }
}

/**
 * This function performs the login.
 *
 * It does this by either clicking on a button with a type
 * of "sumbit", or running a final_button function that was
 * obtained from the setup_login function. Some pages also ask
 * questions about setting up 2FA or other information. Generally,
 * these contain the "skip" text.
 */
async function login(context, commands, final_button) {
  try {
    if (!final_button) {
      // The mouse double click emits an event with `evt.isTrusted=true`
      await commands.mouse.doubleClick.bySelector("button[type=submit]");
      await commands.wait.byTime(10000);
    } else {
      // In some cases, it's preferable to be given a function for the final button
      await final_button();
    }

    // Some pages ask to setup 2FA, skip this based on the text
    const XPATHS = [
      "//a[contains(text(), 'skip')]",
      "//button[contains(text(), 'skip')]",
      "//input[contains(text(), 'skip')]",
      "//div[contains(text(), 'skip')]",
    ];

    for (let xpath of XPATHS) {
      try {
        await commands.mouse.doubleClick.byXpath(xpath);
      } catch (err) {
        if (err.toString().includes("not double click")) {
          context.log.info(`Can't find a button with the text: ${xpath}`);
        } else {
          throw err;
        }
      }
    }
  } catch (err) {
    throw Error(
      `Could not login to website as we could not find the submit button/input: ${err}`
    );
  }
}

/**
 * Grab the base URL from the browsertime url.
 *
 * This is a necessary step for getting the login values from the Taskcluster
 * secrets, which are hashed by the base URL.
 *
 * The first entry is the protocal, third is the top-level domain (or host)
 */
function get_base_URL(fullUrl) {
  let pathAsArray = fullUrl.split("/");
  return pathAsArray[0] + "//" + pathAsArray[2];
}

/**
 * This function attempts the login-login sequence for a live pageload recording
 */
async function perform_live_login(context, commands) {
  let testUrl = context.options.browsertime.url;

  let logins = await get_logins(context);
  const baseUrl = get_base_URL(testUrl);

  await commands.navigate("about:blank");

  let login_info = logins.secret[baseUrl];
  try {
    await commands.navigate(login_info.login_url);
  } catch (err) {
    context.log.info("Unable to acquire login information");
    throw err;
  }
  await commands.wait.byTime(5000);

  let final_button = await setup_login(login_info, context, commands);
  await login(context, commands, final_button);
}

async function pageload_test(context, commands) {
  let testUrl = context.options.browsertime.url;
  let secondaryUrl = context.options.browsertime.secondary_url;
  let testName = context.options.browsertime.testName;
  let input_cmds = context.options.browsertime.commands || "";

  // If the user has RAPTOR_LOGINS configured correctly, a local login pageload
  // test can be attempted. Otherwise if attempting it in CI, only sites with the
  // associated MOZ_SCM_LEVEL will be attempted (e.g. Try = 1, autoland = 3)
  if (context.options.browsertime.login) {
    if (
      process.env.RAPTOR_LOGINS ||
      process.env.MOZ_SCM_LEVEL == 3 ||
      SCM_1_LOGIN_SITES.includes(testName)
    ) {
      try {
        await perform_live_login(context, commands);
      } catch (err) {
        context.log.info(
          "Unable to login. Acquiring a recording without logging in"
        );
        context.log.info("Error:" + err);
      }
    }
  }

  // Wait for browser to settle
  await commands.wait.byTime(1000);

  await commands.measure.start(testUrl);
  if (input_cmds) {
    context.log.info("Searching for cookie prompt elements...");
    let cmds = input_cmds.split(";;;");
    for (let cmdstr of cmds) {
      let [cmd, ...args] = cmdstr.split(":::");
      context.log.info(cmd, args);
      let result = await commands.js.run(
        `return document.evaluate("` +
          args +
          `", document, null, XPathResult.FIRST_ORDERED_NODE_TYPE, null).singleNodeValue;`
      );
      if (result) {
        context.log.info("Element found, clicking on it.");
        await run_command(cmdstr, context, commands);
      } else {
        context.log.info(
          "Element not found! The cookie prompt may have not appeared, please check the screenshots."
        );
      }
    }
  }
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

/**
 * Converts a string such as `measure.start` into the
 * actual function that is found in the `commands` module.
 *
 * XX: Find a way to share this function between
 * perftest_record.js and browsertime_interactive.js
 */
async function get_command_function(cmd, commands) {
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

/**
 * Performs an interactive test.
 *
 * These tests are interactive as the entire test is defined
 * through a set of browsertime commands. This allows users
 * to build arbitrary tests. Furthermore, interactive tests
 * provide the ability to login to websites.
 */
async function interactive_test(input_cmds, context, commands) {
  let cmds = input_cmds.split(";;;");

  let logins;
  if (context.options.browsertime.login) {
    logins = await get_logins(context);
  }

  await commands.navigate("about:blank");

  let user_setup = false;
  let final_button = null;
  for (let cmdstr of cmds) {
    let [cmd, ...args] = cmdstr.split(":::");

    if (cmd == "setup_login") {
      if (!logins) {
        throw Error(
          "This test is not specified as a `login` test so no login information is available."
        );
      }
      if (args.length < 1 || args[0] == "") {
        throw Error(
          `No URL given, can't tell where to setup the login. We only accept: ${logins.keys()}`
        );
      }
      /* Structure for logins is:
          {
              "username": ...,
              "password": ...,
              "suspicious_answer": ...,
              "login_url": ...,
          }
      */
      let login_info = logins.secret[args[0]];

      await commands.navigate(login_info.login_url);
      await commands.wait.byTime(5000);

      final_button = await setup_login(login_info, context, commands);
      user_setup = true;
    } else if (cmd == "login") {
      if (!user_setup) {
        throw Error("setup_login needs to be called before the login command");
      }
      await login(context, commands, final_button);
    } else {
      await run_command(cmdstr, context, commands);
    }
  }
}

async function run_command(cmdstr, context, commands) {
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

async function test(context, commands) {
  let input_cmds = context.options.browsertime.commands;
  let test_type = context.options.browsertime.testType;
  if (test_type == "interactive") {
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
