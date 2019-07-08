// Use some CPU.
var interval = window.setInterval(() => {
  // Compute an arbitrary value, print it out to make sure that the JS
  // engine doesn't discard all our computation.
  var date = Date.now();
  var array = [];
  var i = 0;
  while (Date.now() - date <= 100) {
    array[i % 2] = i++;
  }
}, 300);
