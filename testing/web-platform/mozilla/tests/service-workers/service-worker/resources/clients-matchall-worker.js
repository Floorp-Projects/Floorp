self.onmessage = function(e) {
  var port = e.data.port;
  var options = e.data.options;

  self.clients.matchAll(options).then(function(clients) {
      var message = [];
      clients.forEach(function(client) {
          message.push([client.visibilityState,
                        client.focused,
                        client.url,
                        client.frameType]);
        });
      // Sort by url
      message.sort(function(a, b) { return a[2] > b[2] ? 1 : -1; });
      port.postMessage(message);
    });
};
