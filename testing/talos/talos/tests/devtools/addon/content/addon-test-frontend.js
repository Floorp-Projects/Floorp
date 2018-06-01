// This file is the common bits for the test runner frontend, originally
// extracted out of the tart.html frontend when creating the damp test.

// Executes command at the chrome process.
// Limited to one argument (data), which is enough for TART.
// doneCallback will be called once done and, if applicable, with the result as argument.
// Execution might finish quickly (e.g. when setting prefs) or
// take a while (e.g. when triggering the test run)
function chromeExec(commandName, data, doneCallback) {
  // dispatch an event to the framescript which will take it from there.
  doneCallback = doneCallback || function dummy() {};
  dispatchEvent(
    new CustomEvent("damp@mozilla.org:chrome-exec-event", {
      bubbles: true,
      detail: {
        command: {
          name: commandName,
          data,
        },
        doneCallback
      }
    })
  );
}

function runTest(config, doneCallback) {
  chromeExec("runTest", config, doneCallback);
}

function init() {
  runTest();
}

addEventListener("load", init);
