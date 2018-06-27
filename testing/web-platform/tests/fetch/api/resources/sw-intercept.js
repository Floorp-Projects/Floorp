async function broadcast(msg) {
  for (const client of await clients.matchAll()) {
    client.postMessage(msg);
  }
}

addEventListener('fetch', event => {
  if (event.request.url.endsWith('?signal')) {
    event.waitUntil(broadcast("ready"));
    event.respondWith(new Promise((resolve, reject) => {
      event.request.signal.onabort = () => {
        broadcast("aborted");
        reject();
      };
    }));
  } else {
    event.waitUntil(broadcast(event.request.url));
    event.respondWith(fetch(event.request));
  }
});
