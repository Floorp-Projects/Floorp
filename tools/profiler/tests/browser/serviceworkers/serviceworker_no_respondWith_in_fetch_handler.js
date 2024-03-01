self.addEventListener("install", () => {
  performance.mark("__serviceworker_event");
  console.log("[SW]:", "Install event");
});

self.addEventListener("fetch", event => {
  performance.mark("__serviceworker_event");
  console.log("Handling fetch event for", event.request.url);
});
