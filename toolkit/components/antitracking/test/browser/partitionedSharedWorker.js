let value = "";
self.onconnect = e => {
  e.ports[0].onmessage = event => {
    if (event.data.what === "get") {
      e.ports[0].postMessage(value);
      return;
    }

    if (event.data.what === "put") {
      value = event.data.value;
      return;
    }

    // Error.
    e.ports[0].postMessage(-1);
  };
};
