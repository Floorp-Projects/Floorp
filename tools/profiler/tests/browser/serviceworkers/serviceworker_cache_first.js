const files = ["serviceworker_page.html", "firefox-logo-nightly.svg"];
const cacheName = "v1";

self.addEventListener("install", event => {
  console.log("[SW]:", "Install event");

  event.waitUntil(cacheAssets());
});

async function cacheAssets() {
  const cache = await caches.open(cacheName);
  await cache.addAll(files);
}

self.addEventListener("fetch", event => {
  console.log("Handling fetch event for", event.request.url);
  event.respondWith(handleFetch(event.request));
});

async function handleFetch(request) {
  const cachedResponse = await caches.match(request);
  if (cachedResponse) {
    console.log("Found response in cache:", cachedResponse);

    return cachedResponse;
  }
  console.log("No response found in cache. About to fetch from network...");

  const networkResponse = await fetch(request);
  console.log("Response from network is:", networkResponse);
  return networkResponse;
}
