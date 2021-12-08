/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function test_level() {
  for (const level of ["error", "info", "warning"]) {
    console.info(`Testing level ${level}`);

    const message = await logMessage({ level, message: "foo" });
    is(message.level, level, "Received expected log level");
    is(message.message, "foo", "Received expected log message");
  }

  // Clear the console to avoid side effects since there are several tasks in
  // this file.
  await clearConsole();
});

add_task(async function test_message() {
  const message = await logMessage({ message: "bar" });
  is(message.message, "bar", "Received expected log message");

  // Clear the console to avoid side effects since there are several tasks in
  // this file.
  await clearConsole();
});

async function logMessage(options = {}) {
  const { inContent = true } = options;

  const addScriptErrorInternal = async ({ options }) => {
    const {
      level = "error",
      innerWindowId = content.windowGlobalChild.innerWindowId,
    } = options;

    const { ConsoleListener } = ChromeUtils.import(
      "chrome://remote/content/shared/listeners/ConsoleListener.jsm"
    );

    const consoleListener = new ConsoleListener(innerWindowId);
    let listener;

    return new Promise(resolve => {
      listener = (eventName, data) => {
        is(eventName, level, "Expected event has been fired");

        const { level: currentLevel, message, timeStamp } = data;
        is(typeof currentLevel, "string", "level is of type string");
        is(typeof message, "string", "message is of type string");
        is(typeof timeStamp, "number", "timeStamp is of type number");

        resolve(data);
      };

      consoleListener.on(level, listener);
      consoleListener.startListening();

      const levelToFlags = {
        error: Ci.nsIScriptError.errorFlag,
        info: Ci.nsIScriptError.infoFlag,
        warning: Ci.nsIScriptError.warningFlag,
      };

      const scriptError = Cc["@mozilla.org/scripterror;1"].createInstance(
        Ci.nsIScriptError
      );
      scriptError.initWithWindowID(
        options.message,
        options.sourceName || "sourceName",
        null,
        options.lineNumber || 0,
        options.columnNumber || 0,
        levelToFlags[level],
        options.category || "javascript",
        innerWindowId
      );

      Services.console.logMessage(scriptError);
    }).finally(() => {
      consoleListener.off(level, listener);
      consoleListener.destroy();
    });
  };

  if (inContent) {
    return ContentTask.spawn(
      gBrowser.selectedBrowser,
      { options },
      addScriptErrorInternal
    );
  }

  options.innerWindowId = window.windowGlobalChild.innerWindowId;
  return addScriptErrorInternal({ options });
}
