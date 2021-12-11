try {
  var ns = await import('./async_test_module_circular_1.js');
} catch(ns) {
  throw Error("Fails as expected");
};
