function fib(n) {
  if (n < 2) {
    return 1;
  }
  return fib(n - 1) + fib(n - 2);
}

onmessage = function (e) {
  self.postMessage(fib(Number(e.data)));
};
