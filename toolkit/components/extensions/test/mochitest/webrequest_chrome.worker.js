"use strict";

onmessage = function () {
  fetch("https://example.com/example.txt").then(() => {
    postMessage("Done!");
  });
};
