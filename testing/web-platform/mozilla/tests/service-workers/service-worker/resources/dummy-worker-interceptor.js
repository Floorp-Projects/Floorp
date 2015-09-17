var worker_text = 'postMessage("worker loading intercepted by service worker"); ';

self.onfetch = function(event) {
  if (event.request.url.indexOf('synthesized') != -1) {
    event.respondWith(new Response(worker_text));
  }
};

