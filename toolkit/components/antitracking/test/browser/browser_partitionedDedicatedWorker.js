PartitionedStorageHelper.runTestInNormalAndPrivateMode(
  "DedicatedWorkers",
  async (win3rdParty, win1stParty) => {
    // Set one cookie to the first party context.
    await win1stParty.fetch("cookies.sjs?first").then(r => r.text());

    // Create a dedicated worker in the first-party context.
    let firstPartyWorker = new win1stParty.Worker("dedicatedWorker.js");

    // Verify that the cookie from the first-party worker.
    await new Promise(resolve => {
      firstPartyWorker.addEventListener("message", msg => {
        is(
          msg.data,
          "cookie:foopy=first",
          "Got the expected first-party cookie."
        );
        resolve();
      });

      firstPartyWorker.postMessage("getCookies");
    });

    // Set one cookie to the third-party context.
    await win3rdParty
      .fetch("cookies.sjs?3rd;Partitioned;Secure")
      .then(r => r.text());

    // Create a dedicated worker in the third-party context.
    let thirdPartyWorker = new win3rdParty.Worker("dedicatedWorker.js");

    // Verify that the third-party worker cannot access first-party cookies.
    await new Promise(resolve => {
      thirdPartyWorker.addEventListener("message", msg => {
        is(
          msg.data,
          "cookie:foopy=3rd",
          "Got the expected third-party cookie."
        );
        resolve();
      });

      thirdPartyWorker.postMessage("getCookies");
    });

    firstPartyWorker.terminate();
    thirdPartyWorker.terminate();
  },

  async _ => {
    await new Promise(resolve => {
      Services.clearData.deleteData(Ci.nsIClearDataService.CLEAR_ALL, () =>
        resolve()
      );
    });
  },
  []
);
