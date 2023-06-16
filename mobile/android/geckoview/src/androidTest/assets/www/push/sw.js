self.addEventListener("install", function () {
  self.skipWaiting();
});

self.addEventListener("activate", function (e) {
  e.waitUntil(self.clients.claim());
});

self.addEventListener("push", async function (e) {
  const clients = await self.clients.matchAll();
  let text = "";
  if (e.data) {
    text = e.data.text();
  }
  clients.forEach(function (client) {
    client.postMessage({ type: "push", payload: text });
  });

  try {
    const { title, body } = e.data.json();
    self.registration.showNotification(title, { body });
  } catch (e) {}
});

self.addEventListener("pushsubscriptionchange", async function (e) {
  const clients = await self.clients.matchAll();
  clients.forEach(function (client) {
    client.postMessage({ type: "pushsubscriptionchange" });
  });
});
