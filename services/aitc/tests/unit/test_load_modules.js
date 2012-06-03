const modules = [
  "client.js",
  "storage.js"
];

function run_test() {
  for each (let m in modules) {
    Cu.import("resource://services-aitc/" + m, {});
  }
}
