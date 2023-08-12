"use strict";

self.onmessage = async message => {
  await fetch(message.data);
  self.close();
};

self.postMessage("loaded");
