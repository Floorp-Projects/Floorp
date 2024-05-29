onmessage = function () {
  try {
    importScripts("evil.worker.js");
  } catch (ex) {
    postMessage("success");
    return;
  }

  postMessage("failure");
};
