let waitForDebugLog = target =>
  new Promise(resolve => {
    Cc["@mozilla.org/appservices/logger;1"]
      .getService(Ci.mozIAppServicesLogger)
      .register(target, {
        maxLevel: Ci.mozIServicesLogSink.LEVEL_INFO,
        info: resolve,
      });
  });

let rustLog = (target, message) => {
  Cc["@mozilla.org/xpcom/debug;1"]
    .getService(Ci.nsIDebug2)
    .rustLog(target, message);
};

add_task(async () => {
  let target = "app-services:webext_storage:sync";
  let expectedMessage = "info error: uh oh";
  let promiseMessage = waitForDebugLog(target);

  rustLog(target, expectedMessage);

  let actualMessage = await promiseMessage;
  Assert.ok(actualMessage.includes(expectedMessage));
});
