let value = "";
let fetch_url = "";

self.onfetch = function (e) {
  fetch_url = e.request.url;
};

// Call clients.claim() to enable fetch event.
self.addEventListener("activate", e => {
  e.waitUntil(self.clients.claim());
});

self.addEventListener("message", async e => {
  let res = {};

  switch (e.data.type) {
    case "GetHWConcurrency":
      res.result = "OK";
      res.value = navigator.hardwareConcurrency;
      break;

    case "GetScriptValue":
      res.result = "OK";
      res.value = value;
      break;

    case "SetScriptValue":
      res.result = "OK";
      value = e.data.value;
      break;

    case "HasCache":
      // Return if the cache storage exists or not.
      try {
        res.value = await caches.has(e.data.value);
        res.result = "OK";
      } catch (e) {
        res.result = "ERROR";
      }
      break;

    case "SetCache":
      // Open a cache storage with the given name.
      try {
        let cache = await caches.open(e.data.value);
        await cache.add("empty.js");
        res.result = "OK";
      } catch (e) {
        res.result = "ERROR";
      }
      break;

    case "SetIndexedDB":
      await new Promise(resolve => {
        let idxDB = indexedDB.open("test", 1);

        idxDB.onupgradeneeded = evt => {
          let db = evt.target.result;
          db.createObjectStore("foobar", { keyPath: "id" });
        };

        idxDB.onsuccess = evt => {
          let db = evt.target.result;
          db
            .transaction("foobar", "readwrite")
            .objectStore("foobar")
            .put({ id: 1, value: e.data.value }).onsuccess = _ => {
            resolve();
          };
        };
      });
      res.result = "OK";
      break;

    case "GetIndexedDB":
      res.value = await new Promise(resolve => {
        let idxDB = indexedDB.open("test", 1);

        idxDB.onsuccess = evt => {
          let db = evt.target.result;
          db.transaction("foobar").objectStore("foobar").get(1).onsuccess =
            ee => {
              resolve(
                ee.target.result === undefined ? "" : ee.target.result.value
              );
            };
        };
      });
      res.result = "OK";
      break;

    case "GetFetchURL":
      res.value = fetch_url;
      fetch_url = "";
      res.result = "OK";
      break;

    default:
      res.result = "ERROR";
  }

  e.source.postMessage(res);
});
