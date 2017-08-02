/* eslint-env worker */

onmessage = function() {
  try {
    importScripts("evilWorker.js");
  } catch (ex) {
    postMessage("success");
    return;
  }

  postMessage("failure");
};
