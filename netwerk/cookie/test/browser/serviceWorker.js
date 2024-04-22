self.addEventListener("install", function () {
  self.skipWaiting();
});

self.addEventListener("activate", function (event) {
  event.waitUntil(self.clients.claim());
});

self.addEventListener("message", function (event) {
  if (event.data.action === "fetch") {
    fetch(event.data.url)
      .then(response => response.text())
      .then(data => {
        self.clients.matchAll().then(clients => {
          clients.forEach(client => {
            client.postMessage({ content: data });
          });
        });
      });
  }
});
