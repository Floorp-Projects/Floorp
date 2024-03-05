"use strict";

/* eslint-env worker */

onmessage = function () {
  fetch("https://example.com/example.txt").then(() => {
    postMessage("Done!");
  });
};
