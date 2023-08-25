"use strict";

self.onconnect = async evt => {
  const port = evt.ports[0];
  port.onmessage = async message => {
    await fetch(message.data);
    self.close();
  };
  port.start();
  port.postMessage("loaded");
};
