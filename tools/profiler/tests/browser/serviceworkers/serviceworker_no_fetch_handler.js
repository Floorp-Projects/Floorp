self.addEventListener("install", () => {
  performance.mark("__serviceworker_event");
  console.log("[SW]:", "Install event");
});
