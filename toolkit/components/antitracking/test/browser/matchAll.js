self.addEventListener("message", async e => {
  let clients = await self.clients.matchAll({type: "window", includeUncontrolled: true});

  let hasWindow = false;
  for (let client of clients) {
    if (e.data == client.url) {
      hasWindow = true;
      break;
    }
  }

  e.source.postMessage(hasWindow);
});
