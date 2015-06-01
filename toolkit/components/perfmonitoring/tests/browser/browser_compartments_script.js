
var carryOn = true;

window.addEventListener("message", e => {
  console.log("frame content", "message", e);
  if ("title" in e.data) {
    document.title = e.data.title;
  }
  if ("stop" in e.data) {
    carryOn = false;
  }
});

// Use some CPU.
var interval = window.setInterval(() => {
  if (!carryOn) {
    window.clearInterval(interval);
    return;
  }

  // Compute an arbitrary value, print it out to make sure that the JS
  // engine doesn't discard all our computation.
  var date = Date.now();
  var array = [];
  var i = 0;
  while (Date.now() - date <= 100) {
    array[i%2] = i++;
  }
}, 300);
