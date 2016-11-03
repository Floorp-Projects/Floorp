"use strict";

onmessage = function(event) {
  fetch("https://example.com/example.txt").then(() => {
    postMessage("Done!");
  });
};

