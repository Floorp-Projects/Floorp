self.addEventListener("install", function() {
  self.skipWaiting();
});

self.addEventListener("activate", function(e) {
  e.waitUntil(self.clients.claim());
});

self.addEventListener("push", async function(e) {
  const clients = await self.clients.matchAll();
  clients.forEach(function(client) {
    client.postMessage({ type: "push", payload: e.data.text() });
  });
});

self.addEventListener("pushsubscriptionchange", async function(e) {
  const clients = await self.clients.matchAll();
  clients.forEach(function(client) {
    client.postMessage({ type: "pushsubscriptionchange" });
  });
});
