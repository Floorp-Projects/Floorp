self.addEventListener("install", event => {
  performance.mark("__serviceworker_event");
  console.log("[SW]:", "Install event");
});
