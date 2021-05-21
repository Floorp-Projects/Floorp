// Most of this file has been stolen from dom/serviceworkers/test/utils.js.

function waitForState(worker, state) {
  return new Promise((resolve, reject) => {
    function onStateChange() {
      if (worker.state === state) {
        worker.removeEventListener("statechange", onStateChange);
        resolve();
      }
      if (worker.state === "redundant") {
        worker.removeEventListener("statechange", onStateChange);
        reject(new Error("The service worker failed to install."));
      }
    }

    // First add an event listener, so we won't miss any change that happens
    // before we check the current state.
    worker.addEventListener("statechange", onStateChange);

    // Now check if the worker is already in the desired state.
    onStateChange();
  });
}

async function registerServiceWorkerAndWait(serviceWorkerFile) {
  if (!serviceWorkerFile) {
    throw new Error(
      "No service worker filename has been specified. Please specify a valid filename."
    );
  }

  console.log(`...registering the serviceworker "${serviceWorkerFile}"`);
  const reg = await navigator.serviceWorker.register(`./${serviceWorkerFile}`, {
    scope: "./",
  });
  console.log("...waiting for activation");
  await waitForState(reg.installing, "activated");
  console.log("...activated!");
}
