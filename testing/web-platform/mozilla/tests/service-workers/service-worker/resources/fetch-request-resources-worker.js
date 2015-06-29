var requests = [];
var port = undefined;

self.onmessage = function(e) {
  var message = e.data;
  if ('port' in message) {
    port = message.port;
    port.postMessage({ready: true});
  }
};

self.addEventListener('fetch', function(event) {
    var url = event.request.url;
    if (url.indexOf('dummy?test') == -1) {
      return;
    }
    port.postMessage({
        url: url,
        context: event.request.context,
        context_clone: event.request.clone().context,
        context_new: (new Request(event.request)).context,
        mode: event.request.mode,
        credentials: event.request.credentials
      });
    event.respondWith(Promise.reject());
  });
