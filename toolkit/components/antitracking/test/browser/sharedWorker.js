let ports = 0;
self.onconnect = e => {
  ++ports;
  e.ports[0].onmessage = event => {
    if (event.data === "count") {
      e.ports[0].postMessage(ports);
      return;
    }

    if (event.data === "close") {
      self.close();
      return;
    }

    // Error.
    e.ports[0].postMessage(-1);
  };
};
