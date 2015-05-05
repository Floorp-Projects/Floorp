
window.addEventListener("message", e => {
  console.log("frame content", "message", e);
  if ("title" in e.data) {
    document.title = e.data.title;
  }
});

// Use some CPU.
window.setInterval(() => {
  // Compute an arbitrary value, print it out to make sure that the JS
  // engine doesn't discard all our computation.
  var date = Date.now();
  var array = [];
  var i = 0;
  while (Date.now() - date <= 100) {
    array[i%2] = i++;
  }
  console.log("Arbitrary value", window.location, array);
}, 300);
