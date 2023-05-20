var big_array = [];
var n = 0;

onmessage = function (e) {
  var sum = 0;
  if (n == 0) {
    for (let i = 0; i < 4 * 1024 * 1024; i++) {
      big_array[i] = i * i;
    }
  } else {
    for (let i = 0; i < 4 * 1024 * 1024; i++) {
      sum += big_array[i];
      big_array[i] += 1;
    }
  }
  self.postMessage(`Iter: ${n}, sum: ${sum}`);
  n++;
};
