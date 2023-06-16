navigator.serviceWorker.register("./service-worker.js", {
  scope: ".",
});

function showNotification() {
  Notification.requestPermission(function (result) {
    if (result === "granted") {
      navigator.serviceWorker.ready.then(function (registration) {
        registration.showNotification("Open Window Notification", {
          body: "Hello",
        });
      });
    }
  });
}
