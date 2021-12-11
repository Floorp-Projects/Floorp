window.addEventListener("message", function onMessage(evt) {
  if (evt.data.type === "doXHR") {
    var request = new XMLHttpRequest();
    request.open("GET", evt.data.url, true);
    request.send(null);
  } else if (evt.data.type === "doFetch") {
    fetch(evt.data.url);
  }
});
