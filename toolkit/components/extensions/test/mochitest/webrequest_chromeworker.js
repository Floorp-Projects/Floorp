"use strict";

/* eslint-env worker */

onmessage = function(event) {
  fetch("https://example.com/example.txt").then(() => {
    postMessage("Done!");
  });
};
