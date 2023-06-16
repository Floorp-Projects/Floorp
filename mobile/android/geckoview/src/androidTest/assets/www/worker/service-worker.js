self.addEventListener("install", function () {
  console.log("install");
  self.skipWaiting();
});

self.addEventListener("activate", function (e) {
  console.log("activate");
  e.waitUntil(self.clients.claim());
});

self.onnotificationclick = function (event) {
  console.log("onnotificationclick");
  self.clients.openWindow("open_window_target.html");
  event.notification.close();
};
