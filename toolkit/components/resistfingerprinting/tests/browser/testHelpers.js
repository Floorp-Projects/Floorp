async function registerServiceWorker(win, url) {
  let reg = await win.navigator.serviceWorker.register(url);
  if (reg.installing.state !== "activated") {
    await new Promise(resolve => {
      let w = reg.installing;
      w.addEventListener("statechange", function onStateChange() {
        if (w.state === "activated") {
          w.removeEventListener("statechange", onStateChange);
          resolve();
        }
      });
    });
  }

  return reg.active;
}

function sendAndWaitWorkerMessage(target, worker, message) {
  return new Promise(resolve => {
    worker.addEventListener(
      "message",
      msg => {
        resolve(msg.data);
      },
      { once: true }
    );

    target.postMessage(message);
  });
}
