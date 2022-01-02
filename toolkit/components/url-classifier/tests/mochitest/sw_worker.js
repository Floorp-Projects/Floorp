self.addEventListener("fetch", function(event) {
  let sep = "synth.html?";
  let idx = event.request.url.indexOf(sep);
  if (idx > 0) {
    let url = event.request.url.substring(idx + sep.length);
    event.respondWith(fetch(url, { credentials: "include" }));
  } else {
    event.respondWith(fetch(event.request));
  }
});
