import nsPromise from './async_test_module.js';

console.log("hi");
let ns = await nsPromise;

if (ns.default !== 42) {
  throw new Error("FAIL");
}
if (ns.x !== "named") {
  throw new Error("FAIL");
}
if (ns.y !== 39) {
  throw new Error("FAIL");
}
